// SPDX-License-Identifier: AGPL-3.0-only
// jig-core/sha256.hpp
// Small SHA-256 (FIPS 180-4). Used for the manifest digest, lock contents and toolchain identity.
// The signed-index verification planned in 004 will use a vetted crypto library (libsodium), not this.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace jig {

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    [[nodiscard]] std::string hex();  // finalizes; do not call update afterwards

private:
    void block(const std::uint8_t* p);
    std::array<std::uint32_t, 8> h_;
    std::array<std::uint8_t, 64> buf_{};
    std::size_t buf_len_ = 0;
    std::uint64_t total_ = 0;
};

std::string sha256_hex(std::string_view data);
std::optional<std::string> sha256_file(const std::filesystem::path& p);

}  // namespace jig
