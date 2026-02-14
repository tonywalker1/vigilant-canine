# User tests

add_executable(user_manager_test
    user_manager_test.cpp
    ../../src/user/user_manager.cpp
    ../../src/config/config.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(user_manager_test PRIVATE
    GTest::gtest_main
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    tomlplusplus::tomlplusplus
    hinder::hinder
)

target_include_directories(user_manager_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(user_manager_test)
