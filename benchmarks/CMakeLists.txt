# Benchmarks

add_executable(hash_bench
    hash_bench.cpp
    ../src/core/hash.cpp
)

target_link_libraries(hash_bench PRIVATE
    benchmark::benchmark
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    hinder::hinder
)

target_include_directories(hash_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)
