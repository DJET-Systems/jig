// SPDX-License-Identifier: GPL-3.0-only
// jig-env/toolchain.hpp
// Toolchain providers. This build has one: `system` (detect compilers on PATH, record them, H0).
// The `llvm` provider (signed index, download, store, H1) is specified in docs/blueprint/004
// and is NOT implemented here; asking for it fails with DJ9001.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "jig-core/lock.hpp"
#include "jig-core/manifest.hpp"
#include "jig-core/version.hpp"
#include "jig-ui/ui.hpp"

namespace jig {

struct Compiler {
    std::string family;        // gcc | clang | apple-clang
    Version version;           // full version, e.g. 13.3.0
    fs::path cc;               // C driver as found on PATH
    fs::path cxx;              // C++ driver as found on PATH
    fs::path cxx_real;         // symlinks resolved
    std::string driver_name;   // "g++-13"

    [[nodiscard]] std::string label() const;  // "gcc 13.3.0"
};

struct ToolInfo {
    std::string name;
    std::string version;
    fs::path path;       // as found on PATH
    fs::path real_path;  // symlinks resolved
};

// Finds gcc/g++ and clang/clang++ pairs on PATH (also versioned names such as g++-13), probes each
// with `--version`, and drops duplicates that resolve to the same binary.
std::vector<Compiler> detect_system_compilers(Ui& ui);

std::optional<ToolInfo> probe_tool(const std::string& name, Ui* ui = nullptr);

// The `-std=` value this compiler needs for a standard, or nullopt when it cannot compile it.
// "c++23" may be spelled "c++2b" by older compilers; `extensions` selects the gnu dialect.
std::optional<std::string> std_flag(const Compiler& c, std::string_view language, std::string_view std, bool extensions);
std::vector<std::string> accepted_stds(const Compiler& c, std::string_view language);

struct Resolution {
    Compiler compiler;
    std::string std_flag;
    std::vector<ToolInfo> tools;  // cmake (required), ninja, clang-format (optional)
};

// Chooses a compiler for the manifest. Emits Decision events that explain the choice.
Result<Resolution> resolve_toolchain(const Manifest& m, const std::vector<Compiler>& detected, Ui& ui);
Result<Lock> make_lock(const Manifest& m, const Resolution& r);

// "gcc@13", "clang@18.1", "clang" -> family + version spec. Used by `toolchain use`.
struct ToolchainSpec {
    std::string family;      // gcc | clang | "" (any)
    std::string version;     // "" or "13", "13.3", ...
};
std::optional<ToolchainSpec> parse_toolchain_spec(std::string_view s);

}  // namespace jig
