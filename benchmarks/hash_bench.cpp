//
// vigilant-canine - Hash Benchmarks
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <core/hash.h>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace vigilant_canine {

    //
    // Generate random data for benchmarking.
    //
    static auto generate_random_data(std::size_t size) -> std::vector<std::byte> {
        std::vector<std::byte> data(size);
        std::mt19937_64 rng{42};  // Fixed seed for reproducibility
        std::uniform_int_distribution<unsigned char> dist{0, 255};

        for (auto& byte : data) {
            byte = static_cast<std::byte>(dist(rng));
        }
        return data;
    }

    //
    // Benchmark BLAKE3 throughput at various data sizes.
    //

    static void BM_Blake3_1KB(benchmark::State& state) {
        auto data = generate_random_data(1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::blake3);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Blake3_1KB);

    static void BM_Blake3_1MB(benchmark::State& state) {
        auto data = generate_random_data(1024 * 1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::blake3);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Blake3_1MB);

    static void BM_Blake3_10MB(benchmark::State& state) {
        auto data = generate_random_data(10 * 1024 * 1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::blake3);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Blake3_10MB);

    //
    // Benchmark SHA-256 throughput at various data sizes.
    //

    static void BM_Sha256_1KB(benchmark::State& state) {
        auto data = generate_random_data(1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::sha256);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Sha256_1KB);

    static void BM_Sha256_1MB(benchmark::State& state) {
        auto data = generate_random_data(1024 * 1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::sha256);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Sha256_1MB);

    static void BM_Sha256_10MB(benchmark::State& state) {
        auto data = generate_random_data(10 * 1024 * 1024);
        for (auto _ : state) {
            auto hash = hash_bytes(data, HashAlgorithm::sha256);
            benchmark::DoNotOptimize(hash);
        }
        state.SetBytesProcessed(state.iterations() * data.size());
    }
    BENCHMARK(BM_Sha256_10MB);

}  // namespace vigilant_canine

BENCHMARK_MAIN();
