// SPDX-License-Identifier: AGPL-3.0-only
// jig-env/project.hpp
// "Get this project ready": find jig.toml, make sure jig.lock is current, make sure .jig/env matches it.
// Every command that builds or runs something starts here.
#pragma once

#include "jig-core/lock.hpp"
#include "jig-core/manifest.hpp"
#include "jig-core/settings.hpp"
#include "jig-env/env.hpp"
#include "jig-env/toolchain.hpp"
#include "jig-ui/ui.hpp"

namespace jig {

struct Prepared {
    Manifest manifest;
    Lock lock;
    ChildEnv env;
};

Result<Manifest> open_manifest(const Settings& s, Ui& ui, bool narrate = true);

// Fresh = written for this manifest and the locked compiler still exists.
bool lock_is_fresh(const Manifest& m, const Lock& l);

// Re-resolves the toolchain and writes jig.lock.
Result<Lock> write_new_lock(const Manifest& m, Ui& ui);

// `force_lock` re-resolves even when the lock is fresh (`jig lock --update`).
Result<Prepared> prepare(const Settings& s, Ui& ui, bool force_lock = false);

}  // namespace jig
