// SPDX-License-Identifier: GPL-3.0-only
#include "jig-env/project.hpp"

namespace jig {

Result<Manifest> open_manifest(const Settings& s, Ui& ui, bool narrate) {
    fs::path start = s.manifest_path ? *s.manifest_path : fs::current_path();
    JIG_TRY(file, find_manifest(start));
    JIG_TRY(m, load_manifest(file));
    if (narrate) {
        ui.fact("jig.toml", m.name + " " + m.version + "   " + m.std + "   " + m.kind);
        ui.info("manifest", m.file.string());
    }
    return std::move(m);
}

bool lock_is_fresh(const Manifest& m, const Lock& l) {
    return l.manifest_digest == m.digest && fs::exists(l.toolchain.cxx_real);
}

Result<Lock> write_new_lock(const Manifest& m, Ui& ui) {
    ui.step("Resolving toolchain");
    auto detected = detect_system_compilers(ui);
    JIG_TRY(res, resolve_toolchain(m, detected, ui));
    JIG_TRY(lock, make_lock(m, res));
    JIG_TRY_VOID(save_lock(lock_file(m), lock));
    ui.done("jig.lock written (" + lock.toolchain.family + " " + lock.toolchain.version + ", hermeticity " + lock.toolchain.hermeticity + ")");
    return std::move(lock);
}

Result<Prepared> prepare(const Settings& s, Ui& ui, bool force_lock) {
    ui.step("Reading project");
    JIG_TRY(m, open_manifest(s, ui));

    std::optional<Lock> lock;
    bool exists = fs::exists(lock_file(m));
    if (exists) {
        auto loaded = load_lock(lock_file(m));
        if (!loaded) return jig::unexpected<jig::Error>(loaded.error());
        lock = std::move(*loaded);
    }

    bool fresh = lock && lock_is_fresh(m, *lock);
    if (fresh && !force_lock) {
        std::string tools;
        for (const auto& t : lock->tools)
            if (t.name == "cmake" || t.name == "ninja") tools += " · " + t.name + " " + t.version;
        ui.fact("jig.lock", "current (" + lock->toolchain.family + " " + lock->toolchain.version + tools + ")");
    } else {
        if (s.locked || s.frozen) {
            if (!exists)
                return fail("DJ1102", exit_code::project, "jig.lock is missing and --locked/--frozen was given", "Run `jig lock` and commit the file.");
            return fail("DJ1103", exit_code::project, "jig.lock is stale and --locked/--frozen was given", "Run `jig lock`.",
                        {"jig.toml changed after jig.lock was written, or the locked compiler is gone."});
        }
        ui.fact("jig.lock", !exists ? "missing" : (force_lock ? "re-resolving (--update)" : "stale"));
        auto nl = write_new_lock(m, ui);
        if (!nl) return jig::unexpected<jig::Error>(nl.error());
        lock = std::move(*nl);
    }

    if (!env_in_sync(m)) {
        ui.step("Syncing environment");
        JIG_TRY_VOID(sync_env(m, *lock, ui));
        ui.done(".jig/env built from jig.lock");
    } else {
        ui.fact("environment", ".jig/env is in sync with jig.lock");
    }

    Prepared p{std::move(m), std::move(*lock), {}};
    p.env = make_child_env(p.manifest, p.lock);
    if (!p.env.dropped.empty() && p.manifest.env_inherit != "all") {
        std::string names;
        for (std::size_t i = 0; i < p.env.dropped.size() && i < 6; ++i) names += (i ? ", " : "") + p.env.dropped[i];
        if (p.env.dropped.size() > 6) names += ", ...";
        ui.fact("env hygiene", "dropped " + std::to_string(p.env.dropped.size()) + " variable(s): " + names);
    }
    return p;
}

}  // namespace jig
