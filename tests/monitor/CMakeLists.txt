# Fanotify monitor tests

add_executable(fanotify_monitor_test
    fanotify_monitor_test.cpp
    ../../src/monitor/fanotify_monitor.cpp
    ../../src/baseline/strategy.cpp
    ../../src/distro/detector.cpp
    ../../src/storage/baseline_store.cpp
    ../../src/storage/database.cpp
    ../../src/core/hash.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
)

target_link_libraries(fanotify_monitor_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    ${BLAKE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    hinder::hinder
)

target_include_directories(fanotify_monitor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${BLAKE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(fanotify_monitor_test)
