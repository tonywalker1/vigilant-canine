// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "json.h"

#include <sstream>

namespace vigilant_canine::api::json {

std::string escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
        case '"':
            oss << "\\\"";
            break;
        case '\\':
            oss << "\\\\";
            break;
        case '\b':
            oss << "\\b";
            break;
        case '\f':
            oss << "\\f";
            break;
        case '\n':
            oss << "\\n";
            break;
        case '\r':
            oss << "\\r";
            break;
        case '\t':
            oss << "\\t";
            break;
        default:
            if (c < 0x20) {
                // Control characters
                oss << "\\u00" << std::hex << static_cast<int>(c);
            } else {
                oss << c;
            }
            break;
        }
    }
    return oss.str();
}

std::string to_json(const Alert& alert) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":" << alert.id << ",";
    oss << "\"severity\":\"" << to_string(alert.severity) << "\",";
    oss << "\"category\":\"" << escape(alert.category) << "\",";

    if (alert.path) {
        oss << "\"path\":\"" << escape((*alert.path).value.string()) << "\",";
    } else {
        oss << "\"path\":null,";
    }

    oss << "\"summary\":\"" << escape(alert.summary) << "\",";

    if (alert.details) {
        oss << "\"details\":\"" << escape(*alert.details) << "\",";
    } else {
        oss << "\"details\":null,";
    }

    oss << "\"source\":\"" << escape(alert.source) << "\",";
    oss << "\"acknowledged\":" << (alert.acknowledged ? "true" : "false");
    oss << "}";

    return oss.str();
}

std::string to_json(const std::vector<Alert>& alerts) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < alerts.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << to_json(alerts[i]);
    }
    oss << "]";
    return oss.str();
}

std::string to_json(const Baseline& baseline) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":" << baseline.id << ",";
    oss << "\"path\":\"" << escape(baseline.path.value.string()) << "\",";
    oss << "\"hash_alg\":\"" << escape(baseline.hash_alg) << "\",";
    oss << "\"hash_value\":\"" << escape(baseline.hash_value.value) << "\",";
    oss << "\"size\":" << baseline.size << ",";
    oss << "\"mode\":" << baseline.mode << ",";
    oss << "\"uid\":" << baseline.uid << ",";
    oss << "\"gid\":" << baseline.gid << ",";
    oss << "\"mtime_ns\":" << baseline.mtime_ns << ",";
    oss << "\"source\":\"" << escape(baseline.source) << "\",";

    if (baseline.deployment) {
        oss << "\"deployment\":\"" << escape(*baseline.deployment) << "\"";
    } else {
        oss << "\"deployment\":null";
    }

    oss << "}";
    return oss.str();
}

std::string to_json(const std::vector<Baseline>& baselines) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < baselines.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << to_json(baselines[i]);
    }
    oss << "]";
    return oss.str();
}

std::string to_json(const JournalEventRecord& event) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":" << event.id << ",";
    oss << "\"rule_name\":\"" << escape(event.rule_name) << "\",";
    oss << "\"message\":\"" << escape(event.message) << "\",";
    oss << "\"priority\":" << event.priority << ",";
    oss << "\"unit_name\":\"" << escape(event.unit_name.value_or("")) << "\",";
    oss << "\"created_at\":\"" << escape(event.created_at) << "\"";
    oss << "}";
    return oss.str();
}

std::string to_json(const std::vector<JournalEventRecord>& events) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < events.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << to_json(events[i]);
    }
    oss << "]";
    return oss.str();
}

std::string to_json(const AuditEventRecord& event) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":" << event.id << ",";
    oss << "\"rule_name\":\"" << escape(event.rule_name) << "\",";
    oss << "\"event_type\":\"" << escape(event.event_type) << "\",";

    if (event.pid) {
        oss << "\"pid\":" << *event.pid << ",";
    } else {
        oss << "\"pid\":null,";
    }

    if (event.uid) {
        oss << "\"uid\":" << *event.uid << ",";
    } else {
        oss << "\"uid\":null,";
    }

    if (event.username) {
        oss << "\"username\":\"" << escape(*event.username) << "\",";
    } else {
        oss << "\"username\":null,";
    }

    if (event.exe_path) {
        oss << "\"exe_path\":\"" << escape(*event.exe_path) << "\",";
    } else {
        oss << "\"exe_path\":null,";
    }

    if (event.command_line) {
        oss << "\"command_line\":\"" << escape(*event.command_line) << "\",";
    } else {
        oss << "\"command_line\":null,";
    }

    if (event.details) {
        oss << "\"details\":\"" << escape(*event.details) << "\",";
    } else {
        oss << "\"details\":null,";
    }

    oss << "\"created_at\":\"" << escape(event.created_at) << "\"";
    oss << "}";
    return oss.str();
}

std::string to_json(const std::vector<AuditEventRecord>& events) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < events.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << to_json(events[i]);
    }
    oss << "]";
    return oss.str();
}

std::string paginated_response(const std::string& data_array, const std::string& key,
                               int total, int limit, int offset) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"" << escape(key) << "\":" << data_array << ",";
    oss << "\"total\":" << total << ",";
    oss << "\"limit\":" << limit << ",";
    oss << "\"offset\":" << offset;
    oss << "}";
    return oss.str();
}

std::string error_response(const std::string& code, const std::string& message) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"error\":{";
    oss << "\"code\":\"" << escape(code) << "\",";
    oss << "\"message\":\"" << escape(message) << "\"";
    oss << "}";
    oss << "}";
    return oss.str();
}

} // namespace vigilant_canine::api::json
