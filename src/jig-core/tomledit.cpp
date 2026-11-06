// SPDX-License-Identifier: AGPL-3.0-only
#include "jig-core/tomledit.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace jig {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

bool is_header(const std::string& line) {
    auto t = trim(line);
    return !t.empty() && t.front() == '[';
}

}  // namespace

std::string toml_quote(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string set_toml_key(std::string text, std::string_view section, std::string_view key,
                         std::string_view value_literal) {
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l)) lines.push_back(l);
    }
    const std::string header = "[" + std::string(section) + "]";
    std::size_t sec_begin = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (trim(lines[i]) == header) {
            sec_begin = i;
            break;
        }

    if (sec_begin == lines.size()) {
        if (!lines.empty() && !trim(lines.back()).empty()) lines.emplace_back();
        lines.push_back(header);
        lines.push_back(std::string(key) + " = " + std::string(value_literal));
    } else {
        std::size_t sec_end = lines.size();
        for (std::size_t i = sec_begin + 1; i < lines.size(); ++i)
            if (is_header(lines[i])) {
                sec_end = i;
                break;
            }
        bool replaced = false;
        for (std::size_t i = sec_begin + 1; i < sec_end; ++i) {
            auto t = trim(lines[i]);
            if (t.rfind(key, 0) == 0) {
                auto rest = trim(t.substr(key.size()));
                if (!rest.empty() && rest.front() == '=') {
                    auto eq = lines[i].find('=');
                    lines[i] = lines[i].substr(0, eq + 1) + " " + std::string(value_literal);
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) {
            std::size_t at = sec_end;
            while (at > sec_begin + 1 && trim(lines[at - 1]).empty()) --at;
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(at),
                         std::string(key) + " = " + std::string(value_literal));
        }
    }
    std::string out;
    for (const auto& l : lines) out += l + "\n";
    return out;
}

}  // namespace jig
