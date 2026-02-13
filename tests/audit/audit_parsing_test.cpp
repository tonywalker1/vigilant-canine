//
// vigilant-canine - Audit Parsing Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <audit/audit_parsing.h>

#include <gtest/gtest.h>

using namespace vigilant_canine;

TEST(AuditParsingTest, SanitizeCommandLinePassword) {
    std::string cmdline = "mysql -u root -p'secret123'";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, "mysql -u root -p'[REDACTED]'");
}

TEST(AuditParsingTest, SanitizeCommandLineLongPasswordFlag) {
    std::string cmdline = "mysql --password=secret123 -u root";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, "mysql --password=[REDACTED] -u root");
}

TEST(AuditParsingTest, SanitizeCommandLineURLCredentials) {
    std::string cmdline = "git clone https://user:password@github.com/repo.git";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, "git clone https://user:[REDACTED]@github.com/repo.git");
}

TEST(AuditParsingTest, SanitizeCommandLineEnvironmentVariables) {
    std::string cmdline = "SECRET_KEY=abc123 API_TOKEN=xyz789 /usr/bin/app";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, "SECRET_KEY=[REDACTED] API_TOKEN=[REDACTED] /usr/bin/app");
}

TEST(AuditParsingTest, SanitizeCommandLineTokenFlag) {
    std::string cmdline = "gh auth login --token ghp_1234567890";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, "gh auth login --token=[REDACTED]");
}

TEST(AuditParsingTest, SanitizeCommandLineDisabled) {
    std::string cmdline = "mysql -u root -p'secret123'";
    SanitizationConfig config{.enabled = false};
    auto sanitized = sanitize_command_line(cmdline, config);

    EXPECT_EQ(sanitized, cmdline);  // Unchanged
}

TEST(AuditParsingTest, SanitizeCommandLineNoSecrets) {
    std::string cmdline = "ls -la /home/user";
    auto sanitized = sanitize_command_line(cmdline);

    EXPECT_EQ(sanitized, cmdline);  // Unchanged
}

TEST(AuditParsingTest, JoinArgvSimple) {
    std::vector<std::string> argv = {"ls", "-la", "/home"};
    auto cmdline = join_argv(argv);

    EXPECT_EQ(cmdline, "ls -la /home");
}

TEST(AuditParsingTest, JoinArgvWithSpaces) {
    std::vector<std::string> argv = {"echo", "hello world", "foo"};
    auto cmdline = join_argv(argv);

    EXPECT_EQ(cmdline, "echo \"hello world\" foo");
}

TEST(AuditParsingTest, JoinArgvEmpty) {
    std::vector<std::string> argv;
    auto cmdline = join_argv(argv);

    EXPECT_EQ(cmdline, "");
}

TEST(AuditParsingTest, IsEventCompleteWithSyscallAndExecve) {
    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.pid = 1234};
    event.execve = ExecveRecord{.argv = {"ls"}};

    EXPECT_TRUE(is_event_complete(event));
}

TEST(AuditParsingTest, IsEventCompleteWithSyscallAndPath) {
    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.pid = 1234};
    event.paths.push_back(PathRecord{.name = "/etc/passwd"});

    EXPECT_TRUE(is_event_complete(event));
}

TEST(AuditParsingTest, IsEventIncompleteWithoutSyscall) {
    AuditEventAccumulator event;
    event.execve = ExecveRecord{.argv = {"ls"}};

    EXPECT_FALSE(is_event_complete(event));
}

TEST(AuditParsingTest, IsEventIncompleteWithoutExecveOrPath) {
    AuditEventAccumulator event;
    event.syscall = SyscallRecord{.pid = 1234};

    EXPECT_FALSE(is_event_complete(event));
}

TEST(AuditParsingTest, IncompleteEventTimeoutConstant) {
    EXPECT_EQ(INCOMPLETE_EVENT_TIMEOUT, std::chrono::milliseconds(100));
}
