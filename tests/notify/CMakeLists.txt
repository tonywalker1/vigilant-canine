# Notify tests

add_executable(dbus_notifier_test
    dbus_notifier_test.cpp
    ../../src/notify/dbus_notifier.cpp
    ../../src/storage/alert_store.cpp
    ../../src/storage/database.cpp
)

target_link_libraries(dbus_notifier_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    SQLite::SQLite3
    ${SYSTEMD_LIBRARIES}
)

if(TARGET hinder::hinder)
    target_link_libraries(dbus_notifier_test PRIVATE hinder::hinder)
endif()

target_include_directories(dbus_notifier_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${SYSTEMD_INCLUDE_DIRS}
)

gtest_discover_tests(dbus_notifier_test)
