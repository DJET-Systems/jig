// SPDX-License-Identifier: GPL-3.0-only
#include "jig-core/platform.hpp"

#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <sstream>

extern char** environ;

namespace jig {

HostInfo host_info() {
    struct utsname u {};
    uname(&u);
    HostInfo h;
    std::string sys = u.sysname;
    std::string m = u.machine;
    h.arch = (m == "arm64" || m == "aarch64") ? "aarch64" : m;
    if (sys == "Darwin") {
        h.os = "macos";
        h.triple = h.arch + "-apple-darwin";
    } else {
        h.os = "linux";
        h.triple = h.arch + "-unknown-linux-gnu";
    }
    return h;
}

EnvMap current_env() {
    EnvMap m;
    for (char** e = environ; e && *e; ++e) {
        std::string_view kv(*e);
        auto eq = kv.find('=');
        if (eq == std::string_view::npos || eq == 0) continue;
        m.emplace(std::string(kv.substr(0, eq)), std::string(kv.substr(eq + 1)));
    }
    return m;
}

std::optional<std::string> getenv_str(const char* name) {
    if (const char* v = std::getenv(name)) return std::string(v);
    return std::nullopt;
}

bool stdout_is_tty() { return isatty(STDOUT_FILENO) != 0; }
bool stderr_is_tty() { return isatty(STDERR_FILENO) != 0; }
bool stdin_is_tty() { return isatty(STDIN_FILENO) != 0; }

bool locale_is_utf8() {
    if (auto t = getenv_str("TERM"); t && *t == "dumb") return false;
    for (const char* var : {"LC_ALL", "LC_CTYPE", "LANG"}) {
        if (auto v = getenv_str(var); v && !v->empty()) {
            std::string s = *v;
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s.find("utf-8") != std::string::npos || s.find("utf8") != std::string::npos;
        }
    }
    return false;
}

fs::path jig_home() {
    if (auto v = getenv_str("JIG_HOME"); v && !v->empty()) return *v;
    auto home = getenv_str("HOME").value_or("/tmp");
#ifdef __APPLE__
    return fs::path(home) / "Library" / "Application Support" / "jig";
#else
    if (auto x = getenv_str("XDG_DATA_HOME"); x && !x->empty()) return fs::path(*x) / "jig";
    return fs::path(home) / ".local" / "share" / "jig";
#endif
}

std::vector<fs::path> path_dirs(const std::optional<std::string>& path_override) {
    std::string path = path_override ? *path_override : getenv_str("PATH").value_or("");
    std::vector<fs::path> dirs;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == ':') {
            if (i > start) dirs.emplace_back(path.substr(start, i - start));
            start = i + 1;
        }
    }
    return dirs;
}

std::optional<fs::path> which(std::string_view name, const std::optional<std::string>& path_override) {
    if (name.find('/') != std::string_view::npos) {
        fs::path p(name);
        if (access(p.c_str(), X_OK) == 0 && fs::is_regular_file(p)) return p;
        return std::nullopt;
    }
    for (const auto& d : path_dirs(path_override)) {
        auto p = d / std::string(name);
        std::error_code ec;
        if (fs::is_regular_file(p, ec) && access(p.c_str(), X_OK) == 0) return p;
    }
    return std::nullopt;
}

Result<std::string> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return fail("DJ9000", exit_code::failure, "cannot read " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Result<void> write_file_atomic(const fs::path& p, std::string_view content, bool executable) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    if (ec) return fail("DJ9000", exit_code::failure, "cannot create directory " + p.parent_path().string(), {}, {ec.message()});
    auto tmp = p;
    tmp += ".tmp-" + std::to_string(::getpid());
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return fail("DJ9000", exit_code::failure, "cannot write " + tmp.string());
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) return fail("DJ9000", exit_code::failure, "write failed for " + tmp.string());
    }
    if (executable) ::chmod(tmp.c_str(), 0755);
    fs::rename(tmp, p, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return fail("DJ9000", exit_code::failure, "cannot replace " + p.string(), {}, {ec.message()});
    }
    return {};
}

std::string sanitized_dir_name(const fs::path& dir) {
    auto name = fs::absolute(dir).lexically_normal().filename().string();
    if (name.empty() || name == ".") name = fs::absolute(dir).parent_path().filename().string();
    std::string out;
    for (char c : name) {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) out.push_back(static_cast<char>(std::tolower(u)));
        else if (c == '-' || c == '_') out.push_back(c);
        else if (c == ' ' || c == '.') out.push_back('-');
    }
    while (!out.empty() && !std::isalpha(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
    if (out.size() > 64) out.resize(64);
    return out.empty() ? "app" : out;
}

}  // namespace jig
