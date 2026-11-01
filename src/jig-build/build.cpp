// SPDX-License-Identifier: GPL-3.0-only
#include "jig-build/build.hpp"

#include <unistd.h>

#include <deque>
#include <thread>

#include "jig-build/cmakegen.hpp"
#include "jig-core/glob.hpp"
#include "jig-core/process.hpp"

namespace jig {

namespace {

fs::path build_dir_for(const Manifest& m, const std::string& profile) { return jig_dir(m) / "build" / profile; }
fs::path gen_dir_for(const Manifest& m, const std::string& profile) { return jig_dir(m) / "gen" / profile; }

std::string quote_cmd(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) s += (i ? " " : "") + argv[i];
    return s;
}

// Runs a command, streams its output as events (when `stream`), keeps the tail for error reports.
struct Streamed {
    int status = -1;
    bool spawned = false;
    std::deque<std::string> tail;
};

Streamed run_logged(Ui& ui, const std::vector<std::string>& argv, const fs::path& cwd, const EnvMap& env, bool stream) {
    ui.exec(argv, cwd.string());
    Streamed s;
    auto r = run_streaming(argv, cwd, env,
                           [&](const std::string& line) {
                               s.tail.push_back(line);
                               if (s.tail.size() > 40) s.tail.pop_front();
                               if (stream) ui.output(line);
                           });
    s.status = r.exit_status;
    s.spawned = r.spawned;
    return s;
}

std::vector<std::string> tail_lines(const Streamed& s) { return {s.tail.begin(), s.tail.end()}; }

fs::path artifact_for(const Manifest& m, const fs::path& build_dir) {
    if (m.kind == "exe") return build_dir / m.name;
    if (m.kind == "static-lib") return build_dir / ("lib" + m.name + ".a");
    if (m.kind == "shared-lib")
#ifdef __APPLE__
        return build_dir / ("lib" + m.name + ".dylib");
#else
        return build_dir / ("lib" + m.name + ".so");
#endif
    return {};
}

}  // namespace

Result<BuildOutcome> build_project(const Prepared& p, Ui& ui, const std::string& profile_name, bool with_tests) {
    const Manifest& m = p.manifest;
    Stopwatch total;

    JIG_TRY(profile, resolve_profile(m, profile_name));
    auto flags = profile_flags(m, p.lock, profile);
    ui.step("Configuring");
    ui.decision("profile", profile.name, {flags.summary});

    auto build_dir = build_dir_for(m, profile.name);
    fs::path source_dir;
    std::vector<std::string> configure_extra;

    if (m.backend == "generated") {
        JIG_TRY(gen, generate_cmake(m, p.lock, profile));
        ui.fact("sources", std::to_string(gen.sources.size()) + " file(s)" +
                               (with_tests ? ", " + std::to_string(gen.test_sources.size()) + " test file(s)" : ""));
        source_dir = gen_dir_for(m, profile.name);
        auto cmake_file = source_dir / "CMakeLists.txt";
        auto existing = read_file(cmake_file);
        if (!existing || *existing != gen.cmake) {
            JIG_TRY_VOID(write_file_atomic(cmake_file, gen.cmake));
            ui.decision("cmake project", cmake_file.lexically_relative(m.root).string(), {"generated from jig.toml; the file list changed"});
        } else {
            ui.fact("cmake project", cmake_file.lexically_relative(m.root).string() + " is current");
        }
        configure_extra.push_back(std::string("-DJIG_BUILD_TESTS=") + (with_tests ? "ON" : "OFF"));
    } else {  // backend = "cmake": the project's own CMakeLists.txt
        source_dir = m.root;
        if (!fs::exists(m.root / "CMakeLists.txt"))
            return fail("DJ4001", exit_code::build, "backend = \"cmake\" but there is no CMakeLists.txt in " + m.root.string());
        configure_extra.push_back(std::string("-DCMAKE_BUILD_TYPE=") + (profile.name == "release" ? "Release" : "Debug"));
        for (const auto& a : m.cmake_args) configure_extra.push_back(a);
        ui.decision("cmake project", "CMakeLists.txt", {"build.backend = \"cmake\": jig uses your CMake project as is"});
    }

    const bool ninja = p.lock.tool("ninja") != nullptr;
    std::vector<std::string> configure = {"cmake", "-S", source_dir.string(), "-B", build_dir.string(), "-G", ninja ? "Ninja" : "Unix Makefiles",
                                          "-DCMAKE_TOOLCHAIN_FILE=" + (env_dir(m) / "toolchain.cmake").string(), "--no-warn-unused-cli"};
    for (auto& e : configure_extra) configure.push_back(e);
    auto cfg = run_logged(ui, configure, m.root, p.env.vars, ui.verbosity() >= 3);
    if (!cfg.spawned || cfg.status != 0) {
        return fail("DJ4001", exit_code::build, "CMake configure failed", "Read the last lines of the CMake output above; rerun with -vv to see the command.",
                    tail_lines(cfg));
    }

    // Keep ./compile_commands.json pointing at the current profile so clangd and editors find it.
    {
        auto link = m.root / "compile_commands.json";
        auto target = build_dir / "compile_commands.json";
        std::error_code ec;
        if (fs::exists(target, ec) && (!fs::exists(link, ec) || fs::is_symlink(link, ec))) {
            fs::remove(link, ec);
            fs::create_symlink(fs::relative(target, m.root, ec), link, ec);
            if (!ec) ui.decision("compile commands", "./compile_commands.json", {"links to " + target.lexically_relative(m.root).string()});
        }
    }

    ui.step("Building");
    std::vector<std::string> build = {"cmake", "--build", build_dir.string(), "--parallel"};
    auto bld = run_logged(ui, build, m.root, p.env.vars, true);
    if (!bld.spawned || bld.status != 0)
        return fail("DJ4002", exit_code::build, "The build failed", "Fix the first error shown above, then run `jig build` again.");

    BuildOutcome out;
    out.build_dir = build_dir;
    out.profile = profile.name;
    out.artifact = artifact_for(m, build_dir);
    std::string what = out.artifact.empty() ? m.name + " (header-only, nothing to compile)" : out.artifact.lexically_relative(m.root).string();
    ui.done("built " + what, total.seconds());
    return out;
}

Result<int> run_project(const Prepared& p, Ui& ui, const std::string& profile, const std::vector<std::string>& args) {
    const Manifest& m = p.manifest;
    if (m.kind != "exe")
        return fail("DJ4003", exit_code::failure, "Nothing to run: this project is a " + m.kind, "Only `kind = \"exe\"` projects can run. Use `jig test` for libraries.");
    JIG_TRY(built, build_project(p, ui, profile, false));
    ui.step("Running");
    std::vector<std::string> argv = {built.artifact.string()};
    for (const auto& a : args) argv.push_back(a);
    ui.exec(argv, m.root.string());
    ui.fact("program", built.artifact.lexically_relative(m.root).string() + (args.empty() ? "" : "  " + quote_cmd(args)));
    int status = run_inherit(argv, m.root, p.env.vars);
    if (status < 0) return fail("DJ4003", exit_code::failure, "Could not start " + built.artifact.string());
    return status;
}

Result<void> test_project(const Prepared& p, Ui& ui, const std::string& profile, const std::vector<std::string>& extra) {
    const Manifest& m = p.manifest;
    JIG_TRY(built, build_project(p, ui, profile, true));
    auto tests = glob_files(m.root, m.test_sources);
    if (tests.empty()) {
        ui.warn("DJ4103", "No test files match `test.sources`", "Add files under tests/, or change `[test] sources`.");
        return {};
    }
    ui.step("Testing");
    std::vector<std::string> argv = {"ctest", "--test-dir", built.build_dir.string()};
    for (const auto& a : m.test_args) argv.push_back(a);
    for (const auto& a : extra) argv.push_back(a);
    Stopwatch sw;
    auto r = run_logged(ui, argv, m.root, p.env.vars, true);
    if (!r.spawned || r.status != 0)
        return fail("DJ4005", exit_code::test, "Tests failed", "See the failing test output above.");
    ui.done(std::to_string(tests.size()) + " test executable(s) passed", sw.seconds());
    return {};
}

Result<void> clean_project(const Manifest& m, Ui& ui, bool env, bool all) {
    std::error_code ec;
    ui.step("Cleaning");
    auto remove = [&](const fs::path& d) {
        if (fs::exists(d, ec)) {
            auto n = fs::remove_all(d, ec);
            ui.fact("removed", d.lexically_relative(m.root).string() + " (" + std::to_string(n) + " entries)");
        }
    };
    if (all) {
        remove(jig_dir(m));
    } else {
        remove(jig_dir(m) / "build");
        remove(jig_dir(m) / "gen");
        if (env) remove(env_dir(m));
    }
    auto link = m.root / "compile_commands.json";
    if (fs::is_symlink(link, ec)) {
        auto tgt = fs::read_symlink(link, ec).string();
        if (tgt.rfind(".jig/", 0) == 0) fs::remove(link, ec);
    }
    ui.done("clean");
    return {};
}

Result<void> fmt_project(const Prepared& p, Ui& ui, bool check) {
    const Manifest& m = p.manifest;
    std::string tool = "clang-format";
    if (!which("clang-format", p.env.vars.count("PATH") ? std::optional<std::string>(p.env.vars.at("PATH")) : std::nullopt))
        return fail("DJ4007", exit_code::failure, "clang-format was not found", "Install clang-format (apt install clang-format / brew install clang-format).");
    std::vector<std::string> patterns;
    for (const char* dir : {"src", "include", "tests"})
        if (fs::exists(m.root / dir)) patterns.push_back(std::string(dir) + "/**/*.{c,cc,cpp,cxx,h,hh,hpp,hxx}");
    auto files = glob_files(m.root, patterns);
    ui.step(check ? "Checking format" : "Formatting");
    if (files.empty()) {
        ui.warn("", "No source files found under src/, include/ or tests/");
        return {};
    }
    std::vector<std::string> argv = {tool, "--style=" + m.fmt_style, "--fallback-style=LLVM"};
    if (check) {
        argv.push_back("--dry-run");
        argv.push_back("-Werror");
    } else {
        argv.push_back("-i");
    }
    for (const auto& f : files) argv.push_back(f);
    ui.fact("files", std::to_string(files.size()));
    auto r = run_logged(ui, argv, m.root, p.env.vars, true);
    if (!r.spawned || r.status != 0)
        return check ? fail("DJ4006", exit_code::test, "Formatting differs from the style", "Run `jig fmt` to fix it.")
                     : fail("DJ4007", exit_code::failure, "clang-format failed");
    ui.done(check ? "formatting is clean" : "formatted " + std::to_string(files.size()) + " file(s)");
    return {};
}

int exec_in_env(const Prepared& p, Ui& ui, const std::vector<std::string>& argv) {
    ui.exec(argv, p.manifest.root.string());
    return run_inherit(argv, fs::current_path(), p.env.vars);
}

int shell_in_env(const Prepared& p, Ui& ui) {
    std::string sh = getenv_str("SHELL").value_or("/bin/sh");
    ui.fact("shell", sh + "   (leave it with `exit`)");
    auto env = p.env.vars;
    env["JIG_SHELL"] = "1";
    return run_inherit({sh}, fs::current_path(), env);
}

}  // namespace jig
