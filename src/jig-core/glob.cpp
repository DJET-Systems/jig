// SPDX-License-Identifier: GPL-3.0-only
#include "jig-core/glob.hpp"

#include <algorithm>
#include <set>

namespace jig {

std::vector<std::string> expand_braces(std::string_view pattern) {
    auto open = pattern.find('{');
    if (open == std::string_view::npos) return {std::string(pattern)};
    int depth = 0;
    std::size_t close = std::string_view::npos;
    for (std::size_t i = open; i < pattern.size(); ++i) {
        if (pattern[i] == '{') ++depth;
        else if (pattern[i] == '}' && --depth == 0) {
            close = i;
            break;
        }
    }
    if (close == std::string_view::npos) return {std::string(pattern)};  // unbalanced: literal

    auto head = pattern.substr(0, open);
    auto body = pattern.substr(open + 1, close - open - 1);
    auto tail = pattern.substr(close + 1);

    std::vector<std::string> alts;
    depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= body.size(); ++i) {
        if (i == body.size() || (body[i] == ',' && depth == 0)) {
            alts.emplace_back(body.substr(start, i - start));
            start = i + 1;
        } else if (body[i] == '{') {
            ++depth;
        } else if (body[i] == '}') {
            --depth;
        }
    }
    std::vector<std::string> out;
    for (const auto& a : alts)
        for (const auto& t : expand_braces(tail))
            for (const auto& h : expand_braces(std::string(head) + a)) out.push_back(h + t);
    return out;
}

namespace {

bool segment_match(std::string_view pat, std::string_view s) {
    std::size_t p = 0, i = 0, star = std::string_view::npos, mark = 0;
    while (i < s.size()) {
        if (p < pat.size() && (pat[p] == '?' || pat[p] == s[i])) {
            ++p;
            ++i;
        } else if (p < pat.size() && pat[p] == '*') {
            star = p++;
            mark = i;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            i = ++mark;
        } else {
            return false;
        }
    }
    while (p < pat.size() && pat[p] == '*') ++p;
    return p == pat.size();
}

std::vector<std::string_view> split(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i)
        if (i == s.size() || s[i] == '/') {
            if (i > start) out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    return out;
}

bool match_segments(const std::vector<std::string_view>& pat, std::size_t pi,
                    const std::vector<std::string_view>& path, std::size_t si) {
    while (pi < pat.size()) {
        if (pat[pi] == "**") {
            if (pi + 1 == pat.size()) return true;
            for (std::size_t k = si; k <= path.size(); ++k)
                if (match_segments(pat, pi + 1, path, k)) return true;
            return false;
        }
        if (si >= path.size() || !segment_match(pat[pi], path[si])) return false;
        ++pi;
        ++si;
    }
    return si == path.size();
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view path) {
    auto segs = split(path);
    for (const auto& alt : expand_braces(pattern)) {
        auto pat = split(alt);
        if (match_segments(pat, 0, segs, 0)) return true;
    }
    return false;
}

std::vector<std::string> glob_files(const std::filesystem::path& root,
                                    const std::vector<std::string>& patterns) {
    namespace fs = std::filesystem;
    std::set<std::string> found;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        auto name = entry.path().filename().string();
        if (entry.is_directory(ec)) {
            if (name == ".git" || name == ".jig" || name == ".hg" || name == ".svn") it.disable_recursion_pending();
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;
        auto rel = entry.path().lexically_relative(root).generic_string();
        for (const auto& p : patterns)
            if (glob_match(p, rel)) {
                found.insert(rel);
                break;
            }
    }
    return {found.begin(), found.end()};
}

}  // namespace jig
