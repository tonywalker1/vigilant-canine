# Storage tests

add_executable(database_test
    database_test.cpp
    ../../src/storage/database.cpp
    ../../src/storage/baseline_store.cpp
    ../../src/storage/alert_store.cpp
)

target_link_libraries(database_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    hinder::hinder
)

target_include_directories(database_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

add_executable(journal_event_store_test
    journal_event_store_test.cpp
    ../../src/storage/database.cpp
    ../../src/storage/journal_event_store.cpp
)

target_link_libraries(journal_event_store_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    hinder::hinder
)

target_include_directories(journal_event_store_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

add_executable(scan_store_test
    scan_store_test.cpp
    ../../src/storage/database.cpp
    ../../src/storage/scan_store.cpp
)

target_link_libraries(scan_store_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    hinder::hinder
)

target_include_directories(scan_store_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

add_executable(alert_store_test
    alert_store_test.cpp
    ../../src/storage/database.cpp
    ../../src/storage/alert_store.cpp
)

target_link_libraries(alert_store_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    hinder::hinder
)

target_include_directories(alert_store_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(database_test)
gtest_discover_tests(journal_event_store_test)
gtest_discover_tests(scan_store_test)
gtest_discover_tests(alert_store_test)
