// SPDX-License-Identifier: AGPL-3.0-only
#include "jig-core/version.hpp"

#include <cctype>
#include <charconv>

namespace jig {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

bool read_int(std::string_view& s, int& out) {
    std::size_t n = 0;
    while (n < s.size() && std::isdigit(static_cast<unsigned char>(s[n]))) ++n;
    if (n == 0) return false;
    auto r = std::from_chars(s.data(), s.data() + n, out);
    if (r.ec != std::errc{}) return false;
    s.remove_prefix(n);
    return true;
}

// Strict parse used for specs: the whole string must be a version.
std::optional<Version> parse_strict(std::string_view s) {
    s = trim(s);
    Version v;
    if (!read_int(s, v.major)) return std::nullopt;
    if (!s.empty() && s.front() == '.') {
        s.remove_prefix(1);
        if (!read_int(s, v.minor)) return std::nullopt;
        if (!s.empty() && s.front() == '.') {
            s.remove_prefix(1);
            if (!read_int(s, v.patch)) return std::nullopt;
        }
    }
    if (!s.empty()) return std::nullopt;
    return v;
}

}  // namespace

std::string Version::str() const {
    std::string r = std::to_string(major);
    if (minor >= 0) r += "." + std::to_string(minor);
    if (patch >= 0) r += "." + std::to_string(patch);
    return r;
}

std::optional<Version> parse_version(std::string_view s) {
    s = trim(s);
    Version v;
    if (!read_int(s, v.major)) return std::nullopt;
    if (!s.empty() && s.front() == '.') {
        auto t = s;
        t.remove_prefix(1);
        if (read_int(t, v.minor)) {
            s = t;
            if (!s.empty() && s.front() == '.') {
                auto u = s;
                u.remove_prefix(1);
                if (read_int(u, v.patch)) s = u;
            }
        }
    }
    return v;
}

std::optional<VersionSpec> VersionSpec::parse(std::string_view text) {
    VersionSpec spec;
    spec.text_ = std::string(trim(text));
    std::string_view rest = trim(text);
    if (rest.empty()) return std::nullopt;
    if (rest == "latest" || rest == "*") {
        spec.text_ = "latest";
        return spec;
    }
    while (!rest.empty()) {
        auto comma = rest.find(',');
        auto piece = trim(rest.substr(0, comma));
        rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
        if (piece.empty()) return std::nullopt;

        auto take = [&](std::string_view prefix) {
            if (piece.starts_with(prefix)) {
                piece.remove_prefix(prefix.size());
                return true;
            }
            return false;
        };

        if (take(">=")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Ge, *v});
        } else if (take("<=")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Le, *v});
        } else if (take(">")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Gt, *v});
        } else if (take("<")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Lt, *v});
        } else if (take("^")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Ge, v->normalized()});
            spec.constraints_.push_back({Op::Lt, Version{v->major + 1, 0, 0}});
        } else if (take("~")) {
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Ge, v->normalized()});
            if (v->minor >= 0)
                spec.constraints_.push_back({Op::Lt, Version{v->major, v->minor + 1, 0}});
            else
                spec.constraints_.push_back({Op::Lt, Version{v->major + 1, 0, 0}});
        } else {
            take("=");
            auto v = parse_strict(piece);
            if (!v) return std::nullopt;
            spec.constraints_.push_back({Op::Prefix, *v});
        }
    }
    return spec;
}

bool VersionSpec::matches(const Version& v) const {
    for (const auto& c : constraints_) {
        switch (c.op) {
            case Op::Prefix:
                if (v.major != c.v.major) return false;
                if (c.v.minor >= 0 && v.normalized().minor != c.v.minor) return false;
                if (c.v.patch >= 0 && v.normalized().patch != c.v.patch) return false;
                break;
            case Op::Ge:
                if (!(v >= c.v)) return false;
                break;
            case Op::Gt:
                if (!(v > c.v)) return false;
                break;
            case Op::Le:
                if (!(v <= c.v)) return false;
                break;
            case Op::Lt:
                if (!(v < c.v)) return false;
                break;
        }
    }
    return true;
}

}  // namespace jig
