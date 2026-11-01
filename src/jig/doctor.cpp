// SPDX-License-Identifier: GPL-3.0-only
// jig doctor (docs/blueprint/008): say what is true about this machine and project, and what would improve it.
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>

#include "jig-core/process.hpp"
#include "jig-core/sha256.hpp"
#include "jig/commands.hpp"
#include "jig-env/project.hpp"

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

namespace jig::cmd {

namespace {

struct Report {
    Ui& ui;
    std::string out;
    int warnings = 0;
    int errors = 0;
    void heading(const std::string& h) { out += (out.empty() ? "" : "\n") + ui.bold(h) + "\n"; }
    void ok(const std::string& k, const std::string& v) { out += "  " + ui.green(ui.ok_glyph()) + " " + pad(k) + v + "\n"; }
    void warn(const std::string& k, const std::string& v) {
        ++warnings;
        out += "  " + ui.yellow("!") + " " + pad(k) + v + "\n";
    }
    void bad(const std::string& k, const std::string& v) {
        ++errors;
        out += "  " + ui.red(ui.unicode() ? "\xE2\x9C\x97" : "x") + " " + pad(k) + v + "\n";
    }
    void note(const std::string& v) { out += ui.dim("    " + std::string(ui.unicode() ? "\xE2\x94\x82" : "|") + " " + v) + "\n"; }
    static std::string pad(const std::string& s) { return s.size() >= 20 ? s + " " : s + std::string(20 - s.size(), ' '); }
};

std::string gib(std::uintmax_t bytes) {
    char b[32];
    std::snprintf(b, sizeof b, "%.0f GiB", static_cast<double>(bytes) / (1024.0 * 1024 * 1024));
    return b;
}

}  // namespace

int doctor(const Settings& s, Ui& ui, bool strict) {
    Report r{ui, {}, 0, 0};
    auto host = host_info();

    r.heading("Host");
    r.ok("platform", host.triple);
#ifdef __GLIBC__
    r.ok("glibc", gnu_get_libc_version());
#endif
    auto home = jig_home();
    std::error_code ec;
    fs::create_directories(home, ec);
    if (ec || access(home.c_str(), W_OK) != 0) r.bad("JIG_HOME", home.string() + "  (not writable)");
    else {
        auto space = fs::space(home, ec);
        r.ok("JIG_HOME", home.string() + (ec ? "" : "   (" + gib(space.available) + " free)"));
    }
    {
        auto probe = fs::temp_directory_path(ec) / ("jig-doctor-" + std::to_string(::getpid()));
        fs::create_directories(probe, ec);
        fs::create_symlink("target", probe / "link", ec);
        if (ec) r.warn("symlinks", "not supported here; jig will copy instead");
        else r.ok("symlinks", "supported");
        fs::remove_all(probe, ec);
    }

    r.heading("Compilers on PATH");
    auto detected = detect_system_compilers(ui);
    if (detected.empty()) r.bad("compilers", "none found (looked for gcc, g++, clang, clang++)");
    for (const auto& c : detected) {
        r.ok(c.label(), c.cxx.string());
        std::string stds;
        for (const auto& x : accepted_stds(c, "c++")) stds += (stds.empty() ? "" : " ") + x;
        r.note("c++: " + stds);
    }

    r.heading("Tools");
    for (const char* t : {"cmake", "ninja", "git", "clang-format"}) {
        auto info = probe_tool(t);
        bool required = std::string(t) == "cmake";
        if (info) r.ok(t, (info->version.empty() ? "" : info->version + "   ") + info->path.string());
        else if (required) r.bad(t, "not found; jig needs CMake 3.20 or newer");
        else if (std::string(t) == "ninja") r.warn(t, "not found; builds fall back to Unix Makefiles");
        else if (std::string(t) == "clang-format") r.warn(t, "not found; `jig fmt` will not work");
        else r.warn(t, "not found; `jig init --vcs git` will skip repository creation");
    }

    r.heading("Environment variables that leak into builds");
    {
        auto cur = current_env();
        bool any = false;
        for (const char* k : {"CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH", "CMAKE_GENERATOR", "CMAKE_TOOLCHAIN_FILE"})
            if (cur.count(k)) {
                any = true;
                r.warn(k, cur[k] + "   (dropped inside the jig environment unless `[env] inherit = \"all\"`)");
            }
        if (!any) r.ok("none set", "");
    }

    // Project, when there is one.
    auto start = s.manifest_path ? *s.manifest_path : fs::current_path();
    auto file = find_manifest(start);
    if (file) {
        auto m = load_manifest(*file);
        r.heading("Project  " + (m ? m->name : file->parent_path().filename().string()));
        if (!m) {
            r.bad("jig.toml", m.error().code + "  " + m.error().summary);
        } else {
            r.ok("jig.toml", "valid   " + m->language + " " + m->std + "   " + m->kind);
            if (!fs::exists(lock_file(*m))) {
                r.warn("jig.lock", "missing; run `jig lock`");
            } else if (auto l = load_lock(lock_file(*m)); !l) {
                r.bad("jig.lock", l.error().code + "  " + l.error().summary);
            } else {
                if (lock_is_fresh(*m, *l)) r.ok("jig.lock", "fresh   " + l->toolchain.family + " " + l->toolchain.version);
                else r.warn("jig.lock", "stale; run `jig lock`");
                if (env_in_sync(*m)) r.ok("environment", ".jig/env is in sync");
                else r.warn("environment", "not in sync; run `jig sync`");
                r.heading("Hermeticity");
                r.ok("level", "H0  detected");
                r.note("jig found this compiler and recorded its hash; it does not provide it.");
                r.note("the compiler, its libc, headers and the C++ library still come from this machine");
                r.note("raise to H1: needs the llvm provider (signed index + store), not in this build");
                auto hash = sha256_file(l->toolchain.cxx_real);
                if (!hash) r.bad("locked compiler", l->toolchain.cxx_real + " is missing");
                else if (*hash != l->toolchain.sha256) r.warn("locked compiler", "changed since it was locked; run `jig lock --update`");
                else r.ok("locked compiler", "unchanged since jig.lock was written");
            }
        }
    } else {
        r.heading("Project");
        r.note("no jig.toml here; run `jig init` to create a project");
    }

    r.out += "\n";
    if (r.errors) r.out += "  " + ui.red(std::to_string(r.errors) + " problem(s)") + ", " + std::to_string(r.warnings) + " warning(s)\n";
    else if (r.warnings) r.out += "  " + ui.yellow(std::to_string(r.warnings) + " warning(s)") + ", no problems\n";
    else r.out += "  " + ui.green("everything looks good") + "\n";
    ui.print(r.out);

    if (r.errors) return 1;
    if (strict && r.warnings) {
        ui.fail(Error{"DJ8003", "doctor found warnings under --strict", {}, "Fix the listed findings.", exit_code::failure});
        return 1;
    }
    return 0;
}

}  // namespace jig::cmd
