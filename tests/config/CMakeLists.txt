# Config tests

add_executable(config_test
    config_test.cpp
    ../../src/config/config.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(config_test PRIVATE
    GTest::gtest_main
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    tomlplusplus::tomlplusplus
    hinder::hinder
)

target_include_directories(config_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(config_test)

# Policy config tests

add_executable(policy_config_test
    policy_config_test.cpp
    ../../src/config/config.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(policy_config_test PRIVATE
    GTest::gtest_main
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    tomlplusplus::tomlplusplus
    hinder::hinder
)

target_include_directories(policy_config_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

gtest_discover_tests(policy_config_test)

# Config merge tests

add_executable(config_merge_test
    config_merge_test.cpp
    ../../src/config/config.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(config_merge_test PRIVATE
    GTest::gtest_main
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    tomlplusplus::tomlplusplus
    hinder::hinder
)

target_include_directories(config_merge_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

gtest_discover_tests(config_merge_test)
