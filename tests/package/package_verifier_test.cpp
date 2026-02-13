//
// vigilant-canine - Package Verifier Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <package/package_verifier.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    class PackageVerifierTest : public ::testing::Test {
    protected:
        PackageVerifier m_verifier;
    };

    TEST_F(PackageVerifierTest, VerificationStatusToString) {
        EXPECT_EQ(to_string(VerificationStatus::ok), "ok");
        EXPECT_EQ(to_string(VerificationStatus::modified), "modified");
        EXPECT_EQ(to_string(VerificationStatus::missing), "missing");
        EXPECT_EQ(to_string(VerificationStatus::not_packaged), "not_packaged");
        EXPECT_EQ(to_string(VerificationStatus::error), "error");
    }

    TEST_F(PackageVerifierTest, HasPackageManager) {
        // At least one package manager should be available on a Linux system
        bool has_pm = m_verifier.has_rpm() || m_verifier.has_dpkg();

        // This test might fail in minimal containers, so we make it informational
        if (!has_pm) {
            GTEST_SKIP() << "No package manager (rpm/dpkg) available for testing";
        }

        EXPECT_TRUE(has_pm);
    }

    TEST_F(PackageVerifierTest, VerifySystemBinary) {
        // Skip if no package manager available
        if (!m_verifier.has_rpm() && !m_verifier.has_dpkg()) {
            GTEST_SKIP() << "No package manager available";
        }

        // /usr/bin/bash should exist and be packaged on most systems
        std::filesystem::path bash_path = "/usr/bin/bash";

        if (!std::filesystem::exists(bash_path)) {
            GTEST_SKIP() << "/usr/bin/bash not found";
        }

        auto result = m_verifier.verify_file(bash_path);

        // The file should either be OK or modified (if system has been updated)
        // It should NOT be not_packaged or missing
        EXPECT_NE(result.status, VerificationStatus::not_packaged);
        EXPECT_NE(result.status, VerificationStatus::missing);

        // If verification succeeded, we should have a package name
        if (result.status == VerificationStatus::ok ||
            result.status == VerificationStatus::modified) {
            EXPECT_TRUE(result.package_name.has_value());
            EXPECT_FALSE(result.package_name->empty());
        }
    }

    TEST_F(PackageVerifierTest, VerifyNonExistentFile) {
        std::filesystem::path fake_path = "/nonexistent/fake/file.txt";

        auto result = m_verifier.verify_file(fake_path);

        // Should be not_packaged or error (file doesn't exist)
        EXPECT_TRUE(result.status == VerificationStatus::not_packaged ||
                    result.status == VerificationStatus::error);
    }

    TEST_F(PackageVerifierTest, VerifyTempFile) {
        // Create a temporary file that won't be packaged
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_temp.txt";

        // Create the file
        {
            std::ofstream temp_file(temp_path);
            temp_file << "test content\n";
        }

        auto result = m_verifier.verify_file(temp_path);

        // Cleanup
        std::filesystem::remove(temp_path);

        // Temporary files should not be packaged
        EXPECT_EQ(result.status, VerificationStatus::not_packaged);
        EXPECT_FALSE(result.package_name.has_value());
    }

    TEST_F(PackageVerifierTest, VerifyEtcPasswd) {
        // Skip if no package manager available
        if (!m_verifier.has_rpm() && !m_verifier.has_dpkg()) {
            GTEST_SKIP() << "No package manager available";
        }

        std::filesystem::path passwd_path = "/etc/passwd";

        if (!std::filesystem::exists(passwd_path)) {
            GTEST_SKIP() << "/etc/passwd not found";
        }

        auto result = m_verifier.verify_file(passwd_path);

        // /etc/passwd is often modified, so we don't assert specific status
        // Just verify we got some result
        EXPECT_TRUE(result.status == VerificationStatus::ok ||
                    result.status == VerificationStatus::modified ||
                    result.status == VerificationStatus::not_packaged);

        // Log the result for debugging
        std::cout << "  /etc/passwd verification: "
                  << to_string(result.status);
        if (result.package_name) {
            std::cout << " (package: " << *result.package_name << ")";
        }
        std::cout << "\n";
    }

}  // namespace vigilant_canine
