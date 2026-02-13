//
// vigilant-canine - Hash Abstraction
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_CORE_HASH_H
#define VIGILANT_CANINE_CORE_HASH_H

#include <core/types.h>

#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace vigilant_canine {

    //
    // Hash a file using the specified algorithm.
    //
    // Preconditions:
    //   - path must refer to an existing readable file
    //
    // Postconditions:
    //   - On success: returns hex-encoded hash digest
    //   - On failure: returns error message describing the failure
    //
    // Example:
    //   auto result = hash_file(FilePath{"/etc/passwd"}, HashAlgorithm::blake3);
    //   if (result) {
    //       std::cout << "Hash: " << *result.value() << "\n";
    //   } else {
    //       std::cerr << "Error: " << result.error() << "\n";
    //   }
    //
    [[nodiscard]] auto hash_file(FilePath const& path, HashAlgorithm alg)
        -> std::expected<HashValue, std::string>;

    //
    // Hash raw bytes using the specified algorithm.
    //
    // This is a pure function used for testing and composability.
    //
    // Preconditions:
    //   - data span must be valid (not dangling)
    //
    // Postconditions:
    //   - Returns hex-encoded hash digest
    //
    [[nodiscard]] auto hash_bytes(std::span<std::byte const> data, HashAlgorithm alg)
        -> HashValue;

    //
    // Convert between HashAlgorithm enum and string representation.
    //
    [[nodiscard]] auto algorithm_to_string(HashAlgorithm alg) -> std::string_view;

    [[nodiscard]] auto string_to_algorithm(std::string_view str)
        -> std::expected<HashAlgorithm, std::string>;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_CORE_HASH_H
