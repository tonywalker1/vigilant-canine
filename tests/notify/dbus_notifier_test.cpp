//
// vigilant-canine - D-Bus Notifier Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <notify/dbus_notifier.h>

#include <gtest/gtest.h>

namespace vigilant_canine {

    TEST(DbusNotifierTest, Construction) {
        DbusNotifier notifier;

        EXPECT_FALSE(notifier.is_available());
    }

    TEST(DbusNotifierTest, InitializeGraceful) {
        DbusNotifier notifier;

        // Initialize may succeed or fail depending on environment
        auto result = notifier.initialize();

        // Either way, should not crash
        if (result) {
            // D-Bus available
            EXPECT_TRUE(notifier.is_available());
        } else {
            // D-Bus not available (headless system, no session bus)
            EXPECT_FALSE(notifier.is_available());
            EXPECT_FALSE(result.error().empty());
        }
    }

    TEST(DbusNotifierTest, NotifyWithoutInit) {
        DbusNotifier notifier;

        Alert alert{
            .severity = AlertSeverity::WARNING,
            .category = "test",
            .summary = "Test alert",
            .source = "test"
        };

        // Should not crash even without initialization
        EXPECT_NO_THROW(notifier.notify(alert));
    }

    TEST(DbusNotifierTest, NotifyAfterInit) {
        DbusNotifier notifier;

        auto result = notifier.initialize();
        if (!result) {
            GTEST_SKIP() << "D-Bus not available: " << result.error();
        }

        Alert alert{
            .severity = AlertSeverity::CRITICAL,
            .category = "file_modified",
            .path = FilePath{"/etc/passwd"},
            .summary = "Critical file modified",
            .details = "System password file was modified",
            .source = "fanotify"
        };

        // Should not crash
        EXPECT_NO_THROW(notifier.notify(alert));
    }

    TEST(DbusNotifierTest, MultipleSeverities) {
        DbusNotifier notifier;

        auto result = notifier.initialize();
        if (!result) {
            GTEST_SKIP() << "D-Bus not available: " << result.error();
        }

        // Test each severity level
        for (auto severity : {AlertSeverity::INFO, AlertSeverity::WARNING, AlertSeverity::CRITICAL}) {
            Alert alert{
                .severity = severity,
                .category = "test",
                .summary = "Test notification",
                .source = "test"
            };

            EXPECT_NO_THROW(notifier.notify(alert));
        }
    }

    TEST(DbusNotifierTest, NonCopyable) {
        EXPECT_FALSE(std::is_copy_constructible_v<DbusNotifier>);
        EXPECT_FALSE(std::is_copy_assignable_v<DbusNotifier>);
    }

    TEST(DbusNotifierTest, NonMovable) {
        EXPECT_FALSE(std::is_move_constructible_v<DbusNotifier>);
        EXPECT_FALSE(std::is_move_assignable_v<DbusNotifier>);
    }

}  // namespace vigilant_canine
