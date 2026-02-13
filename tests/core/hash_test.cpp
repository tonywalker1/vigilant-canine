//
// vigilant-canine - Hash Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <core/hash.h>

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <filesystem>

namespace vigilant_canine {

    //
    // Test hash_bytes with known test vectors.
    //

    TEST(HashTest, Blake3EmptyString) {
        std::array<std::byte, 0> empty{};
        auto hash = hash_bytes(empty, HashAlgorithm::blake3);

        // BLAKE3 hash of empty string (from official test vectors)
        EXPECT_EQ(*hash, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262");
    }

    TEST(HashTest, Blake3HelloWorld) {
        std::string_view input = "hello world";
        std::span data{reinterpret_cast<std::byte const*>(input.data()), input.size()};

        auto hash = hash_bytes(data, HashAlgorithm::blake3);

        // BLAKE3 hash of "hello world"
        EXPECT_EQ(*hash, "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24");
    }

    TEST(HashTest, Sha256EmptyString) {
        std::array<std::byte, 0> empty{};
        auto hash = hash_bytes(empty, HashAlgorithm::sha256);

        // SHA-256 hash of empty string
        EXPECT_EQ(*hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    TEST(HashTest, Sha256HelloWorld) {
        std::string_view input = "hello world";
        std::span data{reinterpret_cast<std::byte const*>(input.data()), input.size()};

        auto hash = hash_bytes(data, HashAlgorithm::sha256);

        // SHA-256 hash of "hello world"
        EXPECT_EQ(*hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    }

    //
    // Test hash_file with temporary files.
    //

    TEST(HashTest, HashFileSuccess) {
        // Create temporary file
        auto temp_path = std::filesystem::temp_directory_path() / "vigilant_canine_test_file.txt";
        {
            std::ofstream file(temp_path);
            file << "hello world";
        }

        auto result = hash_file(FilePath{temp_path}, HashAlgorithm::blake3);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result.value(), "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24");

        std::filesystem::remove(temp_path);
    }

    TEST(HashTest, HashFileNotFound) {
        auto result = hash_file(FilePath{"/nonexistent/file.txt"}, HashAlgorithm::blake3);

        ASSERT_FALSE(result.has_value());
        EXPECT_NE(result.error().find("Failed to open file"), std::string::npos);
    }

    //
    // Test algorithm conversion functions.
    //

    TEST(HashTest, AlgorithmToString) {
        EXPECT_EQ(algorithm_to_string(HashAlgorithm::blake3), "blake3");
        EXPECT_EQ(algorithm_to_string(HashAlgorithm::sha256), "sha256");
    }

    TEST(HashTest, StringToAlgorithm) {
        auto blake3 = string_to_algorithm("blake3");
        ASSERT_TRUE(blake3.has_value());
        EXPECT_EQ(blake3.value(), HashAlgorithm::blake3);

        auto sha256 = string_to_algorithm("sha256");
        ASSERT_TRUE(sha256.has_value());
        EXPECT_EQ(sha256.value(), HashAlgorithm::sha256);

        auto unknown = string_to_algorithm("unknown");
        ASSERT_FALSE(unknown.has_value());
        EXPECT_NE(unknown.error().find("Unknown hash algorithm"), std::string::npos);
    }

}  // namespace vigilant_canine
