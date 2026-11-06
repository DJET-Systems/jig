// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/platform.hpp
// Host facts, environment access, directories and filesystem helpers.
// v0.1 slice: POSIX only (Linux, macOS). Windows is planned (see docs/blueprint/005).
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "jig-core/error.hpp"

namespace jig {

namespace fs = std::filesystem;

using EnvMap = std::map<std::string, std::string>;

struct HostInfo {
    std::string os;      // "linux" | "macos"
    std::string arch;    // "x86_64" | "aarch64"
    std::string triple;  // "x86_64-unknown-linux-gnu"
};

HostInfo host_info();
EnvMap current_env();
std::optional<std::string> getenv_str(const char* name);

bool stdout_is_tty();
bool stderr_is_tty();
bool stdin_is_tty();
bool locale_is_utf8();

// JIG_HOME, else $XDG_DATA_HOME/jig, else ~/.local/share/jig (macOS: ~/Library/Application Support/jig).
fs::path jig_home();

// Search PATH (or `path_override`) for an executable.
std::optional<fs::path> which(std::string_view name, const std::optional<std::string>& path_override = std::nullopt);
std::vector<fs::path> path_dirs(const std::optional<std::string>& path_override = std::nullopt);

Result<std::string> read_file(const fs::path& p);
// Writes through a temporary file in the same directory, then renames (atomic on POSIX).
Result<void> write_file_atomic(const fs::path& p, std::string_view content, bool executable = false);

// The project directory name, used as the default project name.
std::string sanitized_dir_name(const fs::path& dir);

}  // namespace jig
