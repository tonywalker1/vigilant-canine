//
// vigilant-canine - Config Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <config/config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    TEST(ConfigTest, LoadDefaultConfig) {
        auto cfg_result = load_config_or_default("/nonexistent/path/config.toml");

        ASSERT_TRUE(cfg_result.has_value());
        auto const& cfg = cfg_result.value();

        // Verify defaults
        EXPECT_EQ(cfg.daemon.log_level, "info");
        EXPECT_EQ(cfg.daemon.worker_threads, 0);
        EXPECT_EQ(cfg.hash.algorithm, HashAlgorithm::blake3);
        EXPECT_TRUE(cfg.alerts.journal);
        EXPECT_TRUE(cfg.alerts.dbus);
    }

    TEST(ConfigTest, LoadValidConfig) {
        // Create temporary config file
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_config.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[daemon]
log_level = "debug"
worker_threads = 4

[hash]
algorithm = "sha256"

[alerts]
journal = true
dbus = false
socket = true
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_TRUE(cfg_result.has_value());

        auto const& cfg = cfg_result.value();
        EXPECT_EQ(cfg.daemon.log_level, "debug");
        EXPECT_EQ(cfg.daemon.worker_threads, 4);
        EXPECT_EQ(cfg.hash.algorithm, HashAlgorithm::sha256);
        EXPECT_TRUE(cfg.alerts.journal);
        EXPECT_FALSE(cfg.alerts.dbus);
        EXPECT_TRUE(cfg.alerts.socket);

        std::filesystem::remove(temp_path);
    }

    TEST(ConfigTest, InvalidTomlSyntax) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_invalid.toml";
        {
            std::ofstream file(temp_path);
            file << "this is not valid toml [[[";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_FALSE(cfg_result.has_value());
        EXPECT_NE(cfg_result.error().find("parse error"), std::string::npos);

        std::filesystem::remove(temp_path);
    }

    TEST(ConfigTest, InvalidHashAlgorithm) {
        auto temp_path = std::filesystem::temp_directory_path() / "vc_test_invalid_hash.toml";
        {
            std::ofstream file(temp_path);
            file << R"(
[hash]
algorithm = "md5"
)";
        }

        auto cfg_result = load_config(temp_path);
        ASSERT_FALSE(cfg_result.has_value());
        EXPECT_FALSE(cfg_result.error().empty());

        std::filesystem::remove(temp_path);
    }

}  // namespace vigilant_canine
