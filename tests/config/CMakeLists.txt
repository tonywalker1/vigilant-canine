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
)

if(HINDER_FOUND)
    target_link_libraries(config_test PRIVATE hinder::hinder)
endif()

target_include_directories(config_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(config_test)
