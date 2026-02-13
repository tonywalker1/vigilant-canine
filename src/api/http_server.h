// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tony Narlock

#pragma once

#include <storage/alert_store.h>
#include <storage/baseline_store.h>
#include <storage/audit_event_store.h>

#include <filesystem>
#include <memory>
#include <string>

// Forward declare httplib::Server to avoid including the header in this file
namespace httplib {
class Server;
}

namespace vigilant_canine::api {

// Forward declare handlers
class AlertHandler;
class BaselineHandler;
class EventHandler;

/// HTTP server that listens on a Unix domain socket
class HttpServer {
public:
    HttpServer(const std::filesystem::path& socket_path,
               AlertStore& alert_store,
               BaselineStore& baseline_store,
               EventHandler& event_handler);
    ~HttpServer();

    // Non-copyable, non-movable (contains httplib::Server with active socket)
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    /// Start the HTTP server (blocking call)
    /// Returns false if server failed to start
    bool start();

    /// Stop the HTTP server gracefully
    void stop();

private:
    std::filesystem::path socket_path_;
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<AlertHandler> alert_handler_;
    std::unique_ptr<BaselineHandler> baseline_handler_;
    EventHandler* event_handler_;  // Non-owning pointer

    void setup_routes();
};

} // namespace vigilant_canine::api
