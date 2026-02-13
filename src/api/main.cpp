// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "http_server.h"
#include "handlers/event_handler.h"

#include <storage/database.h>
#include <storage/alert_store.h>
#include <storage/baseline_store.h>
#include <storage/audit_event_store.h>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

volatile std::sig_atomic_t signal_received = 0;
vigilant_canine::api::HttpServer* g_server = nullptr;

void signal_handler(int signal) {
    signal_received = signal;
    if (g_server) {
        g_server->stop();
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\n"
              << "HTTP API daemon for Vigilant Canine.\n"
              << "\n"
              << "Options:\n"
              << "  -s, --socket PATH    Path to Unix domain socket\n"
              << "                       (default: /run/vigilant-canine/api.sock)\n"
              << "  -d, --database PATH  Path to database file\n"
              << "                       (default: /var/lib/vigilant-canine/vigilant-canine.db)\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --version        Show version information\n"
              << "\n";
}

void print_version() {
    std::cout << "vigilant-canined-api 0.1.0\n"
              << "Copyright (C) 2026 Tony Narlock\n"
              << "License: GPL-3.0-or-later\n";
}

} // anonymous namespace

auto main(int argc, char* argv[]) -> int {
    std::filesystem::path socket_path = "/run/vigilant-canine/api.sock";
    std::filesystem::path db_path = "/var/lib/vigilant-canine/vigilant-canine.db";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (arg == "-v" || arg == "--version") {
            print_version();
            return EXIT_SUCCESS;
        }
        if (arg == "-s" || arg == "--socket") {
            if (i + 1 < argc) {
                socket_path = argv[++i];
            } else {
                std::cerr << "Error: --socket requires a path argument\n";
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (arg == "-d" || arg == "--database") {
            if (i + 1 < argc) {
                db_path = argv[++i];
            } else {
                std::cerr << "Error: --database requires a path argument\n";
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
        // Open database in read-write mode (for acknowledge operations)
        std::cout << "Opening database: " << db_path << "\n";
        auto db_result = vigilant_canine::Database::open(db_path);
        if (!db_result) {
            std::cerr << "Failed to open database: " << db_result.error() << "\n";
            return EXIT_FAILURE;
        }

        auto db = std::move(*db_result);

        // Create stores
        vigilant_canine::AlertStore alert_store(db);
        vigilant_canine::BaselineStore baseline_store(db);
        vigilant_canine::AuditEventStore audit_event_store(db);

        // Create event handler
        vigilant_canine::api::EventHandler event_handler(db, audit_event_store);

        // Create HTTP server
        vigilant_canine::api::HttpServer server(socket_path, alert_store, baseline_store, event_handler);
        g_server = &server;

        // Set up signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Start server (blocking)
        std::cout << "Starting API daemon...\n";
        if (!server.start()) {
            std::cerr << "Failed to start HTTP server\n";
            return EXIT_FAILURE;
        }

        if (signal_received) {
            std::cout << "Received signal " << signal_received << ", shutting down\n";
        }

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
