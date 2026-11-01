// SPDX-License-Identifier: GPL-3.0-only
// jig-core/process.hpp
// Spawn child processes. Only jig-build and jig-env use this (001 section 8).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "jig-core/platform.hpp"

namespace jig {

struct ProcessResult {
    int exit_status = -1;   // child's exit status, or 128+signal
    bool spawned = false;   // false when fork/exec itself failed
    std::string output;     // combined stdout+stderr when captured
};

// Run with stdout and stderr merged. Every complete line is passed to `on_line` as it arrives
// (use it to stream compiler output). The full text is also returned in `output` when `keep_output`.
ProcessResult run_streaming(const std::vector<std::string>& argv, const fs::path& cwd, const EnvMap& env,
                            const std::function<void(const std::string&)>& on_line, bool keep_output = false);

// Capture combined output, no streaming. Convenient for `--version` probes.
ProcessResult run_capture(const std::vector<std::string>& argv, const EnvMap& env, const fs::path& cwd = {});

// Run attached to the terminal (stdin, stdout, stderr inherited). Returns the exit status, or -1.
int run_inherit(const std::vector<std::string>& argv, const fs::path& cwd, const EnvMap& env);

}  // namespace jig
