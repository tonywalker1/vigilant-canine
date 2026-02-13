# Event system tests

add_executable(event_bus_test
    event_bus_test.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(event_bus_test PRIVATE
    GTest::gtest_main
)

target_include_directories(event_bus_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(event_bus_test)
