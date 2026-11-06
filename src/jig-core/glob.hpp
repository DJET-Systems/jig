// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/glob.hpp
// Source globs: `*`, `?`, `**` (any number of directories) and `{a,b}` alternatives.
// Paths use '/' separators and are relative to the project root.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jig {

std::vector<std::string> expand_braces(std::string_view pattern);
bool glob_match(std::string_view pattern, std::string_view path);

// Walks `root` and returns the matching files (relative, '/'-separated, sorted).
// Skips `.git`, `.jig`, and any directory named in `skip_dirs`.
std::vector<std::string> glob_files(const std::filesystem::path& root,
                                    const std::vector<std::string>& patterns);

}  // namespace jig
