// SPDX-License-Identifier: AGPL-3.0-only
#include "jig-init/templates.hpp"

#include <algorithm>

namespace jig {

std::string identifier_of(const std::string& n) {
    std::string s = n;
    std::replace(s.begin(), s.end(), '-', '_');
    return s;
}

namespace {

std::string cplusplus_floor(const std::string& std) {
    if (std == "c++14") return "201402L";
    if (std == "c++17") return "201703L";
    if (std == "c++20") return "202002L";
    if (std == "c++23") return "202100L";  // compilers report 202100L (GCC, Clang) or 202302L
    return "202400L";                      // c++26
}

}  // namespace

std::string manifest_text(const TemplateParams& p) {
    const bool cxx = p.language == "c++";
    std::string t;
    t += "[project]\n";
    t += "name     = \"" + p.name + "\"\n";
    t += "version  = \"" + p.version + "\"\n";
    t += "kind     = \"" + p.kind + "\"\n";
    t += "language = \"" + p.language + "\"\n";
    t += "std      = \"" + p.std + "\"\n\n";
    t += "[toolchain]\n";
    t += "vendor  = \"" + p.vendor + "\"\n";
    t += "version = \"" + (p.toolchain_version.empty() ? std::string("latest") : p.toolchain_version) + "\"\n\n";
    if (p.vendor == "system" && !p.family.empty()) {
        t += "[toolchain.system]\n";
        t += p.family == "gcc" ? "cc  = \"gcc\"\ncxx = \"g++\"\n\n" : "cc  = \"clang\"\ncxx = \"clang++\"\n\n";
    }
    t += "[build]\n";
    t += "warnings = \"default\"\n\n";
    t += "[test]\n";
    t += std::string("sources = [\"tests/**/*.") + (cxx ? "cpp" : "c") + "\"]\n";
    return t;
}

FileList project_files(const TemplateParams& p) {
    const bool cxx = p.language == "c++";
    const std::string id = identifier_of(p.name);
    const std::string src_ext = cxx ? "cpp" : "c";
    const std::string hdr_ext = cxx ? "hpp" : "h";
    FileList f;
    f.emplace_back("jig.toml", manifest_text(p));
    f.emplace_back(".gitignore", "# jig\n.jig/\ncompile_commands.json\n");

    std::string readme = "# " + p.name + "\n\nA " + (cxx ? "C++" : "C") + " project managed with [jig](https://djet.online/jig), the native C/C++ toolchain.\n\n```sh\njig build        # configure and build\n";
    if (p.kind == "exe") readme += "jig run          # build and run\n";
    readme += "jig test         # build and run the tests\njig doctor       # check the environment\n```\n";
    f.emplace_back("README.md", readme);

    if (p.kind == "exe") {
        if (cxx) {
            f.emplace_back("src/main.cpp",
                           "#include <iostream>\n\nint main() {\n    std::cout << \"Hello from " + p.name + "!\\n\";\n    return 0;\n}\n");
            f.emplace_back("tests/test_smoke.cpp",
                           "// Executables cannot be linked into tests, so this one checks the language level jig.toml promises.\n"
                           "#if __cplusplus < " + cplusplus_floor(p.std) + "\n#error \"project.std is " + p.std + " but the compiler is not in that mode\"\n#endif\n\nint main() { return 0; }\n");
        } else {
            f.emplace_back("src/main.c", "#include <stdio.h>\n\nint main(void) {\n    printf(\"Hello from " + p.name + "!\\n\");\n    return 0;\n}\n");
            f.emplace_back("tests/test_smoke.c", "int main(void) { return 0; }\n");
        }
        return f;
    }

    // Libraries.
    const std::string hdr = "include/" + p.name + "/" + p.name + "." + hdr_ext;
    if (cxx) {
        if (p.kind == "header-only") {
            f.emplace_back(hdr, "#pragma once\n\nnamespace " + id + " {\n\ninline int add(int a, int b) { return a + b; }\n\n}  // namespace " + id + "\n");
        } else {
            f.emplace_back(hdr, "#pragma once\n\nnamespace " + id + " {\n\nint add(int a, int b);\n\n}  // namespace " + id + "\n");
            f.emplace_back("src/" + p.name + ".cpp", "#include \"" + p.name + "/" + p.name + ".hpp\"\n\nnamespace " + id + " {\n\nint add(int a, int b) { return a + b; }\n\n}  // namespace " + id + "\n");
        }
        f.emplace_back("tests/test_" + id + ".cpp",
                       "#include \"" + p.name + "/" + p.name + ".hpp\"\n\nint main() {\n    if (" + id + "::add(2, 3) != 5) return 1;\n    return 0;\n}\n");
    } else {
        if (p.kind == "header-only") {
            f.emplace_back(hdr, "#ifndef " + id + "_H\n#define " + id + "_H\n\nstatic inline int " + id + "_add(int a, int b) { return a + b; }\n\n#endif\n");
        } else {
            f.emplace_back(hdr, "#ifndef " + id + "_H\n#define " + id + "_H\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\nint " + id + "_add(int a, int b);\n\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n");
            f.emplace_back("src/" + p.name + ".c", "#include \"" + p.name + "/" + p.name + ".h\"\n\nint " + id + "_add(int a, int b) { return a + b; }\n");
        }
        f.emplace_back("tests/test_" + id + ".c",
                       "#include \"" + p.name + "/" + p.name + ".h\"\n\nint main(void) {\n    if (" + id + "_add(2, 3) != 5) return 1;\n    return 0;\n}\n");
    }
    return f;
}

}  // namespace jig
