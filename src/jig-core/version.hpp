// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/version.hpp
// Versions and version specifications (003 section 4).
//
//   "latest"        any version
//   "20" "20.1"     prefix match (major, or major.minor)
//   "20.1.8"        exact
//   "^20" "^20.1"   >=20.0.0,<21.0.0   /  >=20.1.0,<21.0.0
//   "~20.1"         >=20.1.0,<20.2.0
//   ">=18,<21"      comma separated constraints, all must hold
#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jig {

struct Version {
    int major = 0;
    int minor = -1;  // -1 means "not given"
    int patch = -1;

    [[nodiscard]] std::string str() const;
    [[nodiscard]] Version normalized() const { return {major, minor < 0 ? 0 : minor, patch < 0 ? 0 : patch}; }
    friend bool operator==(const Version& a, const Version& b) {
        return a.normalized().major == b.normalized().major && a.normalized().minor == b.normalized().minor &&
               a.normalized().patch == b.normalized().patch;
    }
    friend std::strong_ordering operator<=>(const Version& a, const Version& b) {
        auto x = a.normalized(), y = b.normalized();
        if (auto c = x.major <=> y.major; c != 0) return c;
        if (auto c = x.minor <=> y.minor; c != 0) return c;
        return x.patch <=> y.patch;
    }
};

// Parses the leading "N", "N.M" or "N.M.P" of `s` and ignores anything after it ("13.3.0-6ubuntu2").
std::optional<Version> parse_version(std::string_view s);

class VersionSpec {
public:
    enum class Op { Prefix, Ge, Gt, Le, Lt };
    struct Constraint {
        Op op;
        Version v;
    };

    static std::optional<VersionSpec> parse(std::string_view text);

    [[nodiscard]] bool matches(const Version& v) const;
    [[nodiscard]] const std::string& text() const { return text_; }
    [[nodiscard]] bool is_latest() const { return constraints_.empty(); }

private:
    std::string text_ = "latest";
    std::vector<Constraint> constraints_;
};

}  // namespace jig
