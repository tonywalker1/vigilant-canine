//
// vigilant-canine - Power State Monitor Implementation
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <power/power_monitor.h>

#include <algorithm>
#include <fstream>
#include <string>

namespace vigilant_canine {

    namespace {

        //
        // Read single-line file from sysfs.
        //
        auto read_sysfs_file(std::filesystem::path const& path) -> std::string {
            std::ifstream file(path);
            if (!file.is_open()) {
                return {};
            }

            std::string line;
            std::getline(file, line);

            // Trim trailing whitespace/newline
            if (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }

            return line;
        }

    }  // anonymous namespace

    auto PowerMonitor::read_power_state() const -> PowerState {
        PowerState state{PowerSource::ac, 100, false};

        // Iterate power supply directories
        std::error_code ec;
        auto dir_iter = std::filesystem::directory_iterator(m_sysfs_path, ec);
        if (ec) {
            // Can't read sysfs - assume AC power
            return state;
        }

        for (auto const& entry : dir_iter) {
            if (!entry.is_directory(ec)) {
                continue;
            }

            // Check type file
            auto type_path = entry.path() / "type";
            auto type = read_sysfs_file(type_path);

            if (type != "Battery") {
                continue;
            }

            // Found a battery
            state.battery_present = true;

            // Read status
            auto status_path = entry.path() / "status";
            auto status = read_sysfs_file(status_path);

            // Read capacity
            auto capacity_path = entry.path() / "capacity";
            auto capacity_str = read_sysfs_file(capacity_path);

            if (!capacity_str.empty()) {
                try {
                    int capacity = std::stoi(capacity_str);
                    state.battery_percent = static_cast<std::uint8_t>(
                        std::clamp(capacity, 0, 100));
                } catch (...) {
                    // Invalid capacity - use default
                }
            }

            // Determine power source based on status
            if (status == "Discharging") {
                state.source = PowerSource::battery;
            } else {
                // Charging, Full, Not charging - all mean AC power
                state.source = PowerSource::ac;
            }

            // Use first battery found
            break;
        }

        return state;
    }

}  // namespace vigilant_canine
