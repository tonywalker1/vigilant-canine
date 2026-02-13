// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#include "http_server.h"
#include "handlers/health_handler.h"
#include "handlers/alert_handler.h"
#include "handlers/baseline_handler.h"
#include "handlers/event_handler.h"

#include <httplib.h>

#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace vigilant_canine::api {

HttpServer::HttpServer(const std::filesystem::path& socket_path,
                       AlertStore& alert_store,
                       BaselineStore& baseline_store,
                       EventHandler& event_handler)
    : socket_path_(socket_path),
      server_(std::make_unique<httplib::Server>()),
      alert_handler_(std::make_unique<AlertHandler>(alert_store)),
      baseline_handler_(std::make_unique<BaselineHandler>(baseline_store)),
      event_handler_(&event_handler) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setup_routes() {
    // Health endpoint
    server_->Get("/api/v1/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(HealthHandler::handle_health(), "application/json");
    });

    // Alert endpoints
    server_->Get("/api/v1/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        alert_handler_->handle_list(req, res);
    });

    server_->Get(R"(/api/v1/alerts/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        alert_handler_->handle_get(req, res);
    });

    server_->Post(R"(/api/v1/alerts/(\d+)/acknowledge)", [this](const httplib::Request& req, httplib::Response& res) {
        alert_handler_->handle_acknowledge(req, res);
    });

    server_->Delete(R"(/api/v1/alerts/(\d+)/acknowledge)", [this](const httplib::Request& req, httplib::Response& res) {
        alert_handler_->handle_unacknowledge(req, res);
    });

    // Baseline endpoints
    server_->Get("/api/v1/baselines", [this](const httplib::Request& req, httplib::Response& res) {
        baseline_handler_->handle_list(req, res);
    });

    // Event endpoints
    server_->Get("/api/v1/journal-events", [this](const httplib::Request& req, httplib::Response& res) {
        event_handler_->handle_journal_events(req, res);
    });

    server_->Get("/api/v1/audit-events", [this](const httplib::Request& req, httplib::Response& res) {
        event_handler_->handle_audit_events(req, res);
    });
}

bool HttpServer::start() {
    // Remove existing socket if it exists
    if (std::filesystem::exists(socket_path_)) {
        std::cerr << "Removing existing socket: " << socket_path_ << "\n";
        std::filesystem::remove(socket_path_);
    }

    // Ensure parent directory exists
    auto parent = socket_path_.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::cerr << "Creating socket directory: " << parent << "\n";
        std::filesystem::create_directories(parent);
    }

    std::cout << "Starting HTTP server on Unix socket: " << socket_path_ << "\n";

    // Configure for Unix domain sockets
    std::cout << "Setting address family to AF_UNIX\n";
    server_->set_address_family(AF_UNIX);

    // Listen on Unix domain socket
    // Note: port parameter is technically ignored for Unix sockets, but can't be 0
    std::cout << "Calling listen() on socket path: " << socket_path_.string() << "\n";
    bool success = server_->listen(socket_path_.string(), 80);

    if (success) {
        std::cout << "Server started successfully\n";
        // Set socket permissions after creation (0660)
        chmod(socket_path_.c_str(), 0660);
    } else {
        std::cerr << "Failed to start server (listen() returned false)\n";
        std::cerr << "Check if cpp-httplib supports Unix sockets in this version\n";
    }

    return success;
}

void HttpServer::stop() {
    if (server_) {
        std::cout << "Stopping HTTP server\n";
        server_->stop();
    }

    // Clean up socket file
    if (std::filesystem::exists(socket_path_)) {
        std::filesystem::remove(socket_path_);
    }
}

} // namespace vigilant_canine::api
