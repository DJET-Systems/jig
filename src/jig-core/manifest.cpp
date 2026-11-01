// SPDX-License-Identifier: GPL-3.0-only
#include "jig-core/manifest.hpp"

#include <toml.hpp>

#include <algorithm>
#include <regex>
#include <set>

#include "jig-core/sha256.hpp"

namespace jig {

namespace {

constexpr int kProject = exit_code::project;

std::string where(const fs::path& file, const toml::node* n) {
    std::string w = file.filename().string();
    if (n) w += ":" + std::to_string(n->source().begin.line);
    return w;
}

std::size_t edit_distance(std::string_view a, std::string_view b) {
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

struct Ctx {
    fs::path file;
};

Result<void> check_keys(const Ctx& c, const toml::table& t, const std::string& path,
                        const std::set<std::string_view>& allowed) {
    for (auto&& [k, v] : t) {
        std::string key(k.str());
        if (allowed.count(key)) continue;
        std::string best;
        std::size_t best_d = 3;
        for (auto a : allowed) {
            auto d = edit_distance(key, a);
            if (d < best_d) {
                best_d = d;
                best = std::string(a);
            }
        }
        std::string full = path.empty() ? key : path + "." + key;
        std::string help = best.empty() ? "See reference/manifest.md for the list of keys."
                                        : "Did you mean `" + (path.empty() ? best : path + "." + best) + "`?";
        return fail("DJ1003", kProject, "Unknown key `" + full + "` in jig.toml", help, {where(c.file, &v)});
    }
    return {};
}

unexpected<Error> type_error(const Ctx& c, const toml::node& n, const std::string& path, const char* expected) {
    return fail("DJ1004", kProject, "`" + path + "` must be " + expected, "See reference/manifest.md.",
                {where(c.file, &n)});
}

Result<void> get_string(const Ctx& c, const toml::table& t, std::string_view key, std::string& out,
                        const std::string& path) {
    auto* n = t.get(key);
    if (!n) return {};
    auto v = n->value<std::string>();
    if (!v) return type_error(c, *n, path + "." + std::string(key), "a string");
    out = *v;
    return {};
}

Result<void> get_opt_string(const Ctx& c, const toml::table& t, std::string_view key,
                            std::optional<std::string>& out, const std::string& path) {
    auto* n = t.get(key);
    if (!n) return {};
    // accept integers where the manifest documents a string (opt = 3)
    if (n->is_integer()) {
        out = std::to_string(n->as_integer()->get());
        return {};
    }
    if (n->is_boolean()) {
        out = n->as_boolean()->get() ? "true" : "false";
        return {};
    }
    auto v = n->value<std::string>();
    if (!v) return type_error(c, *n, path + "." + std::string(key), "a string");
    out = *v;
    return {};
}

Result<void> get_bool(const Ctx& c, const toml::table& t, std::string_view key, bool& out, const std::string& path) {
    auto* n = t.get(key);
    if (!n) return {};
    auto v = n->value<bool>();
    if (!v) return type_error(c, *n, path + "." + std::string(key), "true or false");
    out = *v;
    return {};
}

Result<void> get_strings(const Ctx& c, const toml::table& t, std::string_view key, std::vector<std::string>& out,
                         const std::string& path) {
    auto* n = t.get(key);
    if (!n) return {};
    auto* arr = n->as_array();
    if (!arr) return type_error(c, *n, path + "." + std::string(key), "an array of strings");
    std::vector<std::string> v;
    for (auto&& e : *arr) {
        auto s = e.value<std::string>();
        if (!s) return type_error(c, e, path + "." + std::string(key), "an array of strings");
        v.push_back(*s);
    }
    out = std::move(v);
    return {};
}

Result<const toml::table*> get_table(const Ctx& c, const toml::table& t, std::string_view key, const std::string& path) {
    auto* n = t.get(key);
    if (!n) return static_cast<const toml::table*>(nullptr);
    auto* tbl = n->as_table();
    if (!tbl) {
        return fail("DJ1004", kProject, "`" + (path.empty() ? std::string(key) : path + "." + std::string(key)) + "` must be a table",
                    "See reference/manifest.md.", {where(c.file, n)});
    }
    return tbl;
}

Result<void> check_enum(const Ctx& c, const toml::table& t, std::string_view key, const std::string& value,
                        const std::string& path, std::initializer_list<const char*> allowed) {
    for (auto* a : allowed)
        if (value == a) return {};
    std::string list;
    for (auto* a : allowed) list += (list.empty() ? "" : ", ") + std::string("\"") + a + "\"";
    return fail("DJ1004", kProject, "`" + path + "." + std::string(key) + "` has an unsupported value \"" + value + "\"",
                "Allowed values: " + list, {where(c.file, t.get(key))});
}

const std::vector<std::string> kCStds = {"c11", "c17", "c23"};
const std::vector<std::string> kCxxStds = {"c++14", "c++17", "c++20", "c++23", "c++26"};

}  // namespace

const std::vector<std::string>& standards_for(std::string_view language) {
    return language == "c" ? kCStds : kCxxStds;
}

bool std_valid_for_language(std::string_view language, std::string_view std) {
    const auto& v = standards_for(language);
    return std::find(v.begin(), v.end(), std) != v.end();
}

Result<fs::path> find_manifest(const fs::path& start) {
    std::error_code ec;
    auto dir = fs::absolute(start, ec);
    if (fs::is_regular_file(dir, ec)) return dir;
    for (;;) {
        auto candidate = dir / "jig.toml";
        if (fs::is_regular_file(candidate, ec)) return candidate;
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return fail("DJ1001", kProject, "No jig.toml found in " + fs::absolute(start).string() + " or any parent directory",
                "Run `jig init` to create a project, or pass `--manifest-path`.");
}

Result<Manifest> load_manifest(const fs::path& file_in) {
    Ctx c{fs::absolute(file_in)};
    JIG_TRY(text, read_file(c.file));

    toml::parse_result parsed = toml::parse(text, c.file.filename().string());
    if (!parsed) {
        const auto& err = parsed.error();
        return fail("DJ1002", kProject, "jig.toml is not valid TOML",
                    "Fix the syntax at the reported position.",
                    {c.file.filename().string() + ":" + std::to_string(err.source().begin.line) + ":" +
                         std::to_string(err.source().begin.column) + ": " + std::string(err.description())});
    }
    const toml::table& root = parsed.table();

    Manifest m;
    m.file = c.file;
    m.root = c.file.parent_path();
    m.digest = sha256_hex(text);
    m.toolchain_version = *VersionSpec::parse("latest");

    JIG_TRY_VOID(check_keys(c, root, "", {"project", "toolchain", "build", "profile", "test", "fmt", "env", "target",
                                          "dependencies", "dev-dependencies"}));

    if (root.contains("dependencies") || root.contains("dev-dependencies"))
        return fail("DJ1006", kProject, "[dependencies] is reserved for jig 0.2",
                    "Remove the section for now. Vendor your dependencies or use `build.backend = \"cmake\"`.",
                    {where(c.file, root.get("dependencies") ? root.get("dependencies") : root.get("dev-dependencies"))});
    if (root.contains("target"))
        return fail("DJ9001", exit_code::failure, "[[target]] (several targets in one project) is not implemented in this build",
                    "Use a single [project] target for now. See CODE-STATUS.md.", {where(c.file, root.get("target"))});

    // [project]
    JIG_TRY(project, get_table(c, root, "project", ""));
    if (!project) return fail("DJ1004", kProject, "jig.toml has no [project] table", "Run `jig init`, or see reference/manifest.md.");
    JIG_TRY_VOID(check_keys(c, *project, "project", {"name", "version", "description", "license", "authors", "kind", "language", "std", "extensions", "platforms"}));
    JIG_TRY_VOID(get_string(c, *project, "name", m.name, "project"));
    JIG_TRY_VOID(get_string(c, *project, "version", m.version, "project"));
    JIG_TRY_VOID(get_string(c, *project, "description", m.description, "project"));
    JIG_TRY_VOID(get_string(c, *project, "license", m.license, "project"));
    JIG_TRY_VOID(get_strings(c, *project, "authors", m.authors, "project"));
    JIG_TRY_VOID(get_string(c, *project, "kind", m.kind, "project"));
    JIG_TRY_VOID(get_string(c, *project, "language", m.language, "project"));
    JIG_TRY_VOID(get_string(c, *project, "std", m.std, "project"));
    JIG_TRY_VOID(get_bool(c, *project, "extensions", m.extensions, "project"));
    if (m.name.empty()) m.name = sanitized_dir_name(m.root);
    static const std::regex name_re("^[a-z][a-z0-9_-]{0,63}$");
    if (!std::regex_match(m.name, name_re))
        return fail("DJ1005", kProject, "Invalid project name \"" + m.name + "\"", "Use [a-z][a-z0-9_-]{0,63}.", {where(c.file, project->get("name"))});
    JIG_TRY_VOID(check_enum(c, *project, "kind", m.kind, "project", {"exe", "static-lib", "shared-lib", "header-only"}));
    if (m.language.empty()) return fail("DJ1004", kProject, "`project.language` is required", "Set language = \"c\" or \"c++\".", {where(c.file, project)});
    JIG_TRY_VOID(check_enum(c, *project, "language", m.language, "project", {"c", "c++"}));
    if (m.std.empty()) return fail("DJ1004", kProject, "`project.std` is required", "For example std = \"c++23\" or \"c17\".", {where(c.file, project)});
    if (!std_valid_for_language(m.language, m.std)) {
        std::string list;
        for (const auto& s : standards_for(m.language)) list += (list.empty() ? "" : ", ") + s;
        return fail("DJ1007", kProject, "std \"" + m.std + "\" is not valid for language \"" + m.language + "\"",
                    "Valid for " + m.language + ": " + list, {where(c.file, project->get("std"))});
    }

    // [toolchain]
    JIG_TRY(tc, get_table(c, root, "toolchain", ""));
    if (tc) {
        JIG_TRY_VOID(check_keys(c, *tc, "toolchain", {"vendor", "version", "hermeticity", "tools", "system"}));
        JIG_TRY_VOID(get_string(c, *tc, "vendor", m.vendor, "toolchain"));
        JIG_TRY_VOID(check_enum(c, *tc, "vendor", m.vendor, "toolchain", {"llvm", "system"}));
        std::string v = "latest";
        JIG_TRY_VOID(get_string(c, *tc, "version", v, "toolchain"));
        auto spec = VersionSpec::parse(v);
        if (!spec)
            return fail("DJ1008", kProject, "Invalid version specification \"" + v + "\"",
                        "Examples: \"20\", \"20.1.8\", \"^20\", \"~20.1\", \">=18,<21\", \"latest\".", {where(c.file, tc->get("version"))});
        m.toolchain_version = *spec;
        std::string h;
        JIG_TRY_VOID(get_string(c, *tc, "hermeticity", h, "toolchain"));
        if (!h.empty()) {
            JIG_TRY_VOID(check_enum(c, *tc, "hermeticity", h, "toolchain", {"h0", "h1", "h2", "h3"}));
            m.hermeticity = h;
        }
        JIG_TRY(tools, get_table(c, *tc, "tools", "toolchain"));
        if (tools) {
            JIG_TRY_VOID(check_keys(c, *tools, "toolchain.tools", {"cmake", "ninja", "clang-format"}));
            JIG_TRY_VOID(get_string(c, *tools, "cmake", m.tool_cmake, "toolchain.tools"));
            JIG_TRY_VOID(get_string(c, *tools, "ninja", m.tool_ninja, "toolchain.tools"));
        }
        JIG_TRY(sys, get_table(c, *tc, "system", "toolchain"));
        if (sys) {
            JIG_TRY_VOID(check_keys(c, *sys, "toolchain.system", {"cc", "cxx"}));
            JIG_TRY_VOID(get_string(c, *sys, "cc", m.sys_cc, "toolchain.system"));
            JIG_TRY_VOID(get_string(c, *sys, "cxx", m.sys_cxx, "toolchain.system"));
        }
    }

    // [build]
    JIG_TRY(build, get_table(c, root, "build", ""));
    if (build) {
        JIG_TRY_VOID(check_keys(c, *build, "build", {"backend", "sources", "include", "public-include", "defines", "warnings", "link", "cmake-args"}));
        JIG_TRY_VOID(get_string(c, *build, "backend", m.backend, "build"));
        JIG_TRY_VOID(check_enum(c, *build, "backend", m.backend, "build", {"generated", "cmake"}));
        JIG_TRY_VOID(get_strings(c, *build, "sources", m.sources, "build"));
        JIG_TRY_VOID(get_strings(c, *build, "include", m.include, "build"));
        JIG_TRY_VOID(get_strings(c, *build, "public-include", m.public_include, "build"));
        JIG_TRY_VOID(get_strings(c, *build, "defines", m.defines, "build"));
        JIG_TRY_VOID(get_strings(c, *build, "link", m.link, "build"));
        JIG_TRY_VOID(get_strings(c, *build, "cmake-args", m.cmake_args, "build"));
        JIG_TRY_VOID(get_string(c, *build, "warnings", m.warnings, "build"));
        JIG_TRY_VOID(check_enum(c, *build, "warnings", m.warnings, "build", {"off", "default", "strict"}));
    }

    // [profile.*]
    JIG_TRY(profiles, get_table(c, root, "profile", ""));
    if (profiles) {
        for (auto&& [pk, pv] : *profiles) {
            std::string pname(pk.str());
            auto* pt = pv.as_table();
            if (!pt) return type_error(c, pv, "profile." + pname, "a table");
            std::string path = "profile." + pname;
            JIG_TRY_VOID(check_keys(c, *pt, path, {"opt", "debug", "lto", "sanitizers", "assertions", "defines", "inherits"}));
            ProfileSpec ps;
            JIG_TRY_VOID(get_opt_string(c, *pt, "inherits", ps.inherits, path));
            JIG_TRY_VOID(get_opt_string(c, *pt, "opt", ps.opt, path));
            if (ps.opt) JIG_TRY_VOID(check_enum(c, *pt, "opt", *ps.opt, path, {"0", "1", "2", "3", "s", "z"}));
            if (auto* d = pt->get("debug")) {
                auto b = d->value<bool>();
                if (!b) return type_error(c, *d, path + ".debug", "true or false");
                ps.debug = *b;
            }
            JIG_TRY_VOID(get_opt_string(c, *pt, "lto", ps.lto, path));
            if (ps.lto) JIG_TRY_VOID(check_enum(c, *pt, "lto", *ps.lto, path, {"false", "thin", "full"}));
            if (pt->contains("sanitizers")) {
                std::vector<std::string> s;
                JIG_TRY_VOID(get_strings(c, *pt, "sanitizers", s, path));
                for (const auto& x : s)
                    if (x != "address" && x != "undefined" && x != "thread" && x != "memory")
                        return fail("DJ1004", kProject, "`" + path + ".sanitizers` has an unsupported value \"" + x + "\"",
                                    "Allowed values: address, undefined, thread, memory", {where(c.file, pt->get("sanitizers"))});
                ps.sanitizers = std::move(s);
            }
            if (auto* a = pt->get("assertions")) {
                auto b = a->value<bool>();
                if (!b) return type_error(c, *a, path + ".assertions", "true or false");
                ps.assertions = *b;
            }
            JIG_TRY_VOID(get_strings(c, *pt, "defines", ps.defines, path));
            m.profiles[pname] = std::move(ps);
        }
    }

    // [test]
    JIG_TRY(test, get_table(c, root, "test", ""));
    if (test) {
        JIG_TRY_VOID(check_keys(c, *test, "test", {"sources", "runner", "args"}));
        JIG_TRY_VOID(get_strings(c, *test, "sources", m.test_sources, "test"));
        JIG_TRY_VOID(get_strings(c, *test, "args", m.test_args, "test"));
    }

    // [fmt]
    JIG_TRY(fmt, get_table(c, root, "fmt", ""));
    if (fmt) {
        JIG_TRY_VOID(check_keys(c, *fmt, "fmt", {"style"}));
        JIG_TRY_VOID(get_string(c, *fmt, "style", m.fmt_style, "fmt"));
    }

    // [env]
    JIG_TRY(env, get_table(c, root, "env", ""));
    if (env) {
        JIG_TRY_VOID(check_keys(c, *env, "env", {"inherit", "set"}));
        JIG_TRY_VOID(get_string(c, *env, "inherit", m.env_inherit, "env"));
        JIG_TRY_VOID(check_enum(c, *env, "inherit", m.env_inherit, "env", {"none", "minimal", "all"}));
        JIG_TRY(set, get_table(c, *env, "set", "env"));
        if (set) {
            for (auto&& [k, v] : *set) {
                auto s = v.value<std::string>();
                if (!s) return type_error(c, v, "env.set." + std::string(k.str()), "a string");
                m.env_set[std::string(k.str())] = *s;
            }
        }
    }

    return m;
}

Result<ResolvedProfile> resolve_profile(const Manifest& m, const std::string& name) {
    // Built-ins (reference/manifest.md): debug and release exist even when not written.
    std::map<std::string, ProfileSpec> all = m.profiles;
    auto builtin = [&](const std::string& n) {
        ProfileSpec p;
        if (n == "debug") {
            p.opt = "0"; p.debug = true; p.lto = "false"; p.assertions = true;
        } else {
            p.opt = "3"; p.debug = false; p.lto = "thin"; p.assertions = false;
        }
        return p;
    };
    if (name != "debug" && name != "release" && !all.count(name))
        return fail("DJ4101", exit_code::usage, "Profile \"" + name + "\" not found",
                    "Profiles in this project: debug, release" + [&] {
                        std::string s;
                        for (auto& [k, _] : all)
                            if (k != "debug" && k != "release") s += ", " + k;
                        return s;
                    }());

    // Walk the inheritance chain (outermost first).
    std::vector<std::string> chain;
    std::string cur = name;
    for (int depth = 0; depth < 8; ++depth) {
        chain.push_back(cur);
        auto it = all.find(cur);
        std::string parent;
        if (it != all.end() && it->second.inherits) parent = *it->second.inherits;
        else if (cur != "debug" && cur != "release") parent = "debug";
        if (parent.empty()) break;
        if (parent != "debug" && parent != "release" && !all.count(parent))
            return fail("DJ4101", exit_code::usage, "Profile \"" + cur + "\" inherits from unknown profile \"" + parent + "\"");
        cur = parent;
    }
    ResolvedProfile r;
    r.name = name;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        ProfileSpec base = (*it == "debug" || *it == "release") ? builtin(*it) : ProfileSpec{};
        auto found = all.find(*it);
        auto apply = [&](const ProfileSpec& p) {
            if (p.opt) r.opt = *p.opt;
            if (p.debug) r.debug = *p.debug;
            if (p.lto) r.lto = *p.lto;
            if (p.sanitizers) r.sanitizers = *p.sanitizers;
            if (p.assertions) r.assertions = *p.assertions;
            for (const auto& d : p.defines) r.defines.push_back(d);
        };
        apply(base);
        if (found != all.end()) apply(found->second);
    }
    return r;
}

}  // namespace jig
