// SPDX-License-Identifier: GPL-3.0-only
// jig-build/build.hpp
// build, run, test, clean, fmt and exec. Only this library and jig-env spawn compilers (001 section 8).
#pragma once

#include <string>
#include <vector>

#include "jig-env/project.hpp"

namespace jig {

struct BuildOutcome {
    fs::path build_dir;
    fs::path artifact;  // empty for header-only projects
    std::string profile;
};

Result<BuildOutcome> build_project(const Prepared& p, Ui& ui, const std::string& profile, bool with_tests);
// Returns the program's own exit status (so `jig run` can pass it on).
Result<int> run_project(const Prepared& p, Ui& ui, const std::string& profile, const std::vector<std::string>& args);
Result<void> test_project(const Prepared& p, Ui& ui, const std::string& profile, const std::vector<std::string>& extra_args);
Result<void> clean_project(const Manifest& m, Ui& ui, bool env, bool all);
Result<void> fmt_project(const Prepared& p, Ui& ui, bool check);
// Runs any command inside the environment, attached to the terminal. Returns its exit status.
int exec_in_env(const Prepared& p, Ui& ui, const std::vector<std::string>& argv);
int shell_in_env(const Prepared& p, Ui& ui);

}  // namespace jig
