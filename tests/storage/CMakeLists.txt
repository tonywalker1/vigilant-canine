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
)

if(HINDER_FOUND)
    target_link_libraries(database_test PRIVATE hinder::hinder)
endif()

target_include_directories(database_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(database_test)
