//
// vigilant-canine - Correlation Engine Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <correlation/correlation_engine.h>

#include <gtest/gtest.h>

#include <thread>

namespace vigilant_canine {

    TEST(CorrelationEngineTest, Construction) {
        EventBus bus;
        std::vector<CorrelationRule> rules;

        CorrelationEngine engine(bus, std::move(rules));

        // Should construct without error
        SUCCEED();
    }

    TEST(CorrelationEngineTest, StartStop) {
        EventBus bus;
        std::vector<CorrelationRule> rules;

        CorrelationEngine engine(bus, std::move(rules));

        auto result = engine.start();
        ASSERT_TRUE(result.has_value());

        engine.stop();
    }

    TEST(CorrelationEngineTest, ThresholdTrigger) {
        EventBus bus;

        // Rule: 3 FileCreated events in 60 seconds
        CorrelationRule rule{
            .name = "test_rule",
            .event_match = "FileCreated",
            .threshold = 3,
            .window = std::chrono::seconds(60),
            .escalated_severity = EventSeverity::critical
        };

        std::vector<CorrelationRule> rules;
        rules.push_back(rule);

        CorrelationEngine engine(bus, std::move(rules));
        ASSERT_TRUE(engine.start().has_value());

        // Publish 3 FileCreated events
        for (int i = 0; i < 3; ++i) {
            FileCreatedEvent file_event{
                FilePath{std::format("/tmp/file{}.txt", i)},
                HashValue{"hash"},
                std::nullopt
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        // Drain escalated events
        int escalated_count = 0;
        auto sub = bus.subscribe([&](Event const& e) {
            if (e.source == "correlation_engine") {
                escalated_count++;
            }
        });

        engine.drain_escalated_events(bus);

        // Should have triggered exactly once
        EXPECT_EQ(escalated_count, 1);

        engine.stop();
    }

    TEST(CorrelationEngineTest, BelowThreshold) {
        EventBus bus;

        CorrelationRule rule{
            .name = "test_rule",
            .event_match = "FileCreated",
            .threshold = 5,
            .window = std::chrono::seconds(60)
        };

        std::vector<CorrelationRule> rules;
        rules.push_back(rule);

        CorrelationEngine engine(bus, std::move(rules));
        ASSERT_TRUE(engine.start().has_value());

        // Publish only 3 events (below threshold of 5)
        for (int i = 0; i < 3; ++i) {
            FileCreatedEvent file_event{
                FilePath{"/tmp/file.txt"},
                HashValue{"hash"},
                std::nullopt
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        int escalated_count = 0;
        auto sub = bus.subscribe([&](Event const& e) {
            if (e.source == "correlation_engine") {
                escalated_count++;
            }
        });

        engine.drain_escalated_events(bus);

        // Should NOT have triggered
        EXPECT_EQ(escalated_count, 0);

        engine.stop();
    }

    TEST(CorrelationEngineTest, Windowing) {
        EventBus bus;

        CorrelationRule rule{
            .name = "test_rule",
            .event_match = "FileModified",
            .threshold = 2,
            .window = std::chrono::seconds(1),  // Short window
            .escalated_severity = EventSeverity::critical
        };

        std::vector<CorrelationRule> rules;
        rules.push_back(rule);

        CorrelationEngine engine(bus, std::move(rules));
        ASSERT_TRUE(engine.start().has_value());

        // Publish 1 event
        {
            FileModifiedEvent file_event{
                FilePath{"/tmp/file.txt"},
                HashValue{"old"},
                HashValue{"new"},
                "Modified"
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        // Wait for window to expire
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        // Publish another event (outside window - new Event object with new timestamp)
        {
            FileModifiedEvent file_event{
                FilePath{"/tmp/file.txt"},
                HashValue{"old"},
                HashValue{"new"},
                "Modified"
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        int escalated_count = 0;
        auto sub = bus.subscribe([&](Event const& e) {
            if (e.source == "correlation_engine") {
                escalated_count++;
            }
        });

        engine.drain_escalated_events(bus);

        // Should NOT have triggered (events outside window)
        EXPECT_EQ(escalated_count, 0);

        engine.stop();
    }

    TEST(CorrelationEngineTest, Debounce) {
        EventBus bus;

        CorrelationRule rule{
            .name = "test_rule",
            .event_match = "FileCreated",
            .threshold = 2,
            .window = std::chrono::seconds(60)
        };

        std::vector<CorrelationRule> rules;
        rules.push_back(rule);

        CorrelationEngine engine(bus, std::move(rules));
        ASSERT_TRUE(engine.start().has_value());

        int escalated_count = 0;
        auto sub = bus.subscribe([&](Event const& e) {
            if (e.source == "correlation_engine") {
                escalated_count++;
            }
        });

        // First trigger
        for (int i = 0; i < 2; ++i) {
            FileCreatedEvent file_event{
                FilePath{"/tmp/file.txt"},
                HashValue{"hash"},
                std::nullopt
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        engine.drain_escalated_events(bus);
        EXPECT_EQ(escalated_count, 1);

        // Second trigger immediately after (should be debounced)
        for (int i = 0; i < 2; ++i) {
            FileCreatedEvent file_event{
                FilePath{"/tmp/file.txt"},
                HashValue{"hash"},
                std::nullopt
            };
            Event event{file_event, EventSeverity::warning, "test"};
            bus.publish(event);
        }

        engine.drain_escalated_events(bus);
        EXPECT_EQ(escalated_count, 1);  // Still 1, debounced

        engine.stop();
    }

    TEST(CorrelationEngineTest, UpdateRules) {
        EventBus bus;
        std::vector<CorrelationRule> rules;

        CorrelationEngine engine(bus, std::move(rules));

        // Update rules (hot reload scenario)
        std::vector<CorrelationRule> new_rules;
        new_rules.push_back(CorrelationRule{
            .name = "new_rule",
            .event_match = "FileDeleted",
            .threshold = 10
        });

        EXPECT_NO_THROW(engine.update_rules(std::move(new_rules)));
    }

    TEST(CorrelationEngineTest, SelfEventIgnored) {
        EventBus bus;

        CorrelationRule rule{
            .name = "test_rule",
            .event_match = "SuspiciousLog",
            .threshold = 1,  // Very low threshold
            .window = std::chrono::seconds(60)
        };

        std::vector<CorrelationRule> rules;
        rules.push_back(rule);

        CorrelationEngine engine(bus, std::move(rules));
        ASSERT_TRUE(engine.start().has_value());

        // Publish event from correlation_engine itself
        SuspiciousLogEvent log_event{
            .rule_name = "test",
            .unit_name = "test",
            .message = "test",
            .priority = 3
        };
        Event event{log_event, EventSeverity::warning, "correlation_engine"};
        bus.publish(event);

        int escalated_count = 0;
        auto sub = bus.subscribe([&](Event const& e) {
            if (e.source == "correlation_engine") {
                escalated_count++;
            }
        });

        engine.drain_escalated_events(bus);

        // Should NOT trigger (self-events ignored to prevent infinite loop)
        EXPECT_EQ(escalated_count, 0);

        engine.stop();
    }

}  // namespace vigilant_canine
