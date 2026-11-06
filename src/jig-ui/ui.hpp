// SPDX-License-Identifier: AGPL-3.0-only
// jig-ui/ui.hpp
// The only library that prints (001 section 8). Other libraries emit typed events;
// this one renders them as narrated text, terse text, or JSON lines (002 section 4).
#pragma once

#include <chrono>
#include <string>
#include <variant>
#include <vector>

#include "jig-core/error.hpp"
#include "jig-core/settings.hpp"

namespace jig {

struct Alt {
    std::string what;
    std::string why_not;
};

namespace ev {
struct Step { std::string title; };
struct Decision {
    std::string what;                  // "compiler"
    std::string chosen;                // "gcc 13.3.0 (/usr/bin/g++-13)"
    std::vector<std::string> why;      // reasons
    std::vector<Alt> alternatives;     // what was passed over, and why
};
struct Fact { std::string key, value; };               // something jig read or found, shown by default
struct Info { std::string key, value; };               // detail, shown from -v
struct Exec { std::vector<std::string> argv; std::string cwd; };  // shown from -vv
struct Output { std::string line; };                   // compiler / cmake / ninja output
struct Warn { std::string code, message, help; };
struct Done { std::string summary; double seconds = 0; };
struct Hint { std::string text; };
struct Fail { Error err; };
}  // namespace ev

using Event = std::variant<ev::Step, ev::Decision, ev::Fact, ev::Info, ev::Exec, ev::Output, ev::Warn, ev::Done, ev::Hint, ev::Fail>;

class Ui {
public:
    explicit Ui(const Settings& s);

    void emit(const Event& e);

    // Helpers that build the event for you.
    void step(std::string title) { emit(ev::Step{std::move(title)}); }
    void decision(std::string what, std::string chosen, std::vector<std::string> why = {}, std::vector<Alt> alts = {}) {
        emit(ev::Decision{std::move(what), std::move(chosen), std::move(why), std::move(alts)});
    }
    void fact(std::string key, std::string value) { emit(ev::Fact{std::move(key), std::move(value)}); }
    void info(std::string key, std::string value) { emit(ev::Info{std::move(key), std::move(value)}); }
    void exec(std::vector<std::string> argv, std::string cwd = {}) { emit(ev::Exec{std::move(argv), std::move(cwd)}); }
    void output(std::string line) { emit(ev::Output{std::move(line)}); }
    void warn(std::string code, std::string msg, std::string help = {}) { emit(ev::Warn{std::move(code), std::move(msg), std::move(help)}); }
    void done(std::string summary, double seconds = 0) { emit(ev::Done{std::move(summary), seconds}); }
    void hint(std::string text) { emit(ev::Hint{std::move(text)}); }
    void fail(const Error& e) { emit(ev::Fail{e}); }

    [[nodiscard]] int verbosity() const { return s_.verbosity; }
    [[nodiscard]] bool json() const { return s_.json; }
    [[nodiscard]] bool color() const { return color_; }
    [[nodiscard]] bool unicode() const { return unicode_; }

    // For command output that is the result (tables, paths): stdout, no decoration.
    void print(const std::string& text) const;
    // Styling helpers for result text.
    [[nodiscard]] std::string bold(const std::string& s) const;
    [[nodiscard]] std::string dim(const std::string& s) const;
    [[nodiscard]] std::string green(const std::string& s) const;
    [[nodiscard]] std::string yellow(const std::string& s) const;
    [[nodiscard]] std::string red(const std::string& s) const;
    // The word mark for the front of the tool (bare jig, --help, version).
    // Colour follows the usual rules, but judged against stdout, because that is where it goes.
    [[nodiscard]] std::string front() const;
    [[nodiscard]] std::string signal(const std::string& s) const;
    [[nodiscard]] const char* ok_glyph() const { return unicode_ ? "\xE2\x9C\x93" : "ok"; }

private:
    void render_human(const Event& e);
    void render_json(const Event& e);
    void write_err(const std::string& s) const;
    std::string wrap(const char* code, const std::string& s) const;

    Settings s_;
    bool color_ = false;
    bool unicode_ = true;
    bool first_step_ = true;
};

class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}
    [[nodiscard]] double seconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

std::string format_seconds(double s);
std::string json_escape(const std::string& s);

}  // namespace jig
