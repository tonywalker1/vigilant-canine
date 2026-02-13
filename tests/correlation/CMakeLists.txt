# Correlation tests

add_executable(correlation_engine_test
    correlation_engine_test.cpp
    ../../src/correlation/correlation_engine.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(correlation_engine_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(correlation_engine_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

gtest_discover_tests(correlation_engine_test)
