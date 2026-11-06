// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/tomledit.hpp
// Minimal, comment-preserving edits of jig.toml. A real TOML round trip would drop comments,
// so `toolchain use` and `toolchain pin` patch the text instead.
#pragma once

#include <string>
#include <string_view>

namespace jig {

// Sets `key = value_literal` inside `[section]` (section may be dotted: "toolchain.system").
// Replaces the line when the key exists, inserts it at the end of the section when it does not,
// and appends the section when it is missing. `value_literal` is already TOML ("\"13\"", "true").
std::string set_toml_key(std::string text, std::string_view section, std::string_view key,
                         std::string_view value_literal);

std::string toml_quote(std::string_view s);

}  // namespace jig
