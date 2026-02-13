# Alert dispatcher tests

add_executable(alert_dispatcher_test
    alert_dispatcher_test.cpp
    ../../src/dispatch/alert_dispatcher.cpp
    ../../src/events/event.cpp
    ../../src/events/event_bus.cpp
    ../../src/storage/alert_store.cpp
    ../../src/storage/database.cpp
    ../../src/core/hash.cpp
    ../../src/distro/detector.cpp
    ../../src/notify/dbus_notifier.cpp
)

target_link_libraries(alert_dispatcher_test PRIVATE
    GTest::gtest_main
    SQLite::SQLite3
    ${SYSTEMD_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

if(HINDER_FOUND)
    target_link_libraries(alert_dispatcher_test PRIVATE hinder::hinder)
endif()

target_include_directories(alert_dispatcher_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${SYSTEMD_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(alert_dispatcher_test)
