// SPDX-License-Identifier: GPL-3.0-only
#include "jig-ui/ui.hpp"

// The renderers test the event variant with `if (auto* x = get_if<...>) ... else if (auto* x = ...)`;
// each branch reuses the name `x` on purpose, so -Wshadow is noise in this one file.
#pragma GCC diagnostic ignored "-Wshadow"

#include <cstdio>
#include <cstdlib>

#include "jig-core/platform.hpp"

namespace jig {

namespace {

struct Glyphs {
    const char* step;
    const char* decision;
    const char* reason;
    const char* ok;
    const char* warn;
    const char* err;
};
constexpr Glyphs kUnicode{"\xE2\x96\xB8", "\xE2\x97\x86", "\xE2\x94\x82", "\xE2\x9C\x93", "!", "\xE2\x9C\x97"};
constexpr Glyphs kAscii{">", "*", "|", "ok", "!", "x"};

std::string pad(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s + " ";
    return s + std::string(width - s.size(), ' ');
}

std::string join(const std::vector<std::string>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += ' ';
        const auto& a = v[i];
        if (a.find_first_of(" \t\"'$&|;<>()*?") != std::string::npos) out += "'" + a + "'";
        else out += a;
    }
    return out;
}

}  // namespace

std::string format_seconds(double s) {
    char buf[32];
    if (s < 0.05) std::snprintf(buf, sizeof buf, "%.0f ms", s * 1000.0);
    else if (s < 60) std::snprintf(buf, sizeof buf, "%.1f s", s);
    else std::snprintf(buf, sizeof buf, "%d min %02d s", static_cast<int>(s) / 60, static_cast<int>(s) % 60);
    return buf;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    o += b;
                } else {
                    o.push_back(static_cast<char>(c));
                }
        }
    }
    return o;
}

Ui::Ui(const Settings& s) : s_(s) {
    bool no_color_env = getenv_str("NO_COLOR").has_value() && !getenv_str("NO_COLOR")->empty();
    if (s_.color == "always") color_ = true;
    else if (s_.color == "never") color_ = false;
    else color_ = stderr_is_tty() && !no_color_env && getenv_str("TERM").value_or("") != "dumb";
    unicode_ = locale_is_utf8();
}

std::string Ui::wrap(const char* code, const std::string& s) const {
    if (!color_) return s;
    return std::string("\x1b[") + code + "m" + s + "\x1b[0m";
}
std::string Ui::bold(const std::string& s) const { return wrap("1", s); }
std::string Ui::dim(const std::string& s) const { return wrap("2", s); }
std::string Ui::green(const std::string& s) const { return wrap("32", s); }
std::string Ui::yellow(const std::string& s) const { return wrap("33", s); }
std::string Ui::red(const std::string& s) const { return wrap("31", s); }

void Ui::write_err(const std::string& s) const {
    std::fputs(s.c_str(), stderr);
    std::fflush(stderr);
}

void Ui::print(const std::string& text) const {
    std::fputs(text.c_str(), stdout);
    std::fflush(stdout);
}

void Ui::emit(const Event& e) {
    // `next:` hints are for people. The JSON stream ends with `done` or `fail` (002 use case U4).
    if (s_.json && std::holds_alternative<ev::Hint>(e)) return;
    if (s_.json) render_json(e);
    else render_human(e);
}

void Ui::render_human(const Event& e) {
    const Glyphs& g = unicode_ ? kUnicode : kAscii;
    const int v = s_.verbosity;
    std::string out;

    if (auto* x = std::get_if<ev::Step>(&e)) {
        if (v < 1) return;
        if (v >= 2 && !first_step_) out += "\n";
        first_step_ = false;
        out += (v >= 2 ? bold(std::string(g.step) + " " + x->title) : x->title) + "\n";
    } else if (auto* x = std::get_if<ev::Decision>(&e)) {
        if (v < 2) return;
        out += "  " + std::string(g.decision) + " " + pad(x->what, 16) + " " + x->chosen;
        if (!s_.explain && !x->why.empty()) out += dim("  (" + x->why.front() + ")");
        out += "\n";
        if (s_.explain) {
            for (const auto& w : x->why) out += dim("    " + std::string(g.reason) + " " + w) + "\n";
            for (const auto& a : x->alternatives) out += dim("    " + std::string(g.reason) + " not " + a.what + ": " + a.why_not) + "\n";
        }
    } else if (auto* x = std::get_if<ev::Fact>(&e)) {
        if (v < 2) return;
        out += dim("  " + std::string(g.reason) + " ") + pad(x->key, 16) + " " + x->value + "\n";
    } else if (auto* x = std::get_if<ev::Info>(&e)) {
        if (v < 3) return;
        out += dim("    " + pad(x->key, 16) + " " + x->value) + "\n";
    } else if (auto* x = std::get_if<ev::Exec>(&e)) {
        if (v < 4) return;
        out += dim("    $ " + join(x->argv) + (x->cwd.empty() ? "" : "   (in " + x->cwd + ")")) + "\n";
    } else if (auto* x = std::get_if<ev::Output>(&e)) {
        if (v < 1) return;
        out += "    " + x->line + "\n";
    } else if (auto* x = std::get_if<ev::Warn>(&e)) {
        if (v < 1) return;
        out += yellow(std::string(g.warn) + " " + (x->code.empty() ? "" : x->code + "  ") + x->message) + "\n";
        if (!x->help.empty()) out += "    help: " + x->help + "\n";
    } else if (auto* x = std::get_if<ev::Done>(&e)) {
        if (v < 1) return;
        out += "  " + green(g.ok) + " " + x->summary;
        if (x->seconds >= 0.05 && v >= 2) out += dim("   " + format_seconds(x->seconds));
        out += "\n";
    } else if (auto* x = std::get_if<ev::Hint>(&e)) {
        if (v < 2) return;
        out += "\n" + dim("  next: " + x->text) + "\n";
    } else if (auto* x = std::get_if<ev::Fail>(&e)) {
        const Error& er = x->err;
        out += "\n" + red(std::string(g.err) + " " + er.code + "  " + er.summary) + "\n";
        if (!er.details.empty()) {
            out += "\n";
            for (const auto& d : er.details) out += "  " + d + "\n";
        }
        if (!er.help.empty()) out += "\n  help: " + er.help + "\n";
        out += "  docs: https://djet.online/jig/docs/errors#" + er.code + "\n";
    }
    write_err(out);
}

void Ui::render_json(const Event& e) {
    std::string j;
    auto q = [](const std::string& s) { return "\"" + json_escape(s) + "\""; };
    if (auto* x = std::get_if<ev::Step>(&e)) {
        j = "{\"event\":\"step\",\"title\":" + q(x->title) + "}";
    } else if (auto* x = std::get_if<ev::Decision>(&e)) {
        j = "{\"event\":\"decision\",\"what\":" + q(x->what) + ",\"chosen\":" + q(x->chosen) + ",\"why\":[";
        for (std::size_t i = 0; i < x->why.size(); ++i) j += (i ? "," : "") + q(x->why[i]);
        j += "],\"alternatives\":[";
        for (std::size_t i = 0; i < x->alternatives.size(); ++i)
            j += (i ? "," : "") + std::string("{\"what\":") + q(x->alternatives[i].what) + ",\"why_not\":" + q(x->alternatives[i].why_not) + "}";
        j += "]}";
    } else if (auto* x = std::get_if<ev::Fact>(&e)) {
        j = "{\"event\":\"fact\",\"key\":" + q(x->key) + ",\"value\":" + q(x->value) + "}";
    } else if (auto* x = std::get_if<ev::Info>(&e)) {
        j = "{\"event\":\"info\",\"key\":" + q(x->key) + ",\"value\":" + q(x->value) + "}";
    } else if (auto* x = std::get_if<ev::Exec>(&e)) {
        j = "{\"event\":\"exec\",\"argv\":[";
        for (std::size_t i = 0; i < x->argv.size(); ++i) j += (i ? "," : "") + q(x->argv[i]);
        j += "],\"cwd\":" + q(x->cwd) + "}";
    } else if (auto* x = std::get_if<ev::Output>(&e)) {
        j = "{\"event\":\"output\",\"line\":" + q(x->line) + "}";
    } else if (auto* x = std::get_if<ev::Warn>(&e)) {
        j = "{\"event\":\"warn\",\"code\":" + q(x->code) + ",\"message\":" + q(x->message) + ",\"help\":" + q(x->help) + "}";
    } else if (auto* x = std::get_if<ev::Done>(&e)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.3f", x->seconds);
        j = "{\"event\":\"done\",\"summary\":" + q(x->summary) + ",\"seconds\":" + buf + "}";
    } else if (auto* x = std::get_if<ev::Hint>(&e)) {
        j = "{\"event\":\"hint\",\"text\":" + q(x->text) + "}";
    } else if (auto* x = std::get_if<ev::Fail>(&e)) {
        const Error& er = x->err;
        j = "{\"event\":\"fail\",\"code\":" + q(er.code) + ",\"summary\":" + q(er.summary) + ",\"help\":" + q(er.help) + ",\"details\":[";
        for (std::size_t i = 0; i < er.details.size(); ++i) j += (i ? "," : "") + q(er.details[i]);
        j += "],\"exit\":" + std::to_string(er.exit) + "}";
    }
    j += "\n";
    std::fputs(j.c_str(), stdout);
    std::fflush(stdout);
}

}  // namespace jig
