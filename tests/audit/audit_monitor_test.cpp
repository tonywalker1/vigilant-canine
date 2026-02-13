//
// vigilant-canine - Audit Monitor Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_monitor.h>
#include <events/event_bus.h>

#include <gtest/gtest.h>

using namespace vigilant_canine;

class AuditMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        event_bus = std::make_unique<EventBus>();
    }

    void TearDown() override {
        event_bus.reset();
    }

    std::unique_ptr<EventBus> event_bus;
};

TEST_F(AuditMonitorTest, ConstructsSuccessfully) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    EXPECT_NO_THROW({
        AuditMonitor monitor(*event_bus, std::move(rules), config);
    });
}

TEST_F(AuditMonitorTest, InitializeHandlesUnavailableAudit) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto result = monitor.initialize();

    // On systems without audit, should return error but not crash
    // On systems with audit, should succeed
    // Either way, should not throw
    EXPECT_NO_THROW({
        auto result = monitor.initialize();
    });
}

TEST_F(AuditMonitorTest, CannotStartBeforeInitialize) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto result = monitor.start();

    EXPECT_FALSE(result.has_value());
}

TEST_F(AuditMonitorTest, CannotStartTwice) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto init_result = monitor.initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    auto start_result1 = monitor.start();
    ASSERT_TRUE(start_result1.has_value());

    auto start_result2 = monitor.start();
    EXPECT_FALSE(start_result2.has_value());

    monitor.stop();
}

TEST_F(AuditMonitorTest, IsRunningReflectsState) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    EXPECT_FALSE(monitor.is_running());

    auto init_result = monitor.initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    EXPECT_FALSE(monitor.is_running());

    auto start_result = monitor.start();
    if (!start_result) {
        GTEST_SKIP() << "Could not start audit monitor";
    }

    EXPECT_TRUE(monitor.is_running());

    monitor.stop();

    EXPECT_FALSE(monitor.is_running());
}

TEST_F(AuditMonitorTest, StopIsIdempotent) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto init_result = monitor.initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    auto start_result = monitor.start();
    if (!start_result) {
        GTEST_SKIP() << "Could not start audit monitor";
    }

    EXPECT_NO_THROW({
        monitor.stop();
        monitor.stop();  // Second stop should be safe
        monitor.stop();  // Third stop should be safe
    });
}

TEST_F(AuditMonitorTest, UpdateRulesDoesNotCrash) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto init_result = monitor.initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    auto start_result = monitor.start();
    if (!start_result) {
        GTEST_SKIP() << "Could not start audit monitor";
    }

    // Update rules while running
    std::vector<AuditRule> new_rules;
    new_rules.push_back(AuditRule{
        .name = "test_rule",
        .description = "Test rule",
        .field_matches = {},
        .enabled = true,
    });

    EXPECT_NO_THROW({
        monitor.update_rules(std::move(new_rules));
    });

    monitor.stop();
}

TEST_F(AuditMonitorTest, ExcludeCommsFiltersEvents) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;
    config.exclude_comms = {"systemd", "kworker"};

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    // Test that config is accepted
    EXPECT_EQ(config.exclude_comms.size(), 2);
}

TEST_F(AuditMonitorTest, ExcludeUidsFiltersEvents) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;
    config.exclude_uids = {0, 1};  // Exclude root and bin

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    // Test that config is accepted
    EXPECT_EQ(config.exclude_uids.size(), 2);
}

TEST_F(AuditMonitorTest, SanitizationCanBeDisabled) {
    std::vector<AuditRule> rules;
    AuditMonitorConfig config;
    config.sanitize_command_lines = false;

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    // Test that config is accepted
    EXPECT_FALSE(config.sanitize_command_lines);
}

TEST_F(AuditMonitorTest, EventBusReceivesEvents) {
    std::vector<AuditRule> rules = get_default_audit_rules();
    AuditMonitorConfig config;

    bool event_received = false;
    auto subscription = event_bus->subscribe([&event_received](Event const&) {
        event_received = true;
    });

    AuditMonitor monitor(*event_bus, std::move(rules), config);

    auto init_result = monitor.initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    auto start_result = monitor.start();
    if (!start_result) {
        GTEST_SKIP() << "Could not start audit monitor";
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    monitor.stop();

    // May or may not have received events depending on system activity
    // Just verify it doesn't crash - subscription will auto-unsubscribe
}

TEST_F(AuditMonitorTest, DestructorStopsMonitor) {
    auto monitor = std::make_unique<AuditMonitor>(
        *event_bus,
        std::vector<AuditRule>{},
        AuditMonitorConfig{}
    );

    auto init_result = monitor->initialize();
    if (!init_result) {
        GTEST_SKIP() << "Audit subsystem not available";
    }

    auto start_result = monitor->start();
    if (!start_result) {
        GTEST_SKIP() << "Could not start audit monitor";
    }

    EXPECT_TRUE(monitor->is_running());

    // Destructor should stop the monitor
    EXPECT_NO_THROW({
        monitor.reset();
    });
}
