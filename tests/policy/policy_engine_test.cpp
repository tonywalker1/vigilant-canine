//
// vigilant-canine - Policy Engine Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <policy/policy_engine.h>

#include <gtest/gtest.h>

namespace vigilant_canine {

    class PolicyEngineTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Create a basic policy configuration
            m_config.path_rules = {
                {"/usr/bin/*", AlertSeverity::CRITICAL, true},
                {"/etc/*", AlertSeverity::CRITICAL, true},
                {"/tmp/*", AlertSeverity::INFO, false},
                {"/var/log/*", AlertSeverity::INFO, false},
            };
            m_config.alert_on_unknown = true;
            m_config.default_severity = AlertSeverity::WARNING;

            m_engine = std::make_unique<PolicyEngine>(m_config);
        }

        PolicyConfig m_config;
        std::unique_ptr<PolicyEngine> m_engine;
    };

    TEST_F(PolicyEngineTest, ConstructWithDefaultConfig) {
        PolicyEngine engine;
        // Default constructor creates empty policy
        EXPECT_TRUE(engine.config().path_rules.empty());
        EXPECT_TRUE(engine.config().alert_on_unknown);
    }

    TEST_F(PolicyEngineTest, CriticalPathGeneratesAlert) {
        FilePath path{std::filesystem::path{"/usr/bin/bash"}};
        HashValue old_hash{"sha256:abc123"};
        HashValue new_hash{"sha256:def456"};

        FileModifiedEvent event_data{path, old_hash, new_hash, "Modified"};
        Event event{event_data, EventSeverity::critical, "test"};

        auto decision = m_engine->evaluate(event);

        EXPECT_TRUE(decision.generate_alert);
        EXPECT_EQ(decision.severity, AlertSeverity::CRITICAL);
        ASSERT_TRUE(decision.reason.has_value());
        EXPECT_NE(decision.reason->find("/usr/bin/*"), std::string::npos);
    }

    TEST_F(PolicyEngineTest, TempPathSuppressesAlert) {
        FilePath path{std::filesystem::path{"/tmp/tempfile.txt"}};
        HashValue hash{"sha256:xyz789"};

        FileCreatedEvent event_data{path, hash, std::nullopt};
        Event event{event_data, EventSeverity::warning, "test"};

        auto decision = m_engine->evaluate(event);

        EXPECT_FALSE(decision.generate_alert);
        EXPECT_EQ(decision.severity, AlertSeverity::INFO);
    }

    TEST_F(PolicyEngineTest, EtcPathGeneratesAlert) {
        FilePath path{std::filesystem::path{"/etc/passwd"}};
        HashValue old_hash{"sha256:111"};
        HashValue new_hash{"sha256:222"};

        FileModifiedEvent event_data{path, old_hash, new_hash, "Modified"};
        Event event{event_data, EventSeverity::critical, "test"};

        auto decision = m_engine->evaluate(event);

        EXPECT_TRUE(decision.generate_alert);
        EXPECT_EQ(decision.severity, AlertSeverity::CRITICAL);
    }

    TEST_F(PolicyEngineTest, UnknownPathUsesDefault) {
        FilePath path{std::filesystem::path{"/home/user/document.txt"}};
        HashValue hash{"sha256:aaa"};

        FileCreatedEvent event_data{path, hash, std::nullopt};
        Event event{event_data, EventSeverity::warning, "test"};

        auto decision = m_engine->evaluate(event);

        EXPECT_TRUE(decision.generate_alert);  // alert_on_unknown is true
        EXPECT_EQ(decision.severity, AlertSeverity::WARNING);
        ASSERT_TRUE(decision.reason.has_value());
        EXPECT_NE(decision.reason->find("default"), std::string::npos);
    }

    TEST_F(PolicyEngineTest, NonFileEventUsesDefault) {
        ScanCompletedEvent event_data{
            std::filesystem::path{"/usr"},
            1000,
            5,
            std::chrono::milliseconds{500}
        };

        Event event{event_data, EventSeverity::info, "scanner"};

        auto decision = m_engine->evaluate(event);

        EXPECT_TRUE(decision.generate_alert);
        EXPECT_EQ(decision.severity, AlertSeverity::INFO);
    }

    TEST_F(PolicyEngineTest, UpdateConfig) {
        PolicyConfig new_config;
        new_config.path_rules = {
            {"/opt/*", AlertSeverity::WARNING, true}
        };
        new_config.alert_on_unknown = false;

        m_engine->set_config(new_config);

        FilePath path{std::filesystem::path{"/home/user/file.txt"}};
        HashValue hash{"sha256:bbb"};
        FileCreatedEvent event_data{path, hash, std::nullopt};
        Event event{event_data, EventSeverity::warning, "test"};

        auto decision = m_engine->evaluate(event);

        EXPECT_FALSE(decision.generate_alert);  // alert_on_unknown is now false
    }

    TEST_F(PolicyEngineTest, CreateDefaultPolicyTraditional) {
        auto policy = create_default_policy(DistroType::traditional);

        EXPECT_FALSE(policy.path_rules.empty());
        EXPECT_TRUE(policy.alert_on_unknown);

        // Check for expected critical paths
        auto has_usr_bin = std::any_of(policy.path_rules.begin(),
                                        policy.path_rules.end(),
                                        [](auto const& rule) {
                                            return rule.pattern == "/usr/bin/*" &&
                                                   rule.severity == AlertSeverity::CRITICAL;
                                        });
        EXPECT_TRUE(has_usr_bin);
    }

    TEST_F(PolicyEngineTest, CreateDefaultPolicyOstree) {
        auto policy = create_default_policy(DistroType::ostree);

        // Should have ostree-specific rules
        auto has_ostree_rule = std::any_of(policy.path_rules.begin(),
                                            policy.path_rules.end(),
                                            [](auto const& rule) {
                                                return rule.pattern.find("ostree") != std::string::npos;
                                            });
        EXPECT_TRUE(has_ostree_rule);
    }

    TEST_F(PolicyEngineTest, FirstMatchingRuleWins) {
        // Create config with overlapping rules
        PolicyConfig config;
        config.path_rules = {
            {"/usr/bin/test", AlertSeverity::INFO, false},       // Specific file, no alert
            {"/usr/bin/*", AlertSeverity::CRITICAL, true},       // General pattern, alert
        };
        config.alert_on_unknown = true;

        PolicyEngine engine{config};

        FilePath path{std::filesystem::path{"/usr/bin/test"}};
        HashValue old_hash{"sha256:xxx"};
        HashValue new_hash{"sha256:yyy"};
        FileModifiedEvent event_data{path, old_hash, new_hash, "Modified"};
        Event event{event_data, EventSeverity::critical, "test"};

        auto decision = engine.evaluate(event);

        // First rule should match (specific file)
        EXPECT_FALSE(decision.generate_alert);
        EXPECT_EQ(decision.severity, AlertSeverity::INFO);
    }

}  // namespace vigilant_canine
