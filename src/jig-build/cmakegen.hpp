// SPDX-License-Identifier: AGPL-3.0-only
// jig-build/cmakegen.hpp
// The `generated` backend (docs/blueprint/007): jig.toml -> .jig/gen/<profile>/CMakeLists.txt.
// Sources are expanded by jig, not by CMake globs, so the generated file lists every file explicitly
// and changes exactly when the project's file set or settings change.
#pragma once

#include <string>
#include <vector>

#include "jig-core/lock.hpp"
#include "jig-core/manifest.hpp"

namespace jig {

struct ProfileFlags {
    std::vector<std::string> compile;   // -O0 -g -fsanitize=...
    std::vector<std::string> link;      // -fsanitize=... -flto
    std::vector<std::string> defines;   // NDEBUG, user defines
    std::string summary;                // "-O0 -g, assertions on"
};

ProfileFlags profile_flags(const Manifest& m, const Lock& l, const ResolvedProfile& p);

struct GeneratedProject {
    std::string cmake;
    std::vector<std::string> sources;       // relative to the project root
    std::vector<std::string> test_sources;
};

Result<GeneratedProject> generate_cmake(const Manifest& m, const Lock& l, const ResolvedProfile& p);

}  // namespace jig
