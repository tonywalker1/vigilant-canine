# Journal tests

add_executable(journal_rule_test
    journal_rule_test.cpp
    ../../src/journal/journal_rule.cpp
    ../../src/events/event.cpp
)

target_link_libraries(journal_rule_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(journal_rule_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

gtest_discover_tests(journal_rule_test)

# Journal monitor tests
add_executable(journal_monitor_test
    journal_monitor_test.cpp
    ../../src/journal/journal_monitor.cpp
    ../../src/journal/journal_rule.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(journal_monitor_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    ${SYSTEMD_LIBRARIES}
)

target_include_directories(journal_monitor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${SYSTEMD_INCLUDE_DIRS}
)

gtest_discover_tests(journal_monitor_test)
