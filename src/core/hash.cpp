//
// vigilant-canine - Hash Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <core/hash.h>

#include <blake3.h>
#include <hinder/compiler.h>
#include <hinder/exception/exception.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <format>
#include <vector>

namespace vigilant_canine {

    HINDER_DEFINE_EXCEPTION(hash_error, hinder::generic_error);

    namespace {

        // Buffer size for file reading (1 MiB)
        constexpr std::size_t BUFFER_SIZE = 1024 * 1024;

        //
        // Convert binary digest to hex string.
        //
        [[nodiscard]] auto to_hex(std::span<std::byte const> digest) -> std::string {
            std::string hex;
            hex.reserve(digest.size() * 2);
            for (auto const byte : digest) {
                hex += std::format("{:02x}", static_cast<unsigned char>(byte));
            }
            return hex;
        }

        //
        // Hash bytes using BLAKE3.
        //
        [[nodiscard]] auto hash_bytes_blake3(std::span<std::byte const> data) -> HashValue {
            blake3_hasher hasher;
            blake3_hasher_init(&hasher);
            blake3_hasher_update(&hasher, data.data(), data.size());

            std::array<std::byte, BLAKE3_OUT_LEN> output{};
            blake3_hasher_finalize(&hasher, reinterpret_cast<uint8_t*>(output.data()), BLAKE3_OUT_LEN);

            return HashValue{to_hex(output)};
        }

        //
        // Hash bytes using SHA-256.
        //
        [[nodiscard]] auto hash_bytes_sha256(std::span<std::byte const> data) -> HashValue {
            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            HINDER_EXPECTS(ctx != nullptr, hash_error)
                .message("Failed to create OpenSSL EVP context");

            auto cleanup = [&] { EVP_MD_CTX_free(ctx); };

            int rc = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            HINDER_EXPECTS(rc == 1, hash_error)
                .message("Failed to initialize SHA-256 digest");

            rc = EVP_DigestUpdate(ctx, data.data(), data.size());
            HINDER_EXPECTS(rc == 1, hash_error)
                .message("Failed to update SHA-256 digest");

            std::array<std::byte, EVP_MAX_MD_SIZE> output{};
            unsigned int output_len = 0;
            rc = EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(output.data()), &output_len);
            HINDER_EXPECTS(rc == 1, hash_error)
                .message("Failed to finalize SHA-256 digest");

            cleanup();

            return HashValue{to_hex(std::span{output.data(), output_len})};
        }

    }  // anonymous namespace

    auto hash_bytes(std::span<std::byte const> data, HashAlgorithm alg) -> HashValue {
        switch (alg) {
            case HashAlgorithm::blake3:
                return hash_bytes_blake3(data);
            case HashAlgorithm::sha256:
                return hash_bytes_sha256(data);
        }
        HINDER_THROW(hash_error).message("Unknown hash algorithm");
    }

    auto hash_file(FilePath const& path, HashAlgorithm alg)
        -> std::expected<HashValue, std::string> {
        std::ifstream file(*path, std::ios::binary);
        if (!file) {
            return std::unexpected(std::format("Failed to open file: {}", (*path).string()));
        }

        // Read file in chunks and update hash
        std::vector<std::byte> buffer(BUFFER_SIZE);
        std::vector<std::byte> all_data;

        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE);
            std::size_t bytes_read = file.gcount();
            if (bytes_read > 0) {
                all_data.insert(all_data.end(), buffer.begin(), buffer.begin() + bytes_read);
            }
        }

        if (file.bad()) {
            return std::unexpected(std::format("Error reading file: {}", (*path).string()));
        }

        try {
            return hash_bytes(all_data, alg);
        } catch (hash_error const& e) {
            return std::unexpected(std::format("Hash error: {}", e.what()));
        }
    }

    auto algorithm_to_string(HashAlgorithm alg) -> std::string_view {
        switch (alg) {
            case HashAlgorithm::blake3:
                return "blake3";
            case HashAlgorithm::sha256:
                return "sha256";
        }
        return "unknown";
    }

    auto string_to_algorithm(std::string_view str) -> std::expected<HashAlgorithm, std::string> {
        if (str == "blake3") {
            return HashAlgorithm::blake3;
        }
        if (str == "sha256") {
            return HashAlgorithm::sha256;
        }
        return std::unexpected(std::format("Unknown hash algorithm: {}", str));
    }

}  // namespace vigilant_canine
