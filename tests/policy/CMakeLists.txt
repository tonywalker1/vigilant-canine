# Policy engine tests

add_executable(policy_engine_test
    policy_engine_test.cpp
    ../../src/policy/policy_engine.cpp
    ../../src/events/event.cpp
    ../../src/distro/detector.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(policy_engine_test PRIVATE
    GTest::gtest_main
    ${BLAKE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    hinder::hinder
)

target_include_directories(policy_engine_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${BLAKE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(policy_engine_test)
