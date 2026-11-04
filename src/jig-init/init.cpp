// SPDX-License-Identifier: GPL-3.0-only
#include "jig-init/init.hpp"

#include <algorithm>
#include <iostream>
#include <regex>
#include <set>

#include "jig-core/process.hpp"
#include "jig-init/templates.hpp"

namespace jig {

namespace {

struct Choice {
    std::string value;
    std::string label;
    std::string note;
    std::string disabled;  // non-empty: shown but not selectable, with the reason
};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// A numbered list on stderr, an answer on stdin. Enter takes the default.
Result<std::string> ask(Ui& ui, const std::string& title, const std::string& explain, const std::vector<Choice>& choices, std::size_t default_index) {
    std::string out = "\n  ? " + title + "\n";
    if (!explain.empty()) out += ui.dim("    " + explain) + "\n";
    for (std::size_t i = 0; i < choices.size(); ++i) {
        const auto& c = choices[i];
        std::string line = "    " + std::to_string(i + 1) + ") " + c.label;
        if (!c.note.empty()) line += ui.dim("   " + c.note);
        if (!c.disabled.empty()) line = ui.dim(line + "   [unavailable: " + c.disabled + "]");
        if (i == default_index) line += ui.dim("   (default)");
        out += line + "\n";
    }
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::fputs((out + "  choose [" + std::to_string(default_index + 1) + "]: ").c_str(), stderr);
        out.clear();
        std::string line;
        if (!std::getline(std::cin, line)) return choices[default_index].value;
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t\r") + 1 > line.size() ? line.size() : line.find_last_not_of(" \t\r") + 1);
        if (line.empty()) return choices[default_index].value;
        std::size_t pick = choices.size();
        if (std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isdigit(c); })) {
            auto n = static_cast<std::size_t>(std::stoul(line));
            if (n >= 1 && n <= choices.size()) pick = n - 1;
        } else {
            for (std::size_t i = 0; i < choices.size(); ++i)
                if (lower(choices[i].value) == lower(line) || lower(choices[i].label) == lower(line)) pick = i;
        }
        if (pick == choices.size()) {
            std::fputs("  Please enter a number from the list.\n", stderr);
            continue;
        }
        if (!choices[pick].disabled.empty()) {
            std::fputs(("  " + choices[pick].label + " is unavailable here: " + choices[pick].disabled + "\n").c_str(), stderr);
            continue;
        }
        return choices[pick].value;
    }
    return fail("DJ6003", exit_code::usage, "No valid answer was given", "Pass the choice as a flag, for example `--lang c++ --std c++23`.");
}

bool in_git_repo(const fs::path& dir) {
    std::error_code ec;
    for (auto d = fs::absolute(dir, ec); !d.empty(); d = d.parent_path()) {
        if (fs::exists(d / ".git", ec)) return true;
        if (d == d.root_path() || d == d.parent_path()) break;
    }
    return false;
}

}  // namespace

Result<void> run_init(const Settings& s, Ui& ui, const InitOptions& o) {
    std::error_code ec;
    const bool can_ask = s.interactive && stdin_is_tty() && !s.yes && !s.json;
    auto dir = fs::absolute(o.dir, ec);
    if (ec)
        return fail("DJ9000", exit_code::failure, "cannot resolve project directory " + o.dir.string(), {},
                    {ec.message()});
    dir = dir.lexically_normal();
    if (dir.filename().empty()) dir = dir.parent_path();

    // 1. Name and directory.
    std::string name = o.name ? *o.name : sanitized_dir_name(dir);
    static const std::regex name_re("^[a-z][a-z0-9_-]{0,63}$");
    if (!std::regex_match(name, name_re))
        return fail("DJ6002", exit_code::usage, "Invalid project name \"" + name + "\"", "Use [a-z][a-z0-9_-]{0,63}, or pass `--name`.");

    const bool dir_exists = fs::exists(dir, ec);
    if (ec)
        return fail("DJ9000", exit_code::failure, "cannot inspect project path " + dir.string(), {}, {ec.message()});
    if (dir_exists) {
        if (!fs::is_directory(dir, ec)) {
            if (ec)
                return fail("DJ9000", exit_code::failure, "cannot inspect project path " + dir.string(), {},
                            {ec.message()});
            return fail("DJ6001", exit_code::failure, "The path is not a directory: " + dir.string(),
                        "Choose a directory path, or move the existing file.");
        }
        if (ec)
            return fail("DJ9000", exit_code::failure, "cannot inspect project path " + dir.string(), {}, {ec.message()});

        bool empty = true;
        fs::directory_iterator entry(dir, ec), end;
        if (ec)
            return fail("DJ9000", exit_code::failure, "cannot read project directory " + dir.string(), {}, {ec.message()});
        for (; entry != end; entry.increment(ec)) {
            if (ec)
                return fail("DJ9000", exit_code::failure, "cannot read project directory " + dir.string(), {},
                            {ec.message()});
            auto& e = *entry;
            if (e.path().filename() != ".git") empty = false;
        }
        if (ec)
            return fail("DJ9000", exit_code::failure, "cannot read project directory " + dir.string(), {}, {ec.message()});
        if (!empty && !o.force)
            return fail("DJ6001", exit_code::failure, "The directory is not empty: " + dir.string(), "Use `--force` to write the project files anyway.");
    }

    // 2. Which compilers can this machine actually use?
    ui.step("Detecting compilers");
    auto detected = detect_system_compilers(ui);
    if (detected.empty())
        return fail("DJ2201", exit_code::toolchain, "No system compiler found on PATH", "Install gcc/g++ or clang/clang++ first, then run `jig init` again.");
    for (const auto& c : detected) ui.fact(c.label(), c.cxx.string());

    // 3. Required answers. Anything missing is asked for; without a terminal it is an error (002 U2).
    std::vector<std::string> missing;
    if (!o.language && !can_ask && !s.yes) missing.push_back("--lang");
    if (!o.std && !can_ask && !s.yes) missing.push_back("--std");
    if (!o.toolchain && !can_ask && !s.yes) missing.push_back("--toolchain");
    if (!missing.empty()) {
        std::string list;
        for (auto& m : missing) list += (list.empty() ? "" : ", ") + m;
        return fail("DJ6003", exit_code::usage, "Not interactive, and these flags are missing: " + list,
                    "Example: jig init --lang c++ --std c++23 --toolchain gcc@" + std::to_string(detected.front().version.major) + "   (or pass -y to accept defaults)");
    }

    if (can_ask) std::fputs(("\n  jig " JIG_VERSION "  ·  new project \"" + name + "\"\n").c_str(), stderr);

    // Language.
    std::string language;
    if (o.language) {
        language = *o.language == "cpp" || *o.language == "cxx" ? "c++" : *o.language;
        if (language != "c" && language != "c++") return fail("DJ6003", exit_code::usage, "--lang must be `c` or `c++`, got \"" + *o.language + "\"");
    } else if (can_ask) {
        JIG_TRY(l, ask(ui, "Language", "C++ projects can also contain C files.", {{"c++", "C++", "", ""}, {"c", "C", "", ""}}, 0));
        language = l;
    } else {
        language = "c++";
    }

    // Standard: only what at least one detected compiler can build is selectable.
    std::set<std::string> available;
    std::string newest_any;
    for (const auto& c : detected)
        for (const auto& st : accepted_stds(c, language)) available.insert(st);
    std::string std;
    if (o.std) {
        std = *o.std;
        if (!std_valid_for_language(language, std))
            return fail("DJ1007", exit_code::project, "std \"" + std + "\" is not valid for language \"" + language + "\"");
        if (!available.count(std))
            return fail("DJ2102", exit_code::toolchain, "No detected compiler accepts " + std, "Use a newer compiler or a lower standard.",
                        {"Detected: " + [&] {
                            std::string x;
                            for (auto& c : detected) x += (x.empty() ? "" : ", ") + c.label();
                            return x;
                        }()});
    } else {
        std::vector<Choice> choices;
        std::size_t def = 0;
        const auto& all = standards_for(language);
        for (std::size_t i = 0; i < all.size(); ++i) {
            Choice c{all[i], all[i], "", available.count(all[i]) ? "" : "no detected compiler accepts it"};
            if (c.disabled.empty()) {
                // default: the newest standard that is not the newest experimental one
                if (all[i] != "c++26" && all[i] != "c23") def = i;
            }
            choices.push_back(c);
        }
        if (can_ask) {
            JIG_TRY(v, ask(ui, "Language standard", "Only standards your compilers can build are selectable.", choices, def));
            std = v;
        } else {
            std = choices[def].value;
            ui.decision("std", std, {"default with -y: newest stable standard a detected compiler accepts"});
        }
    }

    // Compiler.
    std::vector<const Compiler*> usable;
    for (const auto& c : detected)
        if (std_flag(c, language, std, o.extensions)) usable.push_back(&c);
    std::sort(usable.begin(), usable.end(), [](const Compiler* a, const Compiler* b) { return a->version > b->version; });
    const Compiler* chosen = nullptr;
    if (o.toolchain) {
        auto spec = parse_toolchain_spec(*o.toolchain);
        if (!spec) return fail("DJ6003", exit_code::usage, "Cannot read --toolchain \"" + *o.toolchain + "\"", "Examples: gcc@13, clang@18, clang, gcc");
        auto vs = spec->version.empty() ? VersionSpec::parse("latest") : VersionSpec::parse(spec->version);
        for (auto* c : usable) {
            bool fam = spec->family.empty() || (spec->family == "clang" ? (c->family == "clang" || c->family == "apple-clang") : c->family == "gcc");
            if (fam && vs->matches(c->version)) {
                chosen = c;
                break;
            }
        }
        if (!chosen) {
            std::vector<std::string> det;
            for (auto* c : usable) det.push_back("  " + c->label() + "  (" + c->cxx.string() + ")");
            return fail("DJ2101", exit_code::toolchain, "No detected compiler matches \"" + *o.toolchain + "\" and accepts " + std,
                        "Pick one from the list, or use a lower standard.", det.empty() ? std::vector<std::string>{"No detected compiler accepts " + std} : det);
        }
    } else if (can_ask) {
        std::vector<Choice> choices;
        for (auto* c : usable) choices.push_back({c->family + "@" + std::to_string(c->version.major), c->label(), c->cxx.string(), ""});
        JIG_TRY(v, ask(ui, "Compiler", "Pinned in jig.toml; jig.lock records the exact binary.", choices, 0));
        for (auto* c : usable)
            if (c->family + "@" + std::to_string(c->version.major) == v) chosen = c;
    } else {
        chosen = usable.front();
        ui.decision("compiler", chosen->label(), {"default with -y: highest version that accepts " + std});
    }

    // Kind.
    std::string kind = o.kind.value_or("exe");
    if (!o.kind && can_ask) {
        JIG_TRY(v, ask(ui, "Project kind", "", {{"exe", "executable", "", ""}, {"static-lib", "static library", "", ""}, {"shared-lib", "shared library", "", ""}, {"header-only", "header-only library", "", ""}}, 0));
        kind = v;
    }
    if (kind != "exe" && kind != "static-lib" && kind != "shared-lib" && kind != "header-only")
        return fail("DJ6003", exit_code::usage, "--kind must be exe, static-lib, shared-lib or header-only, got \"" + kind + "\"");

    // Version control.
    std::string vcs = o.vcs.value_or("");
    bool git_available = which("git").has_value();
    if (vcs.empty()) {
        if (in_git_repo(dir)) vcs = "none";
        else if (can_ask && git_available) {
            JIG_TRY(v, ask(ui, "Initialise a git repository", "", {{"git", "yes", "", ""}, {"none", "no", "", ""}}, 0));
            vcs = v;
        } else vcs = git_available ? "git" : "none";
    }
    if (vcs != "git" && vcs != "none") return fail("DJ6003", exit_code::usage, "--vcs must be git or none");

    // 4. Write the project.
    TemplateParams tp;
    tp.name = name;
    tp.language = language;
    tp.std = std;
    tp.kind = kind;
    tp.vendor = "system";
    tp.family = chosen->family == "gcc" ? "gcc" : "clang";
    tp.toolchain_version = std::to_string(chosen->version.major);
    auto files = project_files(tp);
    if (o.extensions) {
        // keep the template simple; an extensions flag is a one-line manifest addition
        for (auto& [path, content] : files)
            if (path == "jig.toml") content = content.replace(content.find("std      ="), 0, "extensions = true\n") ;
    }

    ui.step("Creating project");
    ui.decision("project", name, {language + " " + std + ", " + kind + ", " + chosen->label()});
    fs::create_directories(dir, ec);
    if (ec)
        return fail("DJ9000", exit_code::failure, "cannot create directory " + dir.string(), {}, {ec.message()});
    for (const auto& [rel, content] : files) {
        auto p = dir / rel;
        if (fs::exists(p) && !o.force) return fail("DJ6001", exit_code::failure, "Refusing to overwrite " + rel, "Use `--force`.");
        JIG_TRY_VOID(write_file_atomic(p, content));
        ui.fact("wrote", rel);
    }
    if (vcs == "git") {
        if (!git_available) ui.warn("DJ6005", "git was requested but not found", "Install git or use `--vcs none`.");
        else {
            auto r = run_capture({"git", "init", "-q"}, current_env(), dir);
            if (r.spawned && r.exit_status == 0) ui.fact("git", "initialised an empty repository");
            else ui.warn("DJ6005", "git init failed", r.output);
        }
    }
    ui.done("created " + name);

    // 5. Resolve, lock and sync, so the project builds right away. Shows the compiler decision.
    Settings ps = s;
    ps.manifest_path = dir / "jig.toml";
    auto prepared = prepare(ps, ui);
    if (!prepared) {
        // The files are written; report the toolchain problem without removing them.
        return jig::unexpected<jig::Error>(prepared.error());
    }
    auto rel = fs::relative(dir, fs::current_path(), ec);
    std::string cd = (rel.empty() || rel == ".") ? "" : "cd " + rel.string() + " && ";
    ui.hint(cd + (kind == "exe" ? "jig run" : "jig test") + "   ·   jig build   ·   jig doctor");
    return {};
}

}  // namespace jig
