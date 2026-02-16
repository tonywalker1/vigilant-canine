//
// vigilant-canine - Power Monitor Tests
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <power/power_monitor.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace vigilant_canine {

    namespace {

        //
        // Test fixture with temporary sysfs directory.
        //
        class PowerMonitorTest : public ::testing::Test {
        protected:
            void SetUp() override {
                // Create temp directory for mock sysfs
                temp_dir = std::filesystem::temp_directory_path() / "vc_power_test";
                std::filesystem::create_directories(temp_dir);
            }

            void TearDown() override {
                // Clean up temp directory
                std::filesystem::remove_all(temp_dir);
            }

            //
            // Create mock battery with given status and capacity.
            //
            void create_battery(std::string const& name,
                                std::string const& status,
                                int capacity) {
                auto battery_dir = temp_dir / name;
                std::filesystem::create_directories(battery_dir);

                // Write type file
                std::ofstream type_file(battery_dir / "type");
                type_file << "Battery\n";
                type_file.close();

                // Write status file
                std::ofstream status_file(battery_dir / "status");
                status_file << status << "\n";
                status_file.close();

                // Write capacity file
                std::ofstream capacity_file(battery_dir / "capacity");
                capacity_file << capacity << "\n";
                capacity_file.close();
            }

            //
            // Create mock AC adapter.
            //
            void create_ac_adapter(std::string const& name) {
                auto ac_dir = temp_dir / name;
                std::filesystem::create_directories(ac_dir);

                // Write type file
                std::ofstream type_file(ac_dir / "type");
                type_file << "Mains\n";
                type_file.close();
            }

            std::filesystem::path temp_dir;
        };

    }  // anonymous namespace

    TEST_F(PowerMonitorTest, NoPowerSupply) {
        // Empty sysfs directory - should default to AC power
        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 100);
        EXPECT_FALSE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, ACAdapterOnly) {
        // AC adapter but no battery
        create_ac_adapter("AC0");

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 100);
        EXPECT_FALSE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryDischarging) {
        // Battery discharging at 65%
        create_battery("BAT0", "Discharging", 65);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::battery);
        EXPECT_EQ(state.battery_percent, 65);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryCharging) {
        // Battery charging - should report AC power
        create_battery("BAT0", "Charging", 45);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 45);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryFull) {
        // Battery full - should report AC power
        create_battery("BAT0", "Full", 100);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 100);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryNotCharging) {
        // Battery "Not charging" - should report AC power
        create_battery("BAT0", "Not charging", 80);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 80);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, MultiplePowerSupplies) {
        // AC adapter + battery - uses first battery found
        create_ac_adapter("AC0");
        create_battery("BAT0", "Charging", 55);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::ac);
        EXPECT_EQ(state.battery_percent, 55);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryLowPercentage) {
        // Battery at low charge
        create_battery("BAT0", "Discharging", 15);

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.source, PowerSource::battery);
        EXPECT_EQ(state.battery_percent, 15);
        EXPECT_TRUE(state.battery_present);
    }

    TEST_F(PowerMonitorTest, BatteryCapacityClamping) {
        // Invalid capacity values should be clamped
        auto battery_dir = temp_dir / "BAT0";
        std::filesystem::create_directories(battery_dir);

        std::ofstream type_file(battery_dir / "type");
        type_file << "Battery\n";
        type_file.close();

        std::ofstream status_file(battery_dir / "status");
        status_file << "Discharging\n";
        status_file.close();

        // Test overflow value
        std::ofstream capacity_file(battery_dir / "capacity");
        capacity_file << "150\n";  // Over 100%
        capacity_file.close();

        PowerMonitor monitor(temp_dir);
        auto state = monitor.read_power_state();

        EXPECT_EQ(state.battery_percent, 100);  // Clamped to max
    }

}  // namespace vigilant_canine
