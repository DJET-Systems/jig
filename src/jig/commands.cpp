// SPDX-License-Identifier: GPL-3.0-only
#include "jig/commands.hpp"

#include <algorithm>
#include <cstdio>

#include "jig-build/build.hpp"
#include "jig-core/process.hpp"
#include "jig-core/tomledit.hpp"
#include "jig-env/project.hpp"

#ifndef JIG_VERSION
#define JIG_VERSION "0.1.0-dev"
#endif
#ifndef JIG_BUILD_COMPILER
#define JIG_BUILD_COMPILER "unknown"
#endif

namespace jig::cmd {

namespace {

int report(Ui& ui, const Error& e) {
    ui.fail(e);
    return e.exit;
}

// Environment commands should print their result and nothing else: prepare the project silently.
Settings quiet(const Settings& s) {
    Settings q = s;
    q.verbosity = std::min(s.verbosity, 1);
    if (s.verbosity >= 3) q.verbosity = s.verbosity;
    else q.verbosity = 0;
    return q;
}

std::string pad(const std::string& s, std::size_t w) { return s.size() >= w ? s + "  " : s + std::string(w - s.size(), ' '); }

std::string join(const std::vector<std::string>& v, const std::string& sep = " ") {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) s += (i ? sep : "") + v[i];
    return s;
}

// The locked compiler for the project in the current directory, if there is one (for a "*" in listings).
std::optional<std::string> locked_cxx(const Settings& s) {
    fs::path start = s.manifest_path ? *s.manifest_path : fs::current_path();
    auto f = find_manifest(start);
    if (!f) return std::nullopt;
    auto m = load_manifest(*f);
    if (!m || !fs::exists(lock_file(*m))) return std::nullopt;
    auto l = load_lock(lock_file(*m));
    if (!l) return std::nullopt;
    return l->toolchain.cxx_real;
}

}  // namespace

int not_implemented(Ui& ui, const std::string& what, const std::string& help) {
    return report(ui, Error{"DJ9001", what + " is not implemented in this build", {"This is a vertical slice of jig 0.1; see CODE-STATUS.md for what exists."}, help, exit_code::failure});
}

int init(const Settings& s, Ui& ui, const InitOptions& o) {
    auto r = run_init(s, ui, o);
    return r ? 0 : report(ui, r.error());
}

int toolchain_list(const Settings& s, Ui& ui) {
    auto detected = detect_system_compilers(ui);
    if (detected.empty()) {
        ui.print("No system compilers found on PATH.\n");
        return 0;
    }
    auto locked = locked_cxx(s);
    std::string out;
    out += ui.bold("  " + pad("", 0) + pad("FAMILY", 13) + pad("VERSION", 10) + pad("C++ DRIVER", 34) + "C++ STANDARDS") + "\n";
    for (const auto& c : detected) {
        bool mark = locked && *locked == c.cxx_real.string();
        out += std::string("  ") + (mark ? "*" : " ") + pad(c.family, 12) + pad(c.version.str(), 10) + pad(c.cxx.string(), 34) + join(accepted_stds(c, "c++")) + "\n";
        out += "    " + pad("", 22) + ui.dim(pad("", 0) + "C: " + join(accepted_stds(c, "c"))) + "\n";
    }
    out += "\n  " + ui.dim("* = locked for the project in this directory.   Provider: system (hermeticity H0). The llvm provider is not in this build.") + "\n";
    ui.print(out);
    return 0;
}

int toolchain_available(const Settings&, Ui& ui, const std::optional<std::string>& lang, const std::optional<std::string>& std) {
    std::string language = lang.value_or("c++");
    if (language == "cpp") language = "c++";
    if (language != "c" && language != "c++") return report(ui, Error{"DJ6003", "--lang must be c or c++", {}, {}, exit_code::usage});
    if (std && !std_valid_for_language(language, *std))
        return report(ui, Error{"DJ1007", "std \"" + *std + "\" is not valid for language \"" + language + "\"", {}, {}, exit_code::project});
    auto detected = detect_system_compilers(ui);
    std::string out = ui.bold("  " + pad("COMPILER", 20) + pad("USABLE", 8) + "WHY") + "\n";
    for (const auto& c : detected) {
        std::string why;
        bool ok = true;
        if (std) {
            auto flag = std_flag(c, language, *std, false);
            ok = flag.has_value();
            why = ok ? "accepts " + *std + " as -std=" + *flag : c.label() + " cannot compile " + *std;
        } else {
            why = "accepts: " + join(accepted_stds(c, language));
        }
        out += "  " + pad(c.label(), 20) + pad(ok ? "yes" : "no", 8) + why + "\n";
    }
    out += "\n  " + ui.dim("llvm provider (downloaded, pinned toolchains): not available in this build.") + "\n";
    ui.print(out);
    return 0;
}

int toolchain_which(const Settings& s, Ui& ui, const std::optional<std::string>& tool) {
    Ui qui(quiet(s));
    auto m = open_manifest(quiet(s), qui, false);
    if (!m) return report(ui, m.error());
    if (!fs::exists(lock_file(*m)))
        return report(ui, Error{"DJ1102", "jig.lock is missing", {}, "Run `jig lock`.", exit_code::project});
    auto l = load_lock(lock_file(*m));
    if (!l) return report(ui, l.error());
    std::vector<std::pair<std::string, std::string>> rows = {{"cc", l->toolchain.cc}, {"c++", l->toolchain.cxx}};
    for (const auto& t : l->tools) rows.emplace_back(t.name, t.path);
    if (tool) {
        for (const auto& [n, p] : rows)
            if (n == *tool || (*tool == "gcc" && n == "cc" && l->toolchain.family == "gcc") || (*tool == "g++" && n == "c++" && l->toolchain.family == "gcc") ||
                (*tool == "clang" && n == "cc" && l->toolchain.family != "gcc") || (*tool == "clang++" && n == "c++" && l->toolchain.family != "gcc")) {
                ui.print(p + "\n");
                return 0;
            }
        return report(ui, Error{"DJ3101", "No tool named \"" + *tool + "\" in the environment", {}, "Tools here: " + [&] {
                                    std::string x;
                                    for (auto& [n, _] : rows) x += (x.empty() ? "" : ", ") + n;
                                    return x;
                                }(), exit_code::toolchain});
    }
    std::string out;
    for (const auto& [n, p] : rows) out += pad(n, 14) + p + "\n";
    ui.print(out);
    return 0;
}

int toolchain_use(const Settings& s, Ui& ui, const std::string& spec_text) {
    auto spec = parse_toolchain_spec(spec_text);
    if (!spec || spec->family.empty())
        return report(ui, Error{"DJ6003", "Cannot read toolchain \"" + spec_text + "\"", {}, "Examples: gcc@13, clang@18, clang, gcc", exit_code::usage});
    ui.step("Reading project");
    auto m = open_manifest(s, ui);
    if (!m) return report(ui, m.error());
    auto original = read_file(m->file);
    if (!original) return report(ui, original.error());

    std::string text = *original;
    text = set_toml_key(text, "toolchain", "vendor", toml_quote("system"));
    text = set_toml_key(text, "toolchain", "version", toml_quote(spec->version.empty() ? "latest" : spec->version));
    text = set_toml_key(text, "toolchain.system", "cc", toml_quote(spec->family == "gcc" ? "gcc" : "clang"));
    text = set_toml_key(text, "toolchain.system", "cxx", toml_quote(spec->family == "gcc" ? "g++" : "clang++"));
    if (auto w = write_file_atomic(m->file, text); !w) return report(ui, w.error());
    ui.fact("jig.toml", "[toolchain] now asks for " + spec_text);

    // Re-resolve against the new request. If it cannot be satisfied, put jig.toml back.
    auto p = prepare(s, ui, true);
    if (!p) {
        (void)write_file_atomic(m->file, *original);
        ui.fact("jig.toml", "restored (the request could not be satisfied)");
        return report(ui, p.error());
    }
    ui.done("toolchain is now " + p->lock.toolchain.family + " " + p->lock.toolchain.version);
    return 0;
}

int toolchain_pin(const Settings& s, Ui& ui) {
    ui.step("Reading project");
    auto p = prepare(s, ui);
    if (!p) return report(ui, p.error());
    auto original = read_file(p->manifest.file);
    if (!original) return report(ui, original.error());
    auto text = set_toml_key(*original, "toolchain", "version", toml_quote(p->lock.toolchain.version));
    if (auto w = write_file_atomic(p->manifest.file, text); !w) return report(ui, w.error());
    ui.fact("jig.toml", "[toolchain] version = \"" + p->lock.toolchain.version + "\"");
    auto again = prepare(s, ui);  // the manifest changed, so jig.lock is re-written for it
    if (!again) return report(ui, again.error());
    ui.done("pinned " + again->lock.toolchain.family + " " + again->lock.toolchain.version);
    return 0;
}

int lock(const Settings& s, Ui& ui, bool update, bool check) {
    ui.step("Reading project");
    auto m = open_manifest(s, ui);
    if (!m) return report(ui, m.error());
    bool exists = fs::exists(lock_file(*m));
    std::optional<Lock> l;
    if (exists) {
        auto r = load_lock(lock_file(*m));
        if (!r) return report(ui, r.error());
        l = std::move(*r);
    }
    bool fresh = l && lock_is_fresh(*m, *l);
    if (check) {
        if (fresh) {
            ui.done("jig.lock is current");
            return 0;
        }
        return report(ui, Error{exists ? "DJ1103" : "DJ1102", exists ? "jig.lock is stale" : "jig.lock is missing", {}, "Run `jig lock`.", exit_code::project});
    }
    if (fresh && !update) {
        ui.done("jig.lock is already current (use --update to re-resolve)");
        return 0;
    }
    if (s.locked || s.frozen) return report(ui, Error{"DJ1103", "jig.lock needs updating and --locked/--frozen was given", {}, "Run `jig lock`.", exit_code::project});
    auto nl = write_new_lock(*m, ui);
    return nl ? 0 : report(ui, nl.error());
}

int sync(const Settings& s, Ui& ui, bool check) {
    if (check) {
        auto m = open_manifest(s, ui);
        if (!m) return report(ui, m.error());
        if (!fs::exists(lock_file(*m))) return report(ui, Error{"DJ1102", "jig.lock is missing", {}, "Run `jig lock`.", exit_code::project});
        if (env_in_sync(*m)) {
            ui.done(".jig/env is in sync with jig.lock");
            return 0;
        }
        return report(ui, Error{"DJ3103", "The environment is not in sync with jig.lock", {}, "Run `jig sync`.", exit_code::project});
    }
    auto p = prepare(s, ui);
    return p ? 0 : report(ui, p.error());
}

int env(const Settings& s, Ui& ui, const std::string& format) {
    if (format != "sh" && format != "fish" && format != "json")
        return report(ui, Error{"DJ6003", "--format must be sh, fish or json", {}, "pwsh and cmd formats are planned with Windows support.", exit_code::usage});
    Ui qui(quiet(s));
    auto p = prepare(quiet(s), qui);
    if (!p) return report(ui, p.error());
    ui.print(render_env(p->manifest, p->lock, format));
    return 0;
}

int shell(const Settings& s, Ui& ui) {
    Ui qui(quiet(s));
    auto p = prepare(quiet(s), qui);
    if (!p) return report(ui, p.error());
    return shell_in_env(*p, ui);
}

int exec(const Settings& s, Ui& ui, const std::vector<std::string>& argv) {
    if (argv.empty()) return report(ui, Error{"DJ6003", "Nothing to run", {}, "Example: jig exec -- cc --version", exit_code::usage});
    Ui qui(quiet(s));
    auto p = prepare(quiet(s), qui);
    if (!p) return report(ui, p.error());
    int status = exec_in_env(*p, ui, argv);
    if (status == 127) {
        ui.fail(Error{"DJ3101", "Command not found inside the environment: " + argv.front(), {}, "Run `jig toolchain which` to see the tools jig provides.", exit_code::failure});
    }
    return status < 0 ? 1 : status;
}

int build(const Settings& s, Ui& ui, const std::string& profile) {
    auto p = prepare(s, ui);
    if (!p) return report(ui, p.error());
    auto b = build_project(*p, ui, profile, false);
    if (!b) return report(ui, b.error());
    ui.hint(std::string(p->manifest.kind == "exe" ? "jig run" : "jig test") + "   ·   jig build --release   ·   jig doctor");
    return 0;
}

int run(const Settings& s, Ui& ui, const std::string& profile, const std::vector<std::string>& args) {
    auto p = prepare(s, ui);
    if (!p) return report(ui, p.error());
    auto r = run_project(*p, ui, profile, args);
    if (!r) return report(ui, r.error());
    return *r;
}

int test(const Settings& s, Ui& ui, const std::string& profile, const std::vector<std::string>& args) {
    auto p = prepare(s, ui);
    if (!p) return report(ui, p.error());
    auto r = test_project(*p, ui, profile, args);
    return r ? 0 : report(ui, r.error());
}

int fmt(const Settings& s, Ui& ui, bool check) {
    auto p = prepare(s, ui);
    if (!p) return report(ui, p.error());
    auto r = fmt_project(*p, ui, check);
    return r ? 0 : report(ui, r.error());
}

int clean(const Settings& s, Ui& ui, bool env_too, bool all) {
    auto m = open_manifest(s, ui, false);
    if (!m) return report(ui, m.error());
    auto r = clean_project(*m, ui, env_too, all);
    return r ? 0 : report(ui, r.error());
}

int cache(const Settings&, Ui& ui, const std::string& action) {
    auto dir = jig_home() / "cache";
    std::error_code ec;
    if (action == "dir") {
        ui.print(dir.string() + "\n");
        return 0;
    }
    if (action == "size") {
        std::uintmax_t total = 0;
        if (fs::exists(dir, ec))
            for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
                if (it->is_regular_file(ec)) total += it->file_size(ec);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.1f KiB  %s\n", static_cast<double>(total) / 1024.0, dir.string().c_str());
        ui.print(buf);
        return 0;
    }
    if (action == "clean") {
        auto n = fs::exists(dir, ec) ? fs::remove_all(dir, ec) : 0;
        ui.done("removed " + std::to_string(n) + " cache entries");
        return 0;
    }
    return report(ui, Error{"DJ6003", "Unknown cache action \"" + action + "\"", {}, "Use: dir, size, clean", exit_code::usage});
}

int version(const Settings&, Ui& ui) {
    auto h = host_info();
    ui.print(std::string("jig ") + JIG_VERSION + "\n  built with   " + JIG_BUILD_COMPILER + "\n  host         " + h.triple +
             "\n  providers    system (H0)   [llvm: not in this build]\n  site         https://djet.online/jig\n");
    return 0;
}

}  // namespace jig::cmd
