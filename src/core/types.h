//
// vigilant-canine - Core Types
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_CORE_TYPES_H
#define VIGILANT_CANINE_CORE_TYPES_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <compare>

namespace vigilant_canine {

    //
    // Strong type wrapper using CRTP for zero-cost abstraction.
    // Prevents accidental mixing of semantically different types.
    //
    // Example:
    //   using FilePath = StrongType<struct FilePathTag, std::filesystem::path>;
    //   FilePath path{"/etc/passwd"};
    //
    template<typename Tag, typename T>
    struct StrongType {
        T value;

        constexpr StrongType() = default;
        explicit constexpr StrongType(T v) : value(std::move(v)) {}

        // Allow implicit conversion back to underlying type when needed
        [[nodiscard]] constexpr auto operator*() const -> T const& { return value; }
        [[nodiscard]] constexpr auto operator*() -> T& { return value; }

        // Comparison operators
        auto operator<=>(StrongType const&) const = default;
    };

    //
    // Domain-specific strong types
    //

    // File system path (prevents passing raw strings where paths are expected)
    using FilePath = StrongType<struct FilePathTag, std::filesystem::path>;

    // Hash digest value (hex-encoded string)
    using HashValue = StrongType<struct HashValueTag, std::string>;

    // Alert unique identifier
    using AlertId = StrongType<struct AlertIdTag, std::int64_t>;

    // ostree deployment identifier
    using DeploymentId = StrongType<struct DeploymentIdTag, std::string>;

    //
    // Hash algorithm selection
    //
    enum class HashAlgorithm : std::uint8_t {
        blake3,  // Default: fast, cryptographically secure
        sha256   // Alternative: widely recognized, slower
    };

    //
    // Alert severity levels
    //
    enum class Severity : std::uint8_t {
        info,       // Informational (baseline refresh, scan complete)
        warning,    // Suspicious but possibly benign (permission change)
        critical,   // Likely compromise (binary modification, unexpected mount)
        emergency   // Active attack indicators (multiple critical in short window)
    };

    //
    // Distro type for baseline strategy selection
    //
    enum class DistroType : std::uint8_t {
        traditional,     // Fedora Workstation, Ubuntu, Debian, Arch, etc.
        ostree,          // Fedora Silverblue/Kinoite, RHEL for Edge
        btrfs_snapshot   // openSUSE MicroOS / Aeon (future)
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_CORE_TYPES_H
