// SPDX-License-Identifier: GPL-3.0-only
// jig-env/env.hpp
// The project environment: .jig/env (docs/blueprint/005).
//
//   .jig/env/bin/        shims: cc, c++, gcc/g++ or clang/clang++, cmake, ninja, clang-format
//   .jig/env/toolchain.cmake
//   .jig/env/activate    source it in sh, bash or zsh
//   .jig/env/env.json    the variables jig sets
//   .jig/env/shims.json  shim name -> real path
//   .jig/env/state.json  which jig.lock this environment was built from
//
// `.jig/` can be deleted at any time; `jig sync` rebuilds it from jig.lock.
#pragma once

#include <string>
#include <vector>

#include "jig-core/lock.hpp"
#include "jig-core/manifest.hpp"
#include "jig-ui/ui.hpp"

namespace jig {

fs::path jig_dir(const Manifest& m);
fs::path env_dir(const Manifest& m);
fs::path lock_file(const Manifest& m);

// Builds .jig/env from the lock. The previous environment is kept if anything fails (DJ3102).
Result<void> sync_env(const Manifest& m, const Lock& l, Ui& ui);
bool env_in_sync(const Manifest& m);

// The environment for child processes: the machine's environment with leaking variables removed
// (per `[env] inherit`), the shims first on PATH, CC/CXX/CMAKE_* set, then `[env] set` applied.
struct ChildEnv {
    EnvMap vars;
    std::vector<std::string> dropped;  // variables that were present and removed
    std::vector<std::string> set;      // variables jig set or changed
};
ChildEnv make_child_env(const Manifest& m, const Lock& l);

// `jig env --format sh|fish|json`
std::string render_env(const Manifest& m, const Lock& l, const std::string& format);

}  // namespace jig
