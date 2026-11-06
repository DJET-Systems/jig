// SPDX-License-Identifier: AGPL-3.0-only
#include "jig-env/env.hpp"

#include <unistd.h>

#include <algorithm>
#include <set>

#include "jig-core/sha256.hpp"
#include "jig-core/tomledit.hpp"

#ifndef JIG_VERSION
#define JIG_VERSION "0.1.0-dev"
#endif

namespace jig {

fs::path jig_dir(const Manifest& m) { return m.root / ".jig"; }
fs::path env_dir(const Manifest& m) { return jig_dir(m) / "env"; }
fs::path lock_file(const Manifest& m) { return m.root / "jig.lock"; }

namespace {

std::string sh_quote(const std::string& s) {
    std::string o = "'";
    for (char c : s) {
        if (c == '\'') o += "'\\''";
        else o.push_back(c);
    }
    return o + "'";
}

// Variables that commonly leak the machine into a build (reference/env-vars.md).
const std::set<std::string> kLeaking = {
    "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
    "LIBRARY_PATH", "LD_LIBRARY_PATH", "LD_PRELOAD", "DYLD_LIBRARY_PATH", "CMAKE_PREFIX_PATH", "CMAKE_GENERATOR",
    "CMAKE_BUILD_TYPE", "CMAKE_TOOLCHAIN_FILE", "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_C_FLAGS",
    "CMAKE_CXX_FLAGS", "CC", "CXX", "AR", "LD", "NM", "RANLIB", "CCACHE_DISABLE", "SDKROOT"};

bool is_kept_when_none(const std::string& k) {
    static const std::set<std::string> keep = {"PATH", "HOME", "USER", "LOGNAME", "TERM", "LANG", "TMPDIR", "SHELL", "NO_COLOR", "PWD"};
    return keep.count(k) || k.rfind("LC_", 0) == 0;
}

struct Shim {
    std::string name;
    std::string target;
};

std::vector<Shim> shim_list(const Lock& l) {
    std::vector<Shim> s;
    const auto& t = l.toolchain;
    s.push_back({"cc", t.cc});
    s.push_back({"c++", t.cxx});
    if (t.family == "gcc") {
        s.push_back({"gcc", t.cc});
        s.push_back({"g++", t.cxx});
    } else {
        s.push_back({"clang", t.cc});
        s.push_back({"clang++", t.cxx});
    }
    for (const auto& tool : l.tools) s.push_back({tool.name, tool.path});
    return s;
}

std::string shim_script(const Shim& s) {
    return "#!/bin/sh\n# jig shim for `" + s.name + "`, written by `jig sync` from jig.lock.\nexec " + sh_quote(s.target) + " \"$@\"\n";
}

std::string toolchain_cmake(const Lock& l) {
    std::string s = "# Written by `jig sync` from jig.lock. Do not edit.\n";
    s += "set(CMAKE_C_COMPILER " + toml_quote(l.toolchain.cc) + " CACHE FILEPATH \"\")\n";
    s += "set(CMAKE_CXX_COMPILER " + toml_quote(l.toolchain.cxx) + " CACHE FILEPATH \"\")\n";
    s += "set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL \"\")\n";
    return s;
}

std::string activate_script(const fs::path& env, const Manifest& m, const Lock& l) {
    auto bin = (env / "bin").string();
    std::string s;
    s += "# jig environment for `" + m.name + "`. Use it with:  . .jig/env/activate\n";
    s += "# Written by `jig sync`. Run `jig_deactivate` to leave it.\n";
    s += "if [ -n \"${JIG_ENV:-}\" ] && command -v jig_deactivate >/dev/null 2>&1; then jig_deactivate; fi\n";
    s += "_JIG_OLD_PATH=\"$PATH\"\n";
    s += "export JIG_ENV=" + sh_quote(env.string()) + "\n";
    s += "export JIG_PROJECT_DIR=" + sh_quote(m.root.string()) + "\n";
    s += "export PATH=" + sh_quote(bin) + ":\"$PATH\"\n";
    s += "export CC=" + sh_quote(bin + "/cc") + "\n";
    s += "export CXX=" + sh_quote(bin + "/c++") + "\n";
    s += "export CMAKE_TOOLCHAIN_FILE=" + sh_quote((env / "toolchain.cmake").string()) + "\n";
    s += "export CMAKE_EXPORT_COMPILE_COMMANDS=ON\n";
    s += l.tool("ninja") ? "export CMAKE_GENERATOR=Ninja\n" : "export CMAKE_GENERATOR='Unix Makefiles'\n";
    for (const auto& [k, v] : m.env_set) s += "export " + k + "=" + sh_quote(v) + "\n";
    s += "jig_deactivate() {\n  PATH=\"$_JIG_OLD_PATH\"; export PATH\n  unset _JIG_OLD_PATH JIG_ENV JIG_PROJECT_DIR CC CXX CMAKE_TOOLCHAIN_FILE CMAKE_EXPORT_COMPILE_COMMANDS CMAKE_GENERATOR\n  unset -f jig_deactivate\n}\n";
    return s;
}

std::string json_array_obj(const std::vector<Shim>& shims) {
    std::string j = "{\n";
    for (std::size_t i = 0; i < shims.size(); ++i)
        j += "  \"" + json_escape(shims[i].name) + "\": \"" + json_escape(shims[i].target) + "\"" + (i + 1 < shims.size() ? ",\n" : "\n");
    return j + "}\n";
}

}  // namespace

ChildEnv make_child_env(const Manifest& m, const Lock& l) {
    ChildEnv ce;
    auto cur = current_env();
    auto bin = (env_dir(m) / "bin").string();

    if (m.env_inherit == "all") {
        ce.vars = cur;
    } else {
        for (const auto& [k, v] : cur) {
            bool drop = (m.env_inherit == "none") ? !is_kept_when_none(k) : kLeaking.count(k) > 0;
            if (drop) ce.dropped.push_back(k);
            else ce.vars[k] = v;
        }
    }
    auto set = [&](const std::string& k, const std::string& v) {
        ce.vars[k] = v;
        ce.set.push_back(k);
    };
    auto old_path = ce.vars.count("PATH") ? ce.vars["PATH"] : std::string("/usr/bin:/bin");
    set("PATH", bin + ":" + old_path);
    set("CC", bin + "/cc");
    set("CXX", bin + "/c++");
    set("CMAKE_TOOLCHAIN_FILE", (env_dir(m) / "toolchain.cmake").string());
    set("CMAKE_EXPORT_COMPILE_COMMANDS", "ON");
    set("CMAKE_GENERATOR", l.tool("ninja") ? "Ninja" : "Unix Makefiles");
    set("JIG_ENV", env_dir(m).string());
    set("JIG_PROJECT_DIR", m.root.string());
    for (const auto& [k, v] : m.env_set) set(k, v);
    return ce;
}

std::string render_env(const Manifest& m, const Lock& l, const std::string& format) {
    auto ce = make_child_env(m, l);
    std::string out;
    if (format == "json") {
        out = "{\n  \"set\": {\n";
        for (std::size_t i = 0; i < ce.set.size(); ++i)
            out += "    \"" + json_escape(ce.set[i]) + "\": \"" + json_escape(ce.vars[ce.set[i]]) + "\"" + (i + 1 < ce.set.size() ? ",\n" : "\n");
        out += "  },\n  \"unset\": [";
        for (std::size_t i = 0; i < ce.dropped.size(); ++i) out += (i ? ", " : "") + std::string("\"") + json_escape(ce.dropped[i]) + "\"";
        out += "]\n}\n";
        return out;
    }
    if (format == "fish") {
        for (const auto& k : ce.dropped) out += "set -e " + k + "\n";
        for (const auto& k : ce.set) out += "set -gx " + k + " " + sh_quote(ce.vars[k]) + "\n";
        return out;
    }
    for (const auto& k : ce.dropped) out += "unset " + k + "\n";
    for (const auto& k : ce.set) out += "export " + k + "=" + sh_quote(ce.vars[k]) + "\n";
    return out;
}

bool env_in_sync(const Manifest& m) {
    auto state = read_file(env_dir(m) / "state.json");
    if (!state) return false;
    auto digest = lock_digest(lock_file(m));
    if (!digest) return false;
    return state->find("\"lock-digest\": \"" + *digest + "\"") != std::string::npos && fs::exists(env_dir(m) / "bin" / "c++") &&
           fs::exists(env_dir(m) / "toolchain.cmake");
}

Result<void> sync_env(const Manifest& m, const Lock& l, Ui& ui) {
    // The compiler on disk must still be the one in the lock.
    auto hash = sha256_file(l.toolchain.cxx_real);
    if (!hash)
        return fail("DJ3101", exit_code::toolchain, "The locked compiler is missing: " + l.toolchain.cxx_real,
                    "Install it again, or re-resolve with `jig lock --update`.");
    if (*hash != l.toolchain.sha256)
        return fail("DJ3101", exit_code::toolchain, "The compiler on this machine is not the one in jig.lock",
                    "It was updated or replaced. Run `jig lock --update` to lock the new one.",
                    {l.toolchain.cxx_real, "locked sha256:   " + l.toolchain.sha256, "current sha256:  " + *hash});
    auto digest = lock_digest(lock_file(m));
    if (!digest) return jig::unexpected<jig::Error>(digest.error());

    auto final_dir = env_dir(m);
    auto tmp = jig_dir(m) / ("env.tmp-" + std::to_string(::getpid()));
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "bin", ec);
    if (ec) return fail("DJ3102", exit_code::failure, "Cannot create " + tmp.string(), "Check permissions on the project directory.", {ec.message()});

    auto cleanup_fail = [&](const Error& e) -> Result<void> {
        std::error_code ec2;
        fs::remove_all(tmp, ec2);
        Error out = e;
        out.code = "DJ3102";
        out.exit = exit_code::failure;
        out.summary = "Building the environment failed; the previous one was kept: " + e.summary;
        return jig::unexpected<jig::Error>(out);
    };

    auto shims = shim_list(l);
    for (const auto& s : shims) {
        ui.info("shim", s.name + " -> " + s.target);
        if (auto r = write_file_atomic(tmp / "bin" / s.name, shim_script(s), true); !r) return cleanup_fail(r.error());
    }
    // env.dir paths inside generated files refer to the final location, not the temporary one.
    if (auto r = write_file_atomic(tmp / "toolchain.cmake", toolchain_cmake(l)); !r) return cleanup_fail(r.error());
    if (auto r = write_file_atomic(tmp / "activate", activate_script(final_dir, m, l)); !r) return cleanup_fail(r.error());
    if (auto r = write_file_atomic(tmp / "shims.json", json_array_obj(shims)); !r) return cleanup_fail(r.error());
    {
        auto ce = make_child_env(m, l);
        std::string j = "{\n";
        for (std::size_t i = 0; i < ce.set.size(); ++i)
            j += "  \"" + json_escape(ce.set[i]) + "\": \"" + json_escape(ce.vars[ce.set[i]]) + "\"" + (i + 1 < ce.set.size() ? ",\n" : "\n");
        j += "}\n";
        if (auto r = write_file_atomic(tmp / "env.json", j); !r) return cleanup_fail(r.error());
    }
    {
        std::string st = "{\n  \"jig\": \"" JIG_VERSION "\",\n  \"lock-digest\": \"" + *digest + "\",\n  \"toolchain\": \"" +
                         json_escape(l.toolchain.id) + "\",\n  \"hermeticity\": \"" + l.toolchain.hermeticity + "\"\n}\n";
        if (auto r = write_file_atomic(tmp / "state.json", st); !r) return cleanup_fail(r.error());
    }

    // Swap. rename() of a directory onto an existing one fails, so move the old one aside first.
    auto old = jig_dir(m) / ("env.old-" + std::to_string(::getpid()));
    bool had_old = fs::exists(final_dir);
    if (had_old) {
        fs::rename(final_dir, old, ec);
        if (ec) return cleanup_fail(Error{"", "cannot replace the existing environment", {ec.message()}, {}, 1});
    }
    fs::rename(tmp, final_dir, ec);
    if (ec) {
        if (had_old) fs::rename(old, final_dir, ec);
        return cleanup_fail(Error{"", "cannot move the new environment into place", {ec.message()}, {}, 1});
    }
    if (had_old) fs::remove_all(old, ec);
    return {};
}

}  // namespace jig
