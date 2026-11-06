// SPDX-License-Identifier: AGPL-3.0-only
#include "jig-env/toolchain.hpp"

#include <unistd.h>

#include <algorithm>
#include <map>
#include <regex>
#include <set>

#include "jig-core/process.hpp"
#include "jig-core/sha256.hpp"

namespace jig {

std::string Compiler::label() const { return family + " " + version.str(); }

namespace {

EnvMap probe_env() {
    auto e = current_env();
    e["LC_ALL"] = "C";
    return e;
}

fs::path real_path(const fs::path& p) {
    std::error_code ec;
    auto r = fs::canonical(p, ec);
    return ec ? p : r;
}

std::optional<std::string> first_match(const std::string& text, const std::regex& re) {
    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() > 1) return m[1].str();
    return std::nullopt;
}

// Xcode's clang reports its own version line; map to the LLVM release it is based on (approximate).
int apple_to_llvm(int apple_major) {
    if (apple_major >= 16) return apple_major + 1;
    if (apple_major == 15) return 16;
    return apple_major;
}

int effective_clang_major(const Compiler& c) {
    return c.family == "apple-clang" ? apple_to_llvm(c.version.major) : c.version.major;
}

std::string as_gnu(std::string s) {
    if (s.rfind("c++", 0) == 0) return "gnu++" + s.substr(3);
    if (s.rfind("c", 0) == 0) return "gnu" + s.substr(1);
    return s;
}

}  // namespace

std::vector<Compiler> detect_system_compilers(Ui& ui) {
    static const std::regex name_re(R"(^(gcc|g\+\+|clang|clang\+\+)(-[0-9]+)?$)");
    std::map<std::string, fs::path> found;  // driver name -> first path on PATH
    for (const auto& dir : path_dirs()) {
        std::error_code ec;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            auto name = it->path().filename().string();
            if (!std::regex_match(name, name_re)) continue;
            if (found.count(name)) continue;
            if (access(it->path().c_str(), X_OK) != 0) continue;
            found.emplace(name, it->path());
        }
    }
    std::set<std::string> suffixes;
    for (const auto& [name, _] : found) {
        std::smatch m;
        if (std::regex_match(name, m, name_re)) suffixes.insert(m[2].str());
    }

    std::vector<Compiler> out;
    std::set<std::string> seen;
    static const std::regex clang_re(R"(clang version (\d+\.\d+\.\d+))");
    static const std::regex apple_re(R"(Apple (?:clang|LLVM) version (\d+\.\d+\.\d+))");
    static const std::regex ver_re(R"((\d+\.\d+\.\d+))");

    for (const auto& suffix : suffixes) {
        for (auto [cc_base, cxx_base] : {std::pair{"gcc", "g++"}, std::pair{"clang", "clang++"}}) {
            auto cc = found.find(cc_base + suffix);
            auto cxx = found.find(cxx_base + suffix);
            if (cc == found.end() || cxx == found.end()) continue;

            std::vector<std::string> argv{cxx->second.string(), "--version"};
            ui.exec(argv);
            auto r = run_capture(argv, probe_env());
            if (!r.spawned || r.exit_status != 0) {
                ui.info("probe failed", cxx->second.string());
                continue;
            }
            Compiler c;
            c.cc = cc->second;
            c.cxx = cxx->second;
            c.cxx_real = real_path(cxx->second);
            c.driver_name = cxx->first;

            if (auto v = first_match(r.output, apple_re)) {
                c.family = "apple-clang";
                c.version = *parse_version(*v);
            } else if (auto v2 = first_match(r.output, clang_re)) {
                c.family = "clang";
                c.version = *parse_version(*v2);
            } else {
                c.family = "gcc";
                // `--version` text varies by distribution; -dumpfullversion is reliable for gcc.
                std::vector<std::string> a2{cxx->second.string(), "-dumpfullversion"};
                ui.exec(a2);
                auto r2 = run_capture(a2, probe_env());
                std::string text = (r2.spawned && r2.exit_status == 0) ? r2.output : r.output;
                auto v3 = first_match(text, ver_re);
                if (!v3) {
                    ui.info("unparsable version", cxx->second.string());
                    continue;
                }
                c.version = *parse_version(*v3);
            }
            auto key = c.family + "|" + c.version.str() + "|" + c.cxx_real.string();
            if (!seen.insert(key).second) {
                ui.info("duplicate", cxx->first + " is the same binary as an earlier entry");
                continue;
            }
            ui.info("found", c.label() + "  " + c.cxx.string());
            out.push_back(std::move(c));
        }
    }
    return out;
}

std::optional<ToolInfo> probe_tool(const std::string& name, Ui* ui) {
    auto p = which(name);
    if (!p) return std::nullopt;
    ToolInfo t;
    t.name = name;
    t.path = *p;
    t.real_path = real_path(*p);
    std::vector<std::string> argv{p->string(), "--version"};
    if (ui) ui->exec(argv);
    auto r = run_capture(argv, probe_env());
    static const std::regex ver_re(R"((\d+\.\d+(?:\.\d+)?))");
    if (r.spawned && r.exit_status == 0)
        if (auto v = first_match(r.output.substr(0, r.output.find('\n') == std::string::npos ? r.output.size() : r.output.find('\n')), ver_re))
            t.version = *v;
    if (t.version.empty() && r.spawned) {
        if (auto v = first_match(r.output, ver_re)) t.version = *v;
    }
    return t;
}

std::optional<std::string> std_flag(const Compiler& c, std::string_view language, std::string_view std, bool extensions) {
    std::optional<std::string> base;
    const int maj = c.version.major;
    const bool gcc = c.family == "gcc";
    const int eff = gcc ? maj : effective_clang_major(c);

    if (language == "c++") {
        if (std == "c++14") {
            if (gcc ? maj >= 5 : eff >= 4) base = "c++14";
        } else if (std == "c++17") {
            if (gcc ? maj >= 7 : eff >= 5) base = "c++17";
        } else if (std == "c++20") {
            if (gcc ? maj >= 10 : eff >= 10) base = "c++20";
            else if (gcc ? maj >= 8 : eff >= 6) base = "c++2a";
        } else if (std == "c++23") {
            if (gcc) {
                if (maj >= 12) base = "c++23";
                else if (maj == 11) base = "c++2b";
            } else {
                if (eff >= 17) base = "c++23";
                else if (eff >= 13) base = "c++2b";
            }
        } else if (std == "c++26") {
            if (gcc) {
                if (maj >= 14) base = "c++26";
            } else {
                if (eff >= 18) base = "c++26";
                else if (eff == 17) base = "c++2c";
            }
        }
    } else if (language == "c") {
        if (std == "c11") {
            if (gcc ? maj >= 5 : eff >= 4) base = "c11";
        } else if (std == "c17") {
            if (gcc ? maj >= 8 : eff >= 6) base = "c17";
        } else if (std == "c23") {
            if (gcc) {
                if (maj >= 14) base = "c23";
                else if (maj >= 9) base = "c2x";
            } else {
                if (eff >= 18) base = "c23";
                else if (eff >= 9) base = "c2x";
            }
        }
    }
    if (!base) return std::nullopt;
    return extensions ? as_gnu(*base) : *base;
}

std::vector<std::string> accepted_stds(const Compiler& c, std::string_view language) {
    std::vector<std::string> out;
    for (const auto& s : standards_for(language))
        if (std_flag(c, language, s, false)) out.push_back(s);
    return out;
}

std::optional<ToolchainSpec> parse_toolchain_spec(std::string_view s) {
    ToolchainSpec t;
    auto at = s.find('@');
    std::string_view fam = s.substr(0, at);
    if (at != std::string_view::npos) t.version = std::string(s.substr(at + 1));
    if (fam == "gcc" || fam == "g++") t.family = "gcc";
    else if (fam == "clang" || fam == "clang++") t.family = "clang";
    else if (fam.empty() || fam == "system" || fam == "any") t.family = "";
    else return std::nullopt;
    if (!t.version.empty() && !VersionSpec::parse(t.version)) return std::nullopt;
    return t;
}

Result<Resolution> resolve_toolchain(const Manifest& m, const std::vector<Compiler>& detected, Ui& ui) {
    if (m.vendor == "llvm")
        return fail("DJ9001", exit_code::failure, "toolchain vendor \"llvm\" is not implemented in this build",
                    "Use the machine's compilers for now: `jig toolchain use gcc@13` or `clang@18` (sets vendor = \"system\"). See CODE-STATUS.md.",
                    {"The downloaded, pinned LLVM toolchain (hermeticity H1) needs the signed index from docs/blueprint/004."});
    if (m.hermeticity && *m.hermeticity != "h0")
        return fail("DJ2301", exit_code::toolchain,
                    "This project requires hermeticity " + *m.hermeticity + ", but the system provider gives h0",
                    "Lower `[toolchain] hermeticity` to \"h0\", or wait for the llvm provider.");

    if (detected.empty())
        return fail("DJ2201", exit_code::toolchain, "No system compiler found on PATH",
                    "Install gcc/g++ or clang/clang++, then run `jig doctor`.",
                    {"jig looked for gcc, g++, clang, clang++ and versioned names such as g++-13."});

    // What does the manifest ask for?
    std::string want_family;
    auto names = m.sys_cxx + " " + m.sys_cc;
    if (names.find("clang") != std::string::npos) want_family = "clang";
    else if (names.find("g++") != std::string::npos || names.find("gcc") != std::string::npos) want_family = "gcc";

    struct Verdict {
        const Compiler* c;
        std::string reject;  // empty = acceptable
        bool family_ok = true, version_ok = true, std_ok = true;
    };
    std::vector<Verdict> verdicts;
    for (const auto& c : detected) {
        Verdict v{&c, {}};
        bool fam_match = want_family.empty() || (want_family == "clang" ? (c.family == "clang" || c.family == "apple-clang") : c.family == "gcc");
        if (!fam_match) {
            v.family_ok = false;
            v.reject = "the project asks for " + want_family + ", this is " + c.family;
        } else if (!m.toolchain_version.matches(c.version)) {
            v.version_ok = false;
            v.reject = c.version.str() + " does not satisfy \"" + m.toolchain_version.text() + "\"";
        } else if (!std_flag(c, m.language, m.std, m.extensions)) {
            v.std_ok = false;
            v.reject = c.label() + " cannot compile " + m.std;
        }
        verdicts.push_back(std::move(v));
    }

    std::vector<const Verdict*> ok;
    for (const auto& v : verdicts)
        if (v.reject.empty()) ok.push_back(&v);

    if (ok.empty()) {
        std::vector<std::string> det;
        for (const auto& c : detected) det.push_back(c.label() + "  (" + c.cxx.string() + ")");
        bool any_fam_ver = std::any_of(verdicts.begin(), verdicts.end(), [](const Verdict& v) { return v.family_ok && v.version_ok; });
        std::vector<std::string> details = {"Detected on this machine:"};
        for (auto& d : det) details.push_back("  " + d);
        if (any_fam_ver)
            return fail("DJ2102", exit_code::toolchain,
                        "No matching compiler accepts " + m.std, "Use a newer compiler or a lower standard (`project.std`).", details);
        return fail("DJ2101", exit_code::toolchain,
                    "No detected compiler satisfies the toolchain request" + (m.toolchain_version.is_latest() ? std::string{} : " \"" + m.toolchain_version.text() + "\"") +
                        (want_family.empty() ? std::string{} : " (" + want_family + ")"),
                    "Pick one of the detected versions with `jig toolchain use <family>@<version>`; list them with `jig toolchain list`.", details);
    }

    const Verdict* best = ok.front();
    for (auto* v : ok)
        if (v->c->version > best->c->version) best = v;

    Resolution r;
    r.compiler = *best->c;
    r.std_flag = *std_flag(r.compiler, m.language, m.std, m.extensions);

    std::vector<std::string> why;
    why.push_back(m.toolchain_version.is_latest() ? "highest version among " + std::to_string(ok.size()) + " usable compiler(s)"
                                                   : "highest version satisfying \"" + m.toolchain_version.text() + "\"");
    why.push_back("accepts " + m.std);
    std::vector<Alt> alts;
    for (const auto& v : verdicts) {
        if (v.c == best->c) continue;
        alts.push_back({v.c->label() + " (" + v.c->cxx.string() + ")", v.reject.empty() ? "usable, but older than the choice" : v.reject});
    }
    ui.decision("compiler", r.compiler.label() + "  (" + r.compiler.cxx.string() + ")", why, alts);

    std::string flag_why = "-std=" + r.std_flag;
    ui.decision("std flag", "-std=" + r.std_flag,
                {r.compiler.label() + " spells " + m.std + (m.extensions ? " (GNU dialect, extensions = true)" : "") + " as " + r.std_flag});
    ui.decision("hermeticity", "H0 (detected)",
                {"vendor \"system\": jig records this compiler, it does not provide it", "H1 needs the llvm provider (not in this build)"});

    // Tools.
    auto cmake = probe_tool("cmake", &ui);
    if (!cmake)
        return fail("DJ2203", exit_code::toolchain, "cmake not found on PATH", "Install CMake 3.20 or newer (apt install cmake / brew install cmake).");
    if (auto spec = VersionSpec::parse(m.tool_cmake); spec && !cmake->version.empty()) {
        auto cv = parse_version(cmake->version);
        if (cv && !spec->matches(*cv))
            return fail("DJ2101", exit_code::toolchain, "cmake " + cmake->version + " does not satisfy \"" + m.tool_cmake + "\"",
                        "Change `[toolchain.tools] cmake`, or install a matching CMake.");
    }
    r.tools.push_back(*cmake);
    ui.decision("cmake", cmake->version + "  (" + cmake->path.string() + ")", {"found on PATH"});
    if (auto ninja = probe_tool("ninja", &ui)) {
        r.tools.push_back(*ninja);
        ui.decision("generator", "Ninja " + ninja->version, {"found on PATH"});
    } else {
        ui.decision("generator", "Unix Makefiles", {"ninja was not found on PATH"}, {{"Ninja", "not installed"}});
    }
    if (auto cf = probe_tool("clang-format", &ui)) r.tools.push_back(*cf);
    return r;
}

Result<Lock> make_lock(const Manifest& m, const Resolution& r) {
    Lock l;
    l.generated_by = std::string("jig ") + (
#ifdef JIG_VERSION
        JIG_VERSION
#else
        "0.1.0-dev"
#endif
    );
    l.manifest_digest = m.digest;
    auto& t = l.toolchain;
    t.vendor = "system";
    t.family = r.compiler.family;
    t.version = r.compiler.version.str();
    t.host = host_info().triple;
    t.cc = r.compiler.cc.string();
    t.cxx = r.compiler.cxx.string();
    t.cxx_real = r.compiler.cxx_real.string();
    auto hash = sha256_file(r.compiler.cxx_real);
    if (!hash) return fail("DJ2202", exit_code::toolchain, "Cannot read the compiler binary " + t.cxx_real, "Check its permissions.");
    t.sha256 = *hash;
    t.std_flag = r.std_flag;
    t.hermeticity = "h0";
    t.id = "system-" + t.family + "-" + t.version + "-" + t.host + "-" + t.sha256.substr(0, 8);
    for (const auto& tool : r.tools) {
        LockedTool lt;
        lt.name = tool.name;
        lt.version = tool.version;
        lt.path = tool.real_path.string();
        lt.sha256 = sha256_file(tool.real_path).value_or("");
        l.tools.push_back(std::move(lt));
    }
    return l;
}

}  // namespace jig
