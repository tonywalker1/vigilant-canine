#
# Power Monitor Tests
#

add_executable(power_monitor_test
    power_monitor_test.cpp
    ${CMAKE_SOURCE_DIR}/src/power/power_monitor.cpp
)

target_include_directories(power_monitor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(power_monitor_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME power_monitor_test COMMAND power_monitor_test)
