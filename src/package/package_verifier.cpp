//
// vigilant-canine - Package Manager Verification Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <package/package_verifier.h>

#include <array>
#include <cstdio>
#include <format>
#include <memory>

namespace vigilant_canine {

    auto to_string(VerificationStatus status) -> std::string {
        switch (status) {
            case VerificationStatus::ok:
                return "ok";
            case VerificationStatus::modified:
                return "modified";
            case VerificationStatus::missing:
                return "missing";
            case VerificationStatus::not_packaged:
                return "not_packaged";
            case VerificationStatus::error:
                return "error";
        }
        return "unknown";
    }

    auto PackageVerifier::verify_file(std::filesystem::path const& path) const
        -> VerificationResult {

        // Try RPM verification first
        if (has_rpm()) {
            if (auto result = verify_with_rpm(path)) {
                return *result;
            }
        }

        // Try dpkg verification
        if (has_dpkg()) {
            if (auto result = verify_with_dpkg(path)) {
                return *result;
            }
        }

        // File is not managed by any known package manager
        return VerificationResult{
            .status = VerificationStatus::not_packaged,
            .package_name = std::nullopt,
            .expected_hash = std::nullopt,
            .actual_hash = std::nullopt,
            .error = std::nullopt
        };
    }

    auto PackageVerifier::has_rpm() const -> bool {
        auto result = exec_command("which rpm 2>/dev/null");
        return result.has_value() && !result->empty();
    }

    auto PackageVerifier::has_dpkg() const -> bool {
        auto result = exec_command("which dpkg 2>/dev/null");
        return result.has_value() && !result->empty();
    }

    auto PackageVerifier::verify_with_rpm(std::filesystem::path const& path) const
        -> std::optional<VerificationResult> {

        // First, check if file is owned by an RPM package
        auto owner = query_rpm_owner(path);
        if (!owner) {
            return std::nullopt;  // Not owned by RPM package
        }

        // Use rpm -V to verify the file
        // Output format: SM5DLUGT c /path/to/file
        // S = size, M = mode, 5 = MD5 sum, D = device, L = link, U = user, G = group, T = mtime
        auto cmd = std::format("rpm -V --nomtime --nouser --nogroup '{}' 2>&1",
                               *owner);
        auto verify_result = exec_command(cmd);

        if (!verify_result) {
            return VerificationResult{
                .status = VerificationStatus::error,
                .package_name = *owner,
                .expected_hash = std::nullopt,
                .actual_hash = std::nullopt,
                .error = verify_result.error()
            };
        }

        // Parse rpm -V output
        auto const& output = *verify_result;

        // If output is empty, file is OK
        if (output.empty()) {
            return VerificationResult{
                .status = VerificationStatus::ok,
                .package_name = *owner,
                .expected_hash = std::nullopt,
                .actual_hash = std::nullopt,
                .error = std::nullopt
            };
        }

        // Check if our file appears in the output
        if (output.find(path.string()) != std::string::npos) {
            // File has been modified
            // Check if it's just a checksum difference (5 flag)
            if (output.find("5") != std::string::npos) {
                return VerificationResult{
                    .status = VerificationStatus::modified,
                    .package_name = *owner,
                    .expected_hash = "rpm-metadata",
                    .actual_hash = "file-differs",
                    .error = std::nullopt
                };
            }

            // Other type of modification
            return VerificationResult{
                .status = VerificationStatus::modified,
                .package_name = *owner,
                .expected_hash = std::nullopt,
                .actual_hash = std::nullopt,
                .error = "File attributes differ from package"
            };
        }

        // File not in verification output, assume OK
        return VerificationResult{
            .status = VerificationStatus::ok,
            .package_name = *owner,
            .expected_hash = std::nullopt,
            .actual_hash = std::nullopt,
            .error = std::nullopt
        };
    }

    auto PackageVerifier::verify_with_dpkg(std::filesystem::path const& path) const
        -> std::optional<VerificationResult> {

        // First, check if file is owned by a dpkg package
        auto owner = query_dpkg_owner(path);
        if (!owner) {
            return std::nullopt;  // Not owned by dpkg package
        }

        // Use dpkg --verify to check the package
        auto cmd = std::format("dpkg --verify '{}' 2>&1", *owner);
        auto verify_result = exec_command(cmd);

        if (!verify_result) {
            return VerificationResult{
                .status = VerificationStatus::error,
                .package_name = *owner,
                .expected_hash = std::nullopt,
                .actual_hash = std::nullopt,
                .error = verify_result.error()
            };
        }

        auto const& output = *verify_result;

        // If output is empty, all files in package are OK
        if (output.empty()) {
            return VerificationResult{
                .status = VerificationStatus::ok,
                .package_name = *owner,
                .expected_hash = std::nullopt,
                .actual_hash = std::nullopt,
                .error = std::nullopt
            };
        }

        // Check if our specific file is mentioned in the output
        if (output.find(path.string()) != std::string::npos) {
            return VerificationResult{
                .status = VerificationStatus::modified,
                .package_name = *owner,
                .expected_hash = "dpkg-metadata",
                .actual_hash = "file-differs",
                .error = std::nullopt
            };
        }

        // File not mentioned, assume OK
        return VerificationResult{
            .status = VerificationStatus::ok,
            .package_name = *owner,
            .expected_hash = std::nullopt,
            .actual_hash = std::nullopt,
            .error = std::nullopt
        };
    }

    auto PackageVerifier::query_rpm_owner(std::filesystem::path const& path) const
        -> std::optional<std::string> {

        auto cmd = std::format("rpm -qf '{}' 2>/dev/null", path.string());
        auto result = exec_command(cmd);

        if (!result || result->empty()) {
            return std::nullopt;
        }

        // Remove trailing whitespace
        auto pkg = *result;
        while (!pkg.empty() && std::isspace(pkg.back())) {
            pkg.pop_back();
        }

        // Check if it's an error message
        if (pkg.find("not owned") != std::string::npos) {
            return std::nullopt;
        }

        return pkg.empty() ? std::nullopt : std::make_optional(pkg);
    }

    auto PackageVerifier::query_dpkg_owner(std::filesystem::path const& path) const
        -> std::optional<std::string> {

        auto cmd = std::format("dpkg -S '{}' 2>/dev/null | cut -d: -f1", path.string());
        auto result = exec_command(cmd);

        if (!result || result->empty()) {
            return std::nullopt;
        }

        // Remove trailing whitespace
        auto pkg = *result;
        while (!pkg.empty() && std::isspace(pkg.back())) {
            pkg.pop_back();
        }

        return pkg.empty() ? std::nullopt : std::make_optional(pkg);
    }

    auto PackageVerifier::exec_command(std::string const& command) const
        -> std::expected<std::string, std::string> {

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

        if (!pipe) {
            return std::unexpected("Failed to execute command");
        }

        std::array<char, 512> buffer{};
        std::string result;

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    }

}  // namespace vigilant_canine
