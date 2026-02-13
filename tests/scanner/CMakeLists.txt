# Scanner tests

add_executable(scanner_test
    scanner_test.cpp
    ../../src/scanner/scanner.cpp
    ../../src/baseline/strategy.cpp
    ../../src/distro/detector.cpp
    ../../src/storage/baseline_store.cpp
    ../../src/storage/database.cpp
    ../../src/core/hash.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(scanner_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    ${BLAKE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

if(HINDER_FOUND)
    target_link_libraries(scanner_test PRIVATE hinder::hinder)
endif()

target_include_directories(scanner_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${BLAKE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(scanner_test)
