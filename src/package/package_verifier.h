//
// vigilant-canine - Package Manager Verification
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_PACKAGE_PACKAGE_VERIFIER_H
#define VIGILANT_CANINE_PACKAGE_PACKAGE_VERIFIER_H

#include <core/types.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace vigilant_canine {

    //
    // Package file verification result.
    //
    enum class VerificationStatus {
        ok,              // File matches package metadata
        modified,        // File has been modified from package version
        missing,         // File is missing
        not_packaged,    // File is not owned by any package
        error            // Error during verification
    };

    //
    // Convert VerificationStatus to string.
    //
    [[nodiscard]] auto to_string(VerificationStatus status) -> std::string;

    //
    // Package file verification result with details.
    //
    struct VerificationResult {
        VerificationStatus status;
        std::optional<std::string> package_name;  // Package owning the file
        std::optional<std::string> expected_hash; // Expected hash from package
        std::optional<std::string> actual_hash;   // Actual file hash
        std::optional<std::string> error;         // Error message if status is error
    };

    //
    // Package manager verifier.
    //
    // Verifies files against package manager metadata (rpm/dpkg checksums).
    //
    class PackageVerifier {
    public:
        //
        // Verify a file against package manager metadata.
        //
        // Queries the package manager to determine if the file is owned by
        // a package and whether it matches the expected checksum.
        //
        // Preconditions:
        //   - path must refer to a regular file
        //
        // Postconditions:
        //   - Returns verification result with status and details
        //
        [[nodiscard]] auto verify_file(std::filesystem::path const& path) const
            -> VerificationResult;

        //
        // Check if RPM is available on the system.
        //
        [[nodiscard]] auto has_rpm() const -> bool;

        //
        // Check if dpkg is available on the system.
        //
        [[nodiscard]] auto has_dpkg() const -> bool;

    private:
        //
        // Verify file using RPM.
        //
        [[nodiscard]] auto verify_with_rpm(std::filesystem::path const& path) const
            -> std::optional<VerificationResult>;

        //
        // Verify file using dpkg.
        //
        [[nodiscard]] auto verify_with_dpkg(std::filesystem::path const& path) const
            -> std::optional<VerificationResult>;

        //
        // Query RPM for file owner.
        //
        [[nodiscard]] auto query_rpm_owner(std::filesystem::path const& path) const
            -> std::optional<std::string>;

        //
        // Query dpkg for file owner.
        //
        [[nodiscard]] auto query_dpkg_owner(std::filesystem::path const& path) const
            -> std::optional<std::string>;

        //
        // Execute a command and return output.
        //
        [[nodiscard]] auto exec_command(std::string const& command) const
            -> std::expected<std::string, std::string>;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_PACKAGE_PACKAGE_VERIFIER_H
