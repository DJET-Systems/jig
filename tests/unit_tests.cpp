// SPDX-License-Identifier: GPL-3.0-only
// Unit tests. A tiny harness on purpose (no framework dependency in the slice); Catch2 is the plan (011).
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "jig-build/cmakegen.hpp"
#include "jig-core/glob.hpp"
#include "jig-core/lock.hpp"
#include "jig-core/manifest.hpp"
#include "jig-core/sha256.hpp"
#include "jig-core/tomledit.hpp"
#include "jig-core/version.hpp"
#include "jig-env/toolchain.hpp"
#include "jig-init/templates.hpp"

namespace {

struct Case {
    const char* name;
    std::function<void()> fn;
};
std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}
int g_fail = 0;
const char* g_current = "";

struct Reg {
    Reg(const char* n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define TEST(name)                         \
    static void name();                    \
    static Reg reg_##name(#name, name);    \
    static void name()
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "    FAIL %s:%d  %s  [%s]\n", __FILE__, __LINE__, #cond, g_current); \
            ++g_fail;                                                                     \
        }                                                                                 \
    } while (0)

using namespace jig;

fs::path temp_dir(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("jig-test-") + tag + "-" + std::to_string(::getpid()));
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

void write(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << s;
}

// ---- version -----------------------------------------------------------------------------------

TEST(version_parse) {
    CHECK(parse_version("13.3.0")->major == 13);
    CHECK(parse_version("13.3.0-6ubuntu2")->patch == 0);
    CHECK(parse_version("18")->minor == -1);
    CHECK(!parse_version("x"));
    CHECK(*parse_version("13.3.0") < *parse_version("13.10.0"));
    CHECK(*parse_version("13") == *parse_version("13.0.0"));
}

TEST(version_specs) {
    auto m = [](const char* spec, const char* v) { return VersionSpec::parse(spec)->matches(*parse_version(v)); };
    CHECK(m("latest", "1.2.3"));
    CHECK(m("13", "13.3.0"));
    CHECK(!m("13", "14.1.0"));
    CHECK(m("13.3", "13.3.9"));
    CHECK(!m("13.3", "13.4.0"));
    CHECK(m("13.3.0", "13.3.0"));
    CHECK(!m("13.3.0", "13.3.1"));
    CHECK(m("^13", "13.9.0"));
    CHECK(!m("^13", "14.0.0"));
    CHECK(m("~13.3", "13.3.5"));
    CHECK(!m("~13.3", "13.4.0"));
    CHECK(m(">=12,<15", "14.2.0"));
    CHECK(!m(">=12,<15", "15.0.0"));
    CHECK(m(">=3.20,<5", "3.20.0"));
    CHECK(m(">=3.20,<5", "4.4.3"));
    CHECK(!m(">=3.20,<5", "3.19.9"));
    CHECK(!m(">=3.20,<5", "5.0.0"));
    CHECK(!VersionSpec::parse("banana"));
    CHECK(!VersionSpec::parse(">="));
    CHECK(!VersionSpec::parse(""));
}

// ---- sha256 ------------------------------------------------------------------------------------

TEST(sha256_vectors) {
    CHECK(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    std::string million(1000000, 'a');
    CHECK(sha256_hex(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

// ---- glob --------------------------------------------------------------------------------------

TEST(glob_matching) {
    CHECK(glob_match("src/**/*.cpp", "src/a.cpp"));
    CHECK(glob_match("src/**/*.cpp", "src/x/y/a.cpp"));
    CHECK(!glob_match("src/**/*.cpp", "tests/a.cpp"));
    CHECK(glob_match("src/*.{c,cc,cpp}", "src/main.cc"));
    CHECK(!glob_match("src/*.{c,cc,cpp}", "src/main.h"));
    CHECK(glob_match("**/*.hpp", "a/b/c.hpp"));
    CHECK(glob_match("a?c", "abc"));
    CHECK(!glob_match("src/*.cpp", "src/x/a.cpp"));
    CHECK(expand_braces("a{b,c}d").size() == 2);
    CHECK(expand_braces("{a,b}{1,2}").size() == 4);
}

TEST(glob_files_walks_and_skips) {
    auto d = temp_dir("glob");
    write(d / "src/a.cpp", "");
    write(d / "src/deep/b.cpp", "");
    write(d / ".jig/build/x.cpp", "");
    write(d / "tests/t.cpp", "");
    auto f = glob_files(d, {"src/**/*.cpp"});
    CHECK(f.size() == 2);
    CHECK(f.front() == "src/a.cpp");
    CHECK(glob_files(d, {"**/*.cpp"}).size() == 3);  // .jig is skipped
    fs::remove_all(d);
}

// ---- tomledit ----------------------------------------------------------------------------------

TEST(tomledit_replace_insert_append) {
    std::string t = "# my project\n[project]\nname = \"x\"\n\n[toolchain]\nversion = \"13\"   # pinned\n\n[build]\nwarnings = \"strict\"\n";
    auto a = set_toml_key(t, "toolchain", "version", "\"14\"");
    CHECK(a.find("version = \"14\"") != std::string::npos);
    CHECK(a.find("# my project") != std::string::npos);  // comments survive
    CHECK(a.find("warnings = \"strict\"") != std::string::npos);
    auto b = set_toml_key(t, "toolchain", "vendor", "\"system\"");
    CHECK(b.find("vendor = \"system\"") != std::string::npos);
    CHECK(b.find("vendor") < b.find("[build]"));  // inserted inside its own section
    auto c = set_toml_key(t, "toolchain.system", "cc", "\"gcc\"");
    CHECK(c.find("[toolchain.system]\ncc = \"gcc\"") != std::string::npos);
}

// ---- manifest ----------------------------------------------------------------------------------

Result<Manifest> parse_manifest_text(const std::string& text) {
    auto d = temp_dir("manifest");
    write(d / "jig.toml", text);
    auto r = load_manifest(d / "jig.toml");
    fs::remove_all(d);
    return r;
}

TEST(manifest_valid_minimal) {
    auto m = parse_manifest_text("[project]\nname=\"hello\"\nlanguage=\"c++\"\nstd=\"c++23\"\n");
    CHECK(m.has_value());
    if (m) {
        CHECK(m->name == "hello");
        CHECK(m->kind == "exe");
        CHECK(m->vendor == "llvm");  // spec default
        auto cmake = VersionSpec::parse(m->tool_cmake);
        CHECK(cmake && cmake->matches(*parse_version("4.4.3")));
        CHECK(cmake && !cmake->matches(*parse_version("3.19.9")));
        CHECK(cmake && !cmake->matches(*parse_version("5.0.0")));
        CHECK(m->digest.size() == 64);
    }
}

TEST(manifest_errors_have_stable_codes) {
    auto code = [](const std::string& t) {
        auto r = parse_manifest_text(t);
        return r ? std::string("ok") : r.error().code;
    };
    CHECK(code("[project\n") == "DJ1002");
    CHECK(code("[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\nwarnigns=1\n") == "DJ1003");
    CHECK(code("[project]\nname=1\nlanguage=\"c++\"\nstd=\"c++23\"\n") == "DJ1004");
    CHECK(code("[project]\nname=\"Bad Name\"\nlanguage=\"c++\"\nstd=\"c++23\"\n") == "DJ1005");
    CHECK(code("[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\n[dependencies]\nfmt=\"1\"\n") == "DJ1006");
    CHECK(code("[project]\nname=\"a\"\nlanguage=\"c\"\nstd=\"c++23\"\n") == "DJ1007");
    CHECK(code("[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\n[toolchain]\nversion=\"banana\"\n") == "DJ1008");
    CHECK(code("[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\n[[target]]\nname=\"x\"\n") == "DJ9001");
}

TEST(manifest_did_you_mean) {
    auto r = parse_manifest_text("[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\n[build]\nwarnigns=\"off\"\n");
    CHECK(!r);
    if (!r) CHECK(r.error().help.find("build.warnings") != std::string::npos);
}

TEST(profiles_resolve_with_inheritance) {
    auto m = parse_manifest_text(
        "[project]\nname=\"a\"\nlanguage=\"c++\"\nstd=\"c++23\"\n"
        "[profile.asan]\ninherits=\"debug\"\nsanitizers=[\"address\",\"undefined\"]\n");
    CHECK(m.has_value());
    if (!m) return;
    auto d = resolve_profile(*m, "debug");
    CHECK(d && d->opt == "0" && d->debug && d->assertions);
    auto r = resolve_profile(*m, "release");
    CHECK(r && r->opt == "3" && !r->debug && !r->assertions && r->lto == "thin");
    auto a = resolve_profile(*m, "asan");
    CHECK(a && a->opt == "0" && a->sanitizers.size() == 2);
    auto bad = resolve_profile(*m, "nope");
    CHECK(!bad && bad.error().code == "DJ4101");
}

// ---- std flags ---------------------------------------------------------------------------------

Compiler fake(const char* family, int major, int minor = 0) {
    Compiler c;
    c.family = family;
    c.version = {major, minor, 0};
    return c;
}

TEST(std_flag_table) {
    CHECK(*std_flag(fake("gcc", 13), "c++", "c++23", false) == "c++23");
    CHECK(*std_flag(fake("gcc", 11), "c++", "c++23", false) == "c++2b");
    CHECK(!std_flag(fake("gcc", 13), "c++", "c++26", false));  // verified: g++ 13 rejects -std=c++26
    CHECK(*std_flag(fake("gcc", 14), "c++", "c++26", false) == "c++26");
    CHECK(*std_flag(fake("gcc", 13), "c", "c23", false) == "c2x");   // verified: gcc 13 rejects -std=c23, takes c2x
    CHECK(*std_flag(fake("gcc", 14), "c", "c23", false) == "c23");
    CHECK(*std_flag(fake("clang", 18), "c++", "c++26", false) == "c++26");  // verified with clang++ 18
    CHECK(*std_flag(fake("clang", 18), "c", "c23", false) == "c23");
    CHECK(*std_flag(fake("clang", 15), "c++", "c++23", false) == "c++2b");
    CHECK(*std_flag(fake("gcc", 13), "c++", "c++23", true) == "gnu++23");
    CHECK(*std_flag(fake("gcc", 13), "c", "c17", true) == "gnu17");
    CHECK(!std_flag(fake("gcc", 4), "c++", "c++17", false));
    CHECK(accepted_stds(fake("gcc", 13), "c++").size() == 4);  // 14 17 20 23
}

// ---- lock --------------------------------------------------------------------------------------

TEST(lock_round_trip) {
    Lock l;
    l.generated_by = "jig test";
    l.manifest_digest = std::string(64, 'a');
    l.toolchain.family = "gcc";
    l.toolchain.version = "13.3.0";
    l.toolchain.cc = "/usr/bin/gcc-13";
    l.toolchain.cxx = "/usr/bin/g++-13";
    l.toolchain.cxx_real = "/usr/bin/x86_64-linux-gnu-g++-13";
    l.toolchain.sha256 = std::string(64, 'b');
    l.toolchain.std_flag = "c++23";
    l.tools.push_back({"cmake", "3.28.3", "/usr/bin/cmake", std::string(64, 'c')});
    auto d = temp_dir("lock");
    CHECK(save_lock(d / "jig.lock", l).has_value());
    auto back = load_lock(d / "jig.lock");
    CHECK(back.has_value());
    if (back) {
        CHECK(back->manifest_digest == l.manifest_digest);
        CHECK(back->toolchain.cxx_real == l.toolchain.cxx_real);
        CHECK(back->toolchain.std_flag == "c++23");
        CHECK(back->tool("cmake") && back->tool("cmake")->version == "3.28.3");
    }
    write(d / "bad.lock", "version = 99\n");
    auto bad = load_lock(d / "bad.lock");
    CHECK(!bad && bad.error().code == "DJ1104");
    write(d / "junk.lock", "this is not toml [");
    CHECK(!load_lock(d / "junk.lock") && load_lock(d / "junk.lock").error().code == "DJ1105");
    fs::remove_all(d);
}

// ---- templates and generated CMake -------------------------------------------------------------

TEST(every_template_manifest_validates) {
    for (const char* lang : {"c++", "c"})
        for (const char* kind : {"exe", "static-lib", "shared-lib", "header-only"}) {
            TemplateParams p;
            p.name = "demo-app";
            p.language = lang;
            p.std = std::string(lang) == "c" ? "c17" : "c++20";
            p.kind = kind;
            p.family = "gcc";
            p.toolchain_version = "13";
            auto r = parse_manifest_text(manifest_text(p));
            CHECK(r.has_value());
            if (!r) std::fprintf(stderr, "      %s/%s: %s\n", lang, kind, r.error().summary.c_str());
            CHECK(!project_files(p).empty());
        }
}

TEST(generated_cmake_is_deterministic_and_explicit) {
    auto d = temp_dir("gen");
    TemplateParams p;
    p.name = "hello";
    p.language = "c++";
    p.std = "c++23";
    p.kind = "exe";
    p.family = "gcc";
    p.toolchain_version = "13";
    for (auto& [rel, content] : project_files(p)) write(d / rel, content);
    auto m = load_manifest(d / "jig.toml");
    CHECK(m.has_value());
    if (!m) return;
    Lock l;
    l.toolchain.family = "gcc";
    l.toolchain.version = "13.3.0";
    l.toolchain.std_flag = "c++23";
    auto prof = resolve_profile(*m, "debug");
    auto g1 = generate_cmake(*m, l, *prof);
    auto g2 = generate_cmake(*m, l, *prof);
    CHECK(g1 && g2 && g1->cmake == g2->cmake);
    if (g1) {
        CHECK(g1->cmake.find("add_executable(hello") != std::string::npos);
        CHECK(g1->cmake.find("src/main.cpp") != std::string::npos);
        CHECK(g1->cmake.find("-std=c++23") != std::string::npos);
        CHECK(g1->cmake.find("\"-O0\"") != std::string::npos);
        CHECK(g1->cmake.find("add_test(NAME test_smoke") != std::string::npos);
        CHECK(g1->cmake.find("GLOB") == std::string::npos);  // no CMake globbing
    }
    auto rel = resolve_profile(*m, "release");
    auto g3 = generate_cmake(*m, l, *rel);
    CHECK(g3 && g3->cmake.find("NDEBUG") != std::string::npos);
    CHECK(g3 && g3->cmake.find("-flto=thin") == std::string::npos);  // gcc has no thin LTO
    CHECK(g3 && g3->cmake.find("\"-flto\"") != std::string::npos);
    fs::remove_all(d);
}

TEST(no_sources_is_a_clear_error) {
    auto d = temp_dir("nosrc");
    write(d / "jig.toml", "[project]\nname=\"x\"\nlanguage=\"c++\"\nstd=\"c++20\"\n");
    auto m = load_manifest(d / "jig.toml");
    Lock l;
    l.toolchain.family = "gcc";
    l.toolchain.std_flag = "c++20";
    auto r = generate_cmake(*m, l, *resolve_profile(*m, "debug"));
    CHECK(!r && r.error().code == "DJ4008");
    fs::remove_all(d);
}

}  // namespace

int main() {
    int run = 0;
    for (auto& c : cases()) {
        int before = g_fail;
        g_current = c.name;
        c.fn();
        ++run;
        std::printf("  %s  %s\n", g_fail == before ? "ok  " : "FAIL", c.name);
    }
    std::printf("\n%d test(s), %d failed check(s)\n", run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
