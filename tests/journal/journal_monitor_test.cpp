//
// vigilant-canine - Journal Monitor Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <journal/journal_monitor.h>

#include <gtest/gtest.h>

namespace vigilant_canine {

    TEST(JournalMonitorTest, Construction) {
        EventBus bus;
        std::vector<JournalRule> rules;

        JournalMonitor monitor(bus, std::move(rules));

        EXPECT_FALSE(monitor.is_running());
    }

    TEST(JournalMonitorTest, InitializeSuccess) {
        EventBus bus;
        auto rules = get_default_rules();

        JournalMonitor monitor(bus, std::move(rules));

        // Initialize should succeed (requires systemd journal)
        auto result = monitor.initialize();
        
        // On systems with journal, this should succeed
        // On systems without, it should fail gracefully
        if (result) {
            EXPECT_TRUE(result.has_value());
        } else {
            // Graceful failure - error message present
            EXPECT_FALSE(result.error().empty());
        }
    }

    TEST(JournalMonitorTest, StartStopLifecycle) {
        EventBus bus;
        auto rules = get_default_rules();

        JournalMonitor monitor(bus, std::move(rules));

        auto init_result = monitor.initialize();
        if (!init_result) {
            GTEST_SKIP() << "Journal not available: " << init_result.error();
        }

        auto start_result = monitor.start();
        ASSERT_TRUE(start_result.has_value());
        EXPECT_TRUE(monitor.is_running());

        // Stop monitor
        monitor.stop();
        EXPECT_FALSE(monitor.is_running());
    }

    TEST(JournalMonitorTest, CannotStartWithoutInit) {
        EventBus bus;
        auto rules = get_default_rules();

        JournalMonitor monitor(bus, std::move(rules));

        // Start without initialize should fail
        auto result = monitor.start();
        EXPECT_FALSE(result.has_value());
        EXPECT_FALSE(monitor.is_running());
    }

    TEST(JournalMonitorTest, UpdateRules) {
        EventBus bus;
        auto rules = get_default_rules();

        JournalMonitor monitor(bus, std::move(rules));

        // Update rules (hot reload scenario)
        std::vector<JournalRule> new_rules;
        new_rules.push_back(JournalRule{
            .name = "test_rule",
            .description = "Test rule",
            .field_matches = {},
            .enabled = true
        });

        // Should not crash
        EXPECT_NO_THROW(monitor.update_rules(std::move(new_rules)));
    }

    TEST(JournalMonitorTest, ExcludeConfiguration) {
        EventBus bus;
        auto rules = get_default_rules();

        JournalMonitorConfig config{
            .max_priority = 6,
            .exclude_units = {"systemd-journald.service"},
            .exclude_identifiers = {"systemd"}
        };

        JournalMonitor monitor(bus, std::move(rules), config);

        EXPECT_FALSE(monitor.is_running());
    }

    TEST(JournalMonitorTest, NonCopyable) {
        EXPECT_FALSE(std::is_copy_constructible_v<JournalMonitor>);
        EXPECT_FALSE(std::is_copy_assignable_v<JournalMonitor>);
    }

    TEST(JournalMonitorTest, NonMovable) {
        EXPECT_FALSE(std::is_move_constructible_v<JournalMonitor>);
        EXPECT_FALSE(std::is_move_assignable_v<JournalMonitor>);
    }

}  // namespace vigilant_canine
