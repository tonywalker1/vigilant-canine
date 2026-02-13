# Baseline strategy tests

add_executable(strategy_test
    strategy_test.cpp
    ../../src/baseline/strategy.cpp
    ../../src/distro/detector.cpp
)

target_link_libraries(strategy_test PRIVATE
    GTest::gtest_main
)

target_include_directories(strategy_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(strategy_test)
