// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/error.hpp
// One error type for the whole tool. No exception crosses a library boundary (001 D-014).
#pragma once

#include <version>

#include <string>
#include <utility>
#include <vector>

// std::expected where the library has it (GCC 12+ with libstdc++, libc++ 16+); otherwise the vendored
// tl::expected (CC0). Clang 18 on Ubuntu pairs with libstdc++ 13, which hides std::expected because Clang
// reports C++20 concepts as 201907L, so the fallback is needed in practice.
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
namespace jig {
template <class T, class E>
using expected = std::expected<T, E>;
template <class E>
using unexpected = std::unexpected<E>;
}  // namespace jig
#else
#include <tl/expected.hpp>
namespace jig {
template <class T, class E>
using expected = tl::expected<T, E>;
template <class E>
using unexpected = tl::unexpected<E>;
}  // namespace jig
#endif

namespace jig {

// Exit codes (002 section 7).
namespace exit_code {
inline constexpr int ok = 0;
inline constexpr int failure = 1;
inline constexpr int usage = 2;
inline constexpr int project = 3;
inline constexpr int toolchain = 4;
inline constexpr int network = 5;
inline constexpr int integrity = 6;
inline constexpr int build = 7;
inline constexpr int test = 8;
inline constexpr int bug = 101;
inline constexpr int interrupted = 130;
}  // namespace exit_code

struct Error {
    std::string code;                  // "DJ1001", stable once released (D-008)
    std::string summary;               // one line, no trailing period needed
    std::vector<std::string> details;  // facts, one per line
    std::string help;                  // a runnable suggestion
    int exit = exit_code::failure;
};

template <class T>
using Result = expected<T, Error>;

inline unexpected<Error> fail(std::string code, int exit, std::string summary,
                                   std::string help = {}, std::vector<std::string> details = {}) {
    return unexpected<Error>(
        Error{std::move(code), std::move(summary), std::move(details), std::move(help), exit});
}

}  // namespace jig

// Propagate an error out of the current function.
//   JIG_TRY(value, compute());   // `value` is an lvalue reference to the success value
#define JIG_CONCAT_INNER(a, b) a##b
#define JIG_CONCAT(a, b) JIG_CONCAT_INNER(a, b)
#define JIG_TRY(var, expr)                                                         \
    auto JIG_CONCAT(var, _jig_r) = (expr);                                         \
    if (!JIG_CONCAT(var, _jig_r)) return ::jig::unexpected<::jig::Error>(std::move(JIG_CONCAT(var, _jig_r).error())); \
    auto& var = *JIG_CONCAT(var, _jig_r)

#define JIG_TRY_VOID(expr)                                      \
    do {                                                        \
        auto jig_try_void_r = (expr);                           \
        if (!jig_try_void_r) return ::jig::unexpected<::jig::Error>(std::move(jig_try_void_r.error())); \
    } while (false)
