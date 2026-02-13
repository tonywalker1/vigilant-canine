//
// vigilant-canine - Event Bus Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <events/event_bus.h>

#include <gtest/gtest.h>

#include <atomic>

namespace vigilant_canine {

    TEST(EventBusTest, PublishToSubscriber) {
        EventBus bus;

        int call_count = 0;
        auto sub = bus.subscribe([&](Event const& event) {
            call_count++;
            EXPECT_EQ(event.source, "test");
        });

        FileCreatedEvent file_event{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        Event event{file_event, EventSeverity::info, "test"};
        bus.publish(event);

        EXPECT_EQ(call_count, 1);
        EXPECT_EQ(bus.subscription_count(), 1);
    }

    TEST(EventBusTest, MultipleSubscribers) {
        EventBus bus;

        int count1 = 0;
        int count2 = 0;

        auto sub1 = bus.subscribe([&](Event const&) { count1++; });
        auto sub2 = bus.subscribe([&](Event const&) { count2++; });

        FileCreatedEvent file_event{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        Event event{file_event, EventSeverity::info, "test"};
        bus.publish(event);

        EXPECT_EQ(count1, 1);
        EXPECT_EQ(count2, 1);
        EXPECT_EQ(bus.subscription_count(), 2);
    }

    TEST(EventBusTest, Unsubscribe) {
        EventBus bus;

        int call_count = 0;
        auto sub = bus.subscribe([&](Event const&) { call_count++; });

        EXPECT_EQ(bus.subscription_count(), 1);

        FileCreatedEvent file_event{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        Event event{file_event, EventSeverity::info, "test"};

        bus.publish(event);
        EXPECT_EQ(call_count, 1);

        // Unsubscribe
        bus.unsubscribe(sub);
        EXPECT_EQ(bus.subscription_count(), 0);

        // Should not receive this event
        bus.publish(event);
        EXPECT_EQ(call_count, 1);  // Still 1, not 2
    }

    TEST(EventBusTest, SeverityFiltering) {
        EventBus bus;

        int critical_count = 0;
        int warning_count = 0;
        int info_count = 0;

        // Subscribe to critical only
        auto sub1 = bus.subscribe_severity(EventSeverity::critical,
            [&](Event const&) { critical_count++; });

        // Subscribe to warning and above
        auto sub2 = bus.subscribe_severity(EventSeverity::warning,
            [&](Event const&) { warning_count++; });

        // Subscribe to all
        auto sub3 = bus.subscribe([&](Event const&) { info_count++; });

        FileCreatedEvent file_event{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        // Publish info event
        Event info_event{file_event, EventSeverity::info, "test"};
        bus.publish(info_event);

        EXPECT_EQ(critical_count, 0);
        EXPECT_EQ(warning_count, 0);
        EXPECT_EQ(info_count, 1);

        // Publish warning event
        Event warning_event{file_event, EventSeverity::warning, "test"};
        bus.publish(warning_event);

        EXPECT_EQ(critical_count, 0);
        EXPECT_EQ(warning_count, 1);
        EXPECT_EQ(info_count, 2);

        // Publish critical event
        Event critical_event{file_event, EventSeverity::critical, "test"};
        bus.publish(critical_event);

        EXPECT_EQ(critical_count, 1);
        EXPECT_EQ(warning_count, 2);
        EXPECT_EQ(info_count, 3);
    }

    TEST(EventBusTest, EventTypeNames) {
        FileCreatedEvent file_created{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        FileModifiedEvent file_modified{
            FilePath{"/test/file.txt"},
            HashValue{"old"},
            HashValue{"new"},
            "content changed"
        };

        // Phase 2 events
        AuthFailureEvent auth_failure{
            "testuser",
            "sshd",
            "192.168.1.100",
            "Failed password for testuser"
        };

        PrivilegeEscalationEvent priv_esc{
            "testuser",
            "root",
            "sudo",
            "/usr/bin/systemctl restart httpd",
            "testuser : TTY=pts/0 ; PWD=/home/testuser ; USER=root ; COMMAND=/usr/bin/systemctl restart httpd"
        };

        ServiceStateEvent service_state{
            "sshd.service",
            "failed",
            "1",
            "sshd.service: Main process exited, code=exited, status=1/FAILURE"
        };

        SuspiciousLogEvent suspicious_log{
            "kernel_segfault",
            "kernel",
            "segfault at 0 ip 00007f8b9c9d9e20 sp 00007ffce8b0e8a0 error 4 in libc.so.6",
            3  // LOG_ERR
        };

        EXPECT_EQ(event_type_name(file_created), "FileCreated");
        EXPECT_EQ(event_type_name(file_modified), "FileModified");
        EXPECT_EQ(event_type_name(auth_failure), "AuthFailure");
        EXPECT_EQ(event_type_name(priv_esc), "PrivilegeEscalation");
        EXPECT_EQ(event_type_name(service_state), "ServiceState");
        EXPECT_EQ(event_type_name(suspicious_log), "SuspiciousLog");
    }

    TEST(EventBusTest, SeverityNames) {
        EXPECT_EQ(severity_name(EventSeverity::info), "info");
        EXPECT_EQ(severity_name(EventSeverity::warning), "warning");
        EXPECT_EQ(severity_name(EventSeverity::critical), "critical");
    }

    TEST(EventBusTest, HandlerException) {
        EventBus bus;

        int good_handler_count = 0;

        // Handler that throws
        auto sub1 = bus.subscribe([](Event const&) {
            throw std::runtime_error("Test exception");
        });

        // Handler that should still be called
        auto sub2 = bus.subscribe([&](Event const&) {
            good_handler_count++;
        });

        FileCreatedEvent file_event{
            FilePath{"/test/file.txt"},
            HashValue{"abc123"},
            std::nullopt
        };

        Event event{file_event, EventSeverity::info, "test"};

        // Should not throw - exception caught internally
        EXPECT_NO_THROW(bus.publish(event));

        // Good handler should still have been called
        EXPECT_EQ(good_handler_count, 1);
    }

}  // namespace vigilant_canine
