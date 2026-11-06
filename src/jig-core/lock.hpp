// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/lock.hpp
// jig.lock (reference/manifest.md S9). Written by jig, never by hand.
// In this build the only provider is `system`, so the lock records exactly which compiler on this
// machine was chosen, with its hash. That is hermeticity level H0: detected and recorded, not provided.
#pragma once

#include <string>
#include <vector>

#include "jig-core/error.hpp"
#include "jig-core/platform.hpp"

namespace jig {

struct LockedToolchain {
    std::string vendor = "system";
    std::string family;       // gcc | clang | apple-clang
    std::string version;      // 13.3.0
    std::string host;         // x86_64-unknown-linux-gnu
    std::string id;           // system-gcc-13.3.0-x86_64-unknown-linux-gnu-9f3a1c2e
    std::string cc;           // path the C driver was found at
    std::string cxx;          // path the C++ driver was found at
    std::string cxx_real;     // symlinks resolved
    std::string sha256;       // of cxx_real
    std::string std_flag;     // the -std= spelling chosen for the project
    std::string hermeticity = "h0";
};

struct LockedTool {
    std::string name;     // cmake | ninja | clang-format
    std::string version;
    std::string path;
    std::string sha256;
};

struct Lock {
    int version = 1;
    std::string generated_by;
    std::string manifest_digest;
    LockedToolchain toolchain;
    std::vector<LockedTool> tools;

    [[nodiscard]] const LockedTool* tool(std::string_view name) const;
};

std::string serialize_lock(const Lock& l);
Result<Lock> load_lock(const fs::path& file);
Result<void> save_lock(const fs::path& file, const Lock& l);
// sha256 of the lock file's bytes; used to tell whether .jig/env was built from this lock.
Result<std::string> lock_digest(const fs::path& file);

}  // namespace jig
