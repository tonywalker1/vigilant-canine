//
// vigilant-canine - Power State Monitor
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_POWER_POWER_MONITOR_H
#define VIGILANT_CANINE_POWER_POWER_MONITOR_H

#include <cstdint>
#include <filesystem>

namespace vigilant_canine {

    //
    // Power source type.
    //
    enum class PowerSource {
        ac,       // AC power (plugged in)
        battery   // Battery power
    };

    //
    // System power state.
    //
    struct PowerState {
        PowerSource source;
        std::uint8_t battery_percent{100};
        bool battery_present{false};
    };

    //
    // Monitor system power state via sysfs.
    //
    // Reads /sys/class/power_supply to determine:
    // - Power source (AC vs battery)
    // - Battery charge percentage
    // - Battery presence
    //
    class PowerMonitor {
    public:
        //
        // Construct PowerMonitor with custom sysfs path (for testing).
        //
        explicit PowerMonitor(std::filesystem::path sysfs_path = "/sys/class/power_supply")
            : m_sysfs_path(std::move(sysfs_path)) {}

        //
        // Read current power state from sysfs.
        //
        // Returns:
        // - {PowerSource::ac, 100, false} if no battery present
        // - {PowerSource::ac, percent, true} if charging/full/not charging
        // - {PowerSource::battery, percent, true} if discharging
        //
        [[nodiscard]] auto read_power_state() const -> PowerState;

    private:
        std::filesystem::path m_sysfs_path;
    };

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_POWER_POWER_MONITOR_H
