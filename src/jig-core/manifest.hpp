// SPDX-License-Identifier: GPL-3.0-only
// jig-core/manifest.hpp
// jig.toml (reference/manifest.md). Unknown keys are errors (DJ1003).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "jig-core/error.hpp"
#include "jig-core/platform.hpp"
#include "jig-core/version.hpp"

namespace jig {

struct ProfileSpec {
    std::optional<std::string> inherits;
    std::optional<std::string> opt;   // "0" "1" "2" "3" "s" "z"
    std::optional<bool> debug;
    std::optional<std::string> lto;   // "false" | "thin" | "full"
    std::optional<std::vector<std::string>> sanitizers;
    std::optional<bool> assertions;
    std::vector<std::string> defines;
};

struct ResolvedProfile {
    std::string name;
    std::string opt = "0";
    bool debug = true;
    std::string lto = "false";
    std::vector<std::string> sanitizers;
    bool assertions = true;
    std::vector<std::string> defines;
};

struct Manifest {
    fs::path file;    // absolute path of jig.toml
    fs::path root;    // its directory
    std::string digest;  // sha256 of the file's bytes

    // [project]
    std::string name;
    std::string version = "0.1.0";
    std::string description;
    std::string license;
    std::vector<std::string> authors;
    std::string kind = "exe";   // exe | static-lib | shared-lib | header-only
    std::string language;       // c | c++
    std::string std;            // c11 ... c++26
    bool extensions = false;

    // [toolchain]
    std::string vendor = "llvm";  // llvm | system   (llvm is not available in this build)
    VersionSpec toolchain_version;
    std::optional<std::string> hermeticity;  // h0 .. h3 (minimum)
    std::string tool_cmake = "3";
    std::string tool_ninja = "1";
    std::string sys_cc;   // [toolchain.system] cc
    std::string sys_cxx;  // [toolchain.system] cxx

    // [build]
    std::string backend = "generated";  // generated | cmake
    std::vector<std::string> sources = {"src/**/*.{c,cc,cpp,cxx}"};
    std::vector<std::string> include = {"include", "src"};
    std::vector<std::string> public_include = {"include"};
    std::vector<std::string> defines;
    std::vector<std::string> link;
    std::vector<std::string> cmake_args;
    std::string warnings = "default";  // off | default | strict

    // [profile.*]
    std::map<std::string, ProfileSpec> profiles;

    // [test]
    std::vector<std::string> test_sources = {"tests/**/*.{c,cc,cpp,cxx}"};
    std::vector<std::string> test_args = {"--output-on-failure"};

    // [fmt]
    std::string fmt_style = "file";

    // [env]
    std::string env_inherit = "minimal";  // none | minimal | all
    std::map<std::string, std::string> env_set;
};

Result<fs::path> find_manifest(const fs::path& start);
Result<Manifest> load_manifest(const fs::path& file);
Result<ResolvedProfile> resolve_profile(const Manifest& m, const std::string& name);

bool std_valid_for_language(std::string_view language, std::string_view std);
const std::vector<std::string>& standards_for(std::string_view language);

}  // namespace jig
