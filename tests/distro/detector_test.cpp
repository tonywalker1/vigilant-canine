//
// vigilant-canine - Distribution Detection Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <distro/detector.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace vigilant_canine {

    TEST(DistroDetectorTest, ParseOsRelease) {
        // This test will succeed if /etc/os-release exists
        if (!std::filesystem::exists("/etc/os-release") &&
            !std::filesystem::exists("/usr/lib/os-release")) {
            GTEST_SKIP() << "No os-release file found";
        }

        auto result = parse_os_release();
        ASSERT_TRUE(result.has_value()) << "Failed to parse os-release: " << result.error();

        auto const& info = result.value();
        EXPECT_FALSE(info.name.empty()) << "Distribution name should not be empty";

        // Print detected info for debugging
        std::cout << "Detected distro: " << info.name << "\n";
        std::cout << "Version: " << info.version << "\n";
        std::cout << "Variant: " << info.variant << "\n";
    }

    TEST(DistroDetectorTest, DetectDistro) {
        auto result = detect_distro();
        ASSERT_TRUE(result.has_value()) << "Failed to detect distro: " << result.error();

        auto const& info = result.value();
        EXPECT_FALSE(info.name.empty());

        // Print detected type
        std::cout << "Distro type: ";
        switch (info.type) {
        case DistroType::traditional:
            std::cout << "traditional\n";
            break;
        case DistroType::ostree:
            std::cout << "ostree\n";
            break;
        case DistroType::btrfs_snapshot:
            std::cout << "btrfs_snapshot\n";
            break;
        }
    }

    TEST(DistroDetectorTest, OstreeDetection) {
        // This just verifies the function doesn't crash
        bool is_ostree = is_ostree_system();

        // On a regular Fedora Workstation, this should be false
        // On Fedora Silverblue/Kinoite, this should be true
        std::cout << "Is ostree system: " << (is_ostree ? "yes" : "no") << "\n";
    }

    TEST(DistroDetectorTest, BtrfsSnapshotDetection) {
        // This just verifies the function doesn't crash
        bool is_btrfs = is_btrfs_snapshot_system();

        // On openSUSE MicroOS/Aeon, this should be true
        // On most other systems, this should be false
        std::cout << "Is btrfs snapshot system: " << (is_btrfs ? "yes" : "no") << "\n";
    }

}  // namespace vigilant_canine
