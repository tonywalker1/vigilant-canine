//
// vigilant-canined - File Integrity Monitoring Daemon
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#include <daemon/daemon.h>

#include <systemd/sd-journal.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
    void print_usage(char const* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS]\n"
                  << "\n"
                  << "File integrity monitoring daemon for Linux systems.\n"
                  << "\n"
                  << "Options:\n"
                  << "  -c, --config PATH    Path to configuration file\n"
                  << "                       (default: /etc/vigilant-canine/config.toml)\n"
                  << "  -h, --help           Show this help message\n"
                  << "  -v, --version        Show version information\n"
                  << "\n";
    }

    void print_version() {
        std::cout << "vigilant-canined 0.1.0\n"
                  << "Copyright (c) 2026 Tony Walker\n"
                  << "License: GPL-3.0-or-later\n";
    }
}

auto main(int argc, char* argv[]) -> int {
    std::filesystem::path config_path = "/etc/vigilant-canine/config.toml";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (arg == "-v" || arg == "--version") {
            print_version();
            return EXIT_SUCCESS;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                std::cerr << "Error: --config requires a path argument\n";
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    try {
        // Create and initialize daemon
        vigilant_canine::Daemon daemon{config_path};

        auto init_result = daemon.initialize();
        if (!init_result) {
            std::cerr << "Initialization failed: " << init_result.error() << "\n";
            sd_journal_print(LOG_ERR, "vigilant-canined: Initialization failed: %s",
                             init_result.error().c_str());
            return EXIT_FAILURE;
        }

        // Run daemon (blocks until stop signal)
        auto run_result = daemon.run();
        if (!run_result) {
            std::cerr << "Daemon error: " << run_result.error() << "\n";
            sd_journal_print(LOG_ERR, "vigilant-canined: Daemon error: %s",
                             run_result.error().c_str());
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;

    } catch (std::exception const& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        sd_journal_print(LOG_CRIT, "vigilant-canined: Fatal error: %s", e.what());
        return EXIT_FAILURE;
    }
}
