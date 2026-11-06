// SPDX-License-Identifier: AGPL-3.0-only
// jig-init/init.hpp
// `jig init`: ask (or be told) language, standard, compiler and kind; write the project; lock and sync it.
#pragma once

#include <optional>
#include <string>

#include "jig-core/settings.hpp"
#include "jig-env/project.hpp"

namespace jig {

struct InitOptions {
    fs::path dir = ".";
    std::optional<std::string> name;
    std::optional<std::string> language;   // c | c++
    std::optional<std::string> std;
    std::optional<std::string> toolchain;  // gcc@13, clang@18, clang
    std::optional<std::string> kind;
    std::optional<std::string> vcs;        // git | none
    bool extensions = false;
    bool force = false;
};

Result<void> run_init(const Settings& s, Ui& ui, const InitOptions& opts);

}  // namespace jig
