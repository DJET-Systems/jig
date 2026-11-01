// SPDX-License-Identifier: GPL-3.0-only
// jig: the native C/C++ toolchain.  https://djet.online/jig
// main() only parses the command line (CLI11), builds Settings and dispatches (001 section 8).
#include <CLI11.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>

#include "jig-core/platform.hpp"
#include "jig/commands.hpp"

#ifndef JIG_VERSION
#define JIG_VERSION "0.1.0-dev"
#endif

namespace {

using namespace jig;

struct Cli {
    // global flags
    int verbose = 0;
    int quiet = 0;
    bool terse = false, explain = false, json = false, offline = false, locked = false, frozen = false, yes = false, no_interactive = false;
    std::string color;
    std::string manifest_path;
    std::string home;
    bool show_version = false;

    int rc = 0;
    bool ran = false;
};

bool env_on(const char* name) {
    auto v = getenv_str(name);
    return v && !v->empty() && *v != "0" && *v != "false";
}

Settings make_settings(const Cli& c) {
    Settings s;
    int v = c.verbose, q = c.quiet;
    if (auto e = getenv_str("JIG_VERBOSE"); e && v == 0) v = std::atoi(e->c_str());
    if (auto e = getenv_str("JIG_QUIET"); e && q == 0) q = std::atoi(e->c_str());
    s.verbosity = 2 + v - q;
    if (c.terse || env_on("JIG_TERSE")) s.verbosity = std::min(s.verbosity, 1);
    s.verbosity = std::clamp(s.verbosity, 0, 5);
    s.explain = c.explain;
    s.json = c.json || env_on("JIG_JSON");
    s.color = !c.color.empty() ? c.color : getenv_str("JIG_COLOR").value_or("auto");
    s.offline = c.offline || env_on("JIG_OFFLINE");
    s.frozen = c.frozen || env_on("JIG_FROZEN");
    s.locked = c.locked || env_on("JIG_LOCKED") || s.frozen;
    s.yes = c.yes || env_on("JIG_YES");
    s.interactive = !(c.no_interactive || env_on("JIG_NO_INTERACTIVE")) && stdin_is_tty();
    if (!c.manifest_path.empty()) s.manifest_path = fs::path(c.manifest_path);
    else if (auto m = getenv_str("JIG_MANIFEST"); m && !m->empty()) s.manifest_path = fs::path(*m);
    return s;
}

std::string profile_of(const std::string& profile, bool release) { return release ? "release" : (profile.empty() ? "debug" : profile); }

}  // namespace

int main(int argc, char** argv) {
    Cli c;
    CLI::App app{"jig, the native C/C++ toolchain.\nPin the compiler. Pick the standard. Build in an environment that belongs to the project.", "jig"};
    app.set_version_flag("-V,--version", std::string("jig ") + JIG_VERSION);
    app.require_subcommand(1);
    app.fallthrough();
    app.footer(
        "\nExamples:\n"
        "  jig init hello --lang c++ --std c++23 --toolchain gcc@13     create a project\n"
        "  jig build && jig run                                         build and run it\n"
        "  jig toolchain list                                           what compilers can jig use here\n"
        "  jig doctor                                                   check this machine and project\n"
        "\nDocs: https://djet.online/jig/docs     Status of this build: CODE-STATUS.md");

    app.add_flag("-v,--verbose", c.verbose, "More detail: -v paths, -vv commands, -vvv probes")->group("Output");
    app.add_flag("-q,--quiet", c.quiet, "Less output: -q one line per step, -qq errors only")->group("Output");
    app.add_flag("--terse", c.terse, "One line per step, no reasons")->group("Output");
    app.add_flag("--explain", c.explain, "Print the reasons and rejected alternatives for every decision")->group("Output");
    app.add_flag("--json", c.json, "Newline-delimited JSON events on stdout")->group("Output");
    app.add_option("--color", c.color, "auto, always or never")->check(CLI::IsMember({"auto", "always", "never"}))->group("Output");
    app.add_flag("--offline", c.offline, "Never use the network")->group("Safety");
    app.add_flag("--locked", c.locked, "Fail if jig.lock is missing or out of date")->group("Safety");
    app.add_flag("--frozen", c.frozen, "--locked plus --offline")->group("Safety");
    app.add_flag("-y,--yes", c.yes, "Accept defaults instead of asking")->group("Safety");
    app.add_flag("--no-interactive", c.no_interactive, "Never prompt; missing input is an error")->group("Safety");
    app.add_option("--manifest-path", c.manifest_path, "Use this jig.toml")->group("Project");
    app.add_option("--home", c.home, "Override JIG_HOME")->group("Project");

    // Runs `fn` with settings and a Ui, once, after parsing.
    auto run = [&](auto&& fn) {
        if (!c.home.empty()) ::setenv("JIG_HOME", c.home.c_str(), 1);
        Settings s = make_settings(c);
        Ui ui(s);
        c.rc = fn(s, ui);
        c.ran = true;
    };

    // init
    jig::InitOptions io;
    std::string init_dir = ".";
    auto* init = app.add_subcommand("init", "Create a project: asks for language, standard and compiler, or take them as flags");
    init->add_option("path", init_dir, "Directory to create or use (default: current directory)");
    init->add_option("--name", io.name.emplace(), "Project name (default: directory name)");
    init->add_option("--lang", io.language.emplace(), "c or c++")->check(CLI::IsMember({"c", "c++", "cpp"}));
    init->add_option("--std", io.std.emplace(), "c11 c17 c23 | c++14 c++17 c++20 c++23 c++26");
    init->add_option("--toolchain", io.toolchain.emplace(), "gcc@13, clang@18, clang, gcc");
    init->add_option("--kind", io.kind.emplace(), "exe, static-lib, shared-lib, header-only");
    init->add_option("--vcs", io.vcs.emplace(), "git or none")->check(CLI::IsMember({"git", "none"}));
    init->add_flag("--extensions", io.extensions, "Use the GNU dialect (-std=gnu++23)");
    init->add_flag("--force", io.force, "Write into a directory that is not empty");
    init->callback([&] {
        // CLI11 emplaced empty optionals; clear the ones the user did not set.
        for (auto* o : {&io.name, &io.language, &io.std, &io.toolchain, &io.kind, &io.vcs}) if (o->has_value() && (*o)->empty()) o->reset();
        io.dir = init_dir;
        run([&](const Settings& s, Ui& ui) { return cmd::init(s, ui, io); });
    });

    // toolchain
    auto* tc = app.add_subcommand("toolchain", "List, choose and pin compilers");
    tc->require_subcommand(1);
    tc->add_subcommand("list", "Compilers jig found on this machine")->callback([&] { run([&](auto& s, auto& ui) { return cmd::toolchain_list(s, ui); }); });
    {
        static std::string lang, stdv;
        auto* av = tc->add_subcommand("available", "Which compilers can build a language standard");
        av->add_option("--lang", lang, "c or c++ (default c++)");
        av->add_option("--std", stdv, "Show whether each compiler accepts this standard");
        av->callback([&] {
            run([&](auto& s, auto& ui) {
                return cmd::toolchain_available(s, ui, lang.empty() ? std::nullopt : std::optional<std::string>(lang), stdv.empty() ? std::nullopt : std::optional<std::string>(stdv));
            });
        });
    }
    {
        static std::string tool;
        auto* w = tc->add_subcommand("which", "Print the real path of cc, c++, cmake, ninja ...");
        w->add_option("tool", tool);
        w->callback([&] { run([&](auto& s, auto& ui) { return cmd::toolchain_which(s, ui, tool.empty() ? std::nullopt : std::optional<std::string>(tool)); }); });
    }
    {
        static std::string spec;
        auto* u = tc->add_subcommand("use", "Ask this project for gcc@13, clang@18, ... and re-lock");
        u->add_option("spec", spec)->required();
        u->callback([&] { run([&](auto& s, auto& ui) { return cmd::toolchain_use(s, ui, spec); }); });
    }
    tc->add_subcommand("pin", "Write the exact locked compiler version into jig.toml")->callback([&] { run([&](auto& s, auto& ui) { return cmd::toolchain_pin(s, ui); }); });
    for (const char* name : {"install", "remove", "update"}) {
        static std::vector<std::string> ignored;
        auto* sub = tc->add_subcommand(name, std::string("(not in this build) ") + name + " a downloaded toolchain");
        sub->add_option("spec", ignored, "e.g. llvm@20")->expected(-1);
        sub->callback([&, name] {
            run([&](auto&, auto& ui) {
                return cmd::not_implemented(ui, std::string("`jig toolchain ") + name + "`",
                                            "Downloaded toolchains need the signed index (docs/blueprint/004). For now use `jig toolchain list` and `jig toolchain use`.");
            });
        });
    }

    // lock, sync, env, shell, exec
    {
        static bool update = false, check = false;
        auto* l = app.add_subcommand("lock", "Resolve the toolchain and write jig.lock");
        l->add_flag("--update", update, "Re-resolve even when the lock is current");
        l->add_flag("--check", check, "Only report whether jig.lock is current");
        l->callback([&] { run([&](auto& s, auto& ui) { return cmd::lock(s, ui, update, check); }); });
    }
    {
        static bool check = false;
        auto* sy = app.add_subcommand("sync", "Make .jig/env match jig.lock");
        sy->add_flag("--check", check, "Only report whether the environment is in sync");
        sy->callback([&] { run([&](auto& s, auto& ui) { return cmd::sync(s, ui, check); }); });
    }
    {
        static std::string format = "sh";
        auto* e = app.add_subcommand("env", "Print the environment as sh, fish or json (eval \"$(jig env)\")");
        e->add_option("--format", format, "sh, fish or json")->check(CLI::IsMember({"sh", "fish", "json"}));
        e->callback([&] { run([&](auto& s, auto& ui) { return cmd::env(s, ui, format); }); });
    }
    app.add_subcommand("shell", "Start a shell with the project environment active")->callback([&] { run([&](auto& s, auto& ui) { return cmd::shell(s, ui); }); });
    {
        static std::vector<std::string> argvs;
        auto* ex = app.add_subcommand("exec", "Run any command inside the environment:  jig exec -- cc --version");
        ex->add_option("command", argvs, "Command and arguments")->expected(-1);
        ex->prefix_command();
        ex->callback([&] {
            auto all = argvs;
            for (auto& r : ex->remaining()) all.push_back(r);
            run([&](auto& s, auto& ui) { return cmd::exec(s, ui, all); });
        });
    }

    // build, run, test, fmt, clean
    {
        static std::string profile;
        static bool release = false;
        auto* b = app.add_subcommand("build", "Configure and build");
        b->add_option("-p,--profile", profile, "Profile (default: debug)");
        b->add_flag("--release", release, "Same as --profile release");
        b->callback([&] { run([&](auto& s, auto& ui) { return cmd::build(s, ui, profile_of(profile, release)); }); });
    }
    {
        static std::string profile;
        static bool release = false;
        static std::vector<std::string> args;
        auto* r = app.add_subcommand("run", "Build, then run the executable:  jig run -- <arguments>");
        r->add_option("-p,--profile", profile, "Profile (default: debug)");
        r->add_flag("--release", release, "Same as --profile release");
        r->add_option("args", args, "Arguments for the program (after --)")->expected(-1);
        r->callback([&] { run([&](auto& s, auto& ui) { return cmd::run(s, ui, profile_of(profile, release), args); }); });
    }
    {
        static std::string profile;
        static bool release = false;
        static std::vector<std::string> args;
        auto* t = app.add_subcommand("test", "Build and run the tests (one executable per file in tests/)");
        t->add_option("-p,--profile", profile, "Profile (default: debug)");
        t->add_flag("--release", release, "Same as --profile release");
        t->add_option("args", args, "Extra arguments for ctest (after --)")->expected(-1);
        t->callback([&] { run([&](auto& s, auto& ui) { return cmd::test(s, ui, profile_of(profile, release), args); }); });
    }
    {
        static bool check = false;
        auto* f = app.add_subcommand("fmt", "Format sources with clang-format");
        f->add_flag("--check", check, "Do not change files; fail if formatting differs");
        f->callback([&] { run([&](auto& s, auto& ui) { return cmd::fmt(s, ui, check); }); });
    }
    {
        static bool env_too = false, all = false;
        auto* cl = app.add_subcommand("clean", "Remove build output");
        cl->add_flag("--env", env_too, "Also remove .jig/env");
        cl->add_flag("--all", all, "Remove the whole .jig directory");
        cl->callback([&] { run([&](auto& s, auto& ui) { return cmd::clean(s, ui, env_too, all); }); });
    }

    // doctor, cache, version
    {
        static bool strict = false;
        auto* d = app.add_subcommand("doctor", "Check this machine and project; report the hermeticity level");
        d->add_flag("--strict", strict, "Exit 1 on warnings too");
        d->callback([&] { run([&](auto& s, auto& ui) { return cmd::doctor(s, ui, strict); }); });
    }
    {
        static std::string action = "dir";
        auto* ca = app.add_subcommand("cache", "Inspect the user cache (dir, size, clean)");
        ca->add_option("action", action)->check(CLI::IsMember({"dir", "size", "clean"}));
        ca->callback([&] { run([&](auto& s, auto& ui) { return cmd::cache(s, ui, action); }); });
    }
    app.add_subcommand("version", "Version, build and provider information")->callback([&] { run([&](auto& s, auto& ui) { return cmd::version(s, ui); }); });

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp& e) {
        std::cout << app.help();
        return 0;
    } catch (const CLI::CallForAllHelp& e) {
        std::cout << app.help("", CLI::AppFormatMode::All);
        return 0;
    } catch (const CLI::CallForVersion& e) {
        std::cout << "jig " << JIG_VERSION << "\n";
        return 0;
    } catch (const CLI::ParseError& e) {
        // CLI11 prints sub-command help itself for `jig build --help`; everything else is a usage error.
        int code = app.exit(e);
        return (e.get_exit_code() == 0) ? 0 : (code == 0 ? 2 : 2);
    } catch (const std::exception& e) {
        std::cerr << "x DJ9000  Internal error: " << e.what() << "\n  Please report it: https://github.com/ansgrb/jig/issues\n";
        return 101;
    }
    return c.ran ? c.rc : 0;
}
