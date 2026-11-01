// SPDX-License-Identifier: GPL-3.0-only
// jig-init/templates.hpp
// File contents for `jig init` (docs/blueprint/006). Templates are plain functions here so they can be
// unit tested; a template directory with substitution is planned for a later release.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace jig {

struct TemplateParams {
    std::string name;       // my-lib
    std::string version = "0.1.0";
    std::string language;   // c | c++
    std::string std;        // c++23
    std::string kind;       // exe | static-lib | shared-lib | header-only
    std::string vendor = "system";
    std::string family;     // gcc | clang  (for [toolchain.system])
    std::string toolchain_version;  // "13"
};

using FileList = std::vector<std::pair<std::string, std::string>>;  // relative path -> content

std::string identifier_of(const std::string& project_name);  // my-lib -> my_lib
std::string manifest_text(const TemplateParams& p);
FileList project_files(const TemplateParams& p);

}  // namespace jig
