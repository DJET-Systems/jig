// SPDX-License-Identifier: GPL-3.0-only
#include "jig-core/lock.hpp"

#include <toml.hpp>

#include "jig-core/sha256.hpp"
#include "jig-core/tomledit.hpp"

namespace jig {

const LockedTool* Lock::tool(std::string_view name) const {
    for (const auto& t : tools)
        if (t.name == name) return &t;
    return nullptr;
}

std::string serialize_lock(const Lock& l) {
    std::string s;
    s += "# This file is written by jig. Do not edit it by hand; run `jig lock`.\n";
    s += "version = " + std::to_string(l.version) + "\n\n";
    s += "[meta]\n";
    s += "generated-by    = " + toml_quote(l.generated_by) + "\n";
    s += "manifest-digest = " + toml_quote("sha256:" + l.manifest_digest) + "\n\n";
    const auto& t = l.toolchain;
    s += "[[toolchain]]\n";
    s += "vendor      = " + toml_quote(t.vendor) + "\n";
    s += "family      = " + toml_quote(t.family) + "\n";
    s += "version     = " + toml_quote(t.version) + "\n";
    s += "host        = " + toml_quote(t.host) + "\n";
    s += "id          = " + toml_quote(t.id) + "\n";
    s += "cc          = " + toml_quote(t.cc) + "\n";
    s += "cxx         = " + toml_quote(t.cxx) + "\n";
    s += "cxx-real    = " + toml_quote(t.cxx_real) + "\n";
    s += "sha256      = " + toml_quote(t.sha256) + "\n";
    s += "std-flag    = " + toml_quote(t.std_flag) + "\n";
    s += "hermeticity = " + toml_quote(t.hermeticity) + "\n";
    for (const auto& tool : l.tools) {
        s += "\n[[tool]]\n";
        s += "name    = " + toml_quote(tool.name) + "\n";
        s += "version = " + toml_quote(tool.version) + "\n";
        s += "path    = " + toml_quote(tool.path) + "\n";
        s += "sha256  = " + toml_quote(tool.sha256) + "\n";
    }
    return s;
}

Result<Lock> load_lock(const fs::path& file) {
    JIG_TRY(text, read_file(file));
    toml::parse_result parsed = toml::parse(text, file.filename().string());
    if (!parsed)
        return fail("DJ1105", exit_code::project, "jig.lock is malformed", "Delete it and run `jig lock`, or restore it from version control.",
                    {std::string(parsed.error().description())});
    const toml::table& root = parsed.table();
    Lock l;
    l.version = static_cast<int>(root["version"].value_or<int64_t>(0));
    if (l.version > 1)
        return fail("DJ1104", exit_code::project, "jig.lock was written by a newer format (version " + std::to_string(l.version) + ")", "Run `jig self update`.");
    if (l.version < 1) return fail("DJ1105", exit_code::project, "jig.lock has no valid version", "Delete it and run `jig lock`.");

    l.generated_by = root["meta"]["generated-by"].value_or(std::string{});
    auto digest = root["meta"]["manifest-digest"].value_or(std::string{});
    if (digest.rfind("sha256:", 0) == 0) digest.erase(0, 7);
    l.manifest_digest = digest;

    auto* tcs = root["toolchain"].as_array();
    if (!tcs || tcs->empty() || !tcs->front().is_table())
        return fail("DJ1105", exit_code::project, "jig.lock has no [[toolchain]] entry", "Delete it and run `jig lock`.");
    const auto& tc = *tcs->front().as_table();
    auto str = [&](const char* k) { return tc[k].value_or(std::string{}); };
    l.toolchain.vendor = str("vendor");
    l.toolchain.family = str("family");
    l.toolchain.version = str("version");
    l.toolchain.host = str("host");
    l.toolchain.id = str("id");
    l.toolchain.cc = str("cc");
    l.toolchain.cxx = str("cxx");
    l.toolchain.cxx_real = str("cxx-real");
    l.toolchain.sha256 = str("sha256");
    l.toolchain.std_flag = str("std-flag");
    l.toolchain.hermeticity = str("hermeticity");

    if (auto* tools = root["tool"].as_array())
        for (auto&& n : *tools)
            if (auto* t = n.as_table()) {
                LockedTool lt;
                lt.name = (*t)["name"].value_or(std::string{});
                lt.version = (*t)["version"].value_or(std::string{});
                lt.path = (*t)["path"].value_or(std::string{});
                lt.sha256 = (*t)["sha256"].value_or(std::string{});
                l.tools.push_back(std::move(lt));
            }
    return l;
}

Result<void> save_lock(const fs::path& file, const Lock& l) { return write_file_atomic(file, serialize_lock(l)); }

Result<std::string> lock_digest(const fs::path& file) {
    JIG_TRY(text, read_file(file));
    return sha256_hex(text);
}

}  // namespace jig
