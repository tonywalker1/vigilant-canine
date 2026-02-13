# Audit tests

add_executable(audit_rule_test
    audit_rule_test.cpp
    ../../src/audit/audit_rule.cpp
    ../../src/events/event.cpp
)

target_link_libraries(audit_rule_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(audit_rule_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

gtest_discover_tests(audit_rule_test)

# Audit parsing tests
add_executable(audit_parsing_test
    audit_parsing_test.cpp
    ../../src/audit/audit_parsing.cpp
)

target_link_libraries(audit_parsing_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    ${AUDIT_LIBRARIES}
)

target_include_directories(audit_parsing_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${AUDIT_INCLUDE_DIRS}
)

gtest_discover_tests(audit_parsing_test)

# Audit monitor tests
add_executable(audit_monitor_test
    audit_monitor_test.cpp
    ../../src/audit/audit_monitor.cpp
    ../../src/audit/audit_rule.cpp
    ../../src/audit/audit_parsing.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(audit_monitor_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    ${AUDIT_LIBRARIES}
    ${SYSTEMD_LIBRARIES}
)

target_include_directories(audit_monitor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${AUDIT_INCLUDE_DIRS}
)

gtest_discover_tests(audit_monitor_test)
