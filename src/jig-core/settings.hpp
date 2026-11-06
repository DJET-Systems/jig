// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/settings.hpp
// Resolved global settings (002 section 3): flags first, then environment variables, then defaults.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace jig {

struct Settings {
    int verbosity = 2;              // 0 quiet .. 5 trace (002 section 5)
    bool explain = false;
    bool json = false;
    std::string color = "auto";     // auto | always | never
    bool offline = false;
    bool locked = false;
    bool frozen = false;
    bool yes = false;
    bool interactive = true;        // false once --no-interactive, or when stdin is not a terminal
    std::optional<std::filesystem::path> manifest_path;
};

}  // namespace jig
