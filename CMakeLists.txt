cmake_minimum_required(VERSION 3.25)
project(vigilant-canine VERSION 0.1.0 LANGUAGES C CXX)

# C++23 required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Options
option(VC_BUILD_TESTS "Build tests" ON)
option(VC_BUILD_BENCHMARKS "Build benchmarks" ON)

# System dependencies
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(OPENSSL REQUIRED libcrypto)
pkg_check_modules(BLAKE3 REQUIRED libblake3)
pkg_check_modules(AUDIT REQUIRED audit auparse)

# toml++ - header-only TOML parser
include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# cpp-httplib - header-only HTTP library with Unix socket support
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
    FIND_PACKAGE_ARGS NAMES httplib
)
# Set httplib options before making available
set(HTTPLIB_REQUIRE_OPENSSL OFF)
set(HTTPLIB_REQUIRE_ZLIB OFF)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
FetchContent_MakeAvailable(cpp-httplib)

# BLAKE3 - use system package (blake3-devel)

# Hinder - use system package
find_package(hinder REQUIRED)

# Main daemon executable
add_executable(vigilant-canined
    src/daemon/main.cpp
    src/daemon/daemon.cpp
    src/core/hash.cpp
    src/config/config.cpp
    src/storage/database.cpp
    src/storage/baseline_store.cpp
    src/storage/alert_store.cpp
    src/distro/detector.cpp
    src/baseline/strategy.cpp
    src/scanner/scanner.cpp
    src/events/event.cpp
    src/events/event_bus.cpp
    src/monitor/fanotify_monitor.cpp
    src/dispatch/alert_dispatcher.cpp
    src/policy/policy_engine.cpp
    # Phase 2 components
    src/journal/journal_rule.cpp
    src/journal/journal_monitor.cpp
    src/correlation/correlation_engine.cpp
    src/notify/dbus_notifier.cpp
    # Phase 3 components
    src/audit/audit_rule.cpp
    src/audit/audit_parsing.cpp
    src/audit/audit_monitor.cpp
    src/storage/audit_event_store.cpp
)

target_link_libraries(vigilant-canined PRIVATE
    SQLite::SQLite3
    ${SYSTEMD_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    ${AUDIT_LIBRARIES}
    tomlplusplus::tomlplusplus
    hinder::hinder
)

target_include_directories(vigilant-canined PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SYSTEMD_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
    ${AUDIT_INCLUDE_DIRS}
)

target_compile_options(vigilant-canined PRIVATE
    -Wall -Wextra -Wpedantic
    ${SYSTEMD_CFLAGS_OTHER}
    ${OPENSSL_CFLAGS_OTHER}
)

# API daemon executable
add_executable(vigilant-canined-api
    src/api/main.cpp
    src/api/http_server.cpp
    src/api/handlers/health_handler.cpp
    src/api/handlers/alert_handler.cpp
    src/api/handlers/baseline_handler.cpp
    src/api/handlers/event_handler.cpp
    src/api/serialization/json.cpp
    # Reuse storage layer
    src/storage/database.cpp
    src/storage/baseline_store.cpp
    src/storage/alert_store.cpp
    src/storage/audit_event_store.cpp
    src/config/config.cpp
    src/core/hash.cpp
    src/events/event.cpp
)

target_link_libraries(vigilant-canined-api PRIVATE
    SQLite::SQLite3
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
    tomlplusplus::tomlplusplus
    httplib::httplib
    hinder::hinder
)

target_include_directories(vigilant-canined-api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

target_compile_options(vigilant-canined-api PRIVATE
    -Wall -Wextra -Wpedantic
    ${OPENSSL_CFLAGS_OTHER}
)

# Testing
if(VC_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    # Prevent GoogleTest from overriding our compiler/linker options
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

# Benchmarks
if(VC_BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_subdirectory(benchmarks)
endif()

# Installation
include(GNUInstallDirs)

# Binaries
install(TARGETS vigilant-canined vigilant-canined-api
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Configuration example
install(FILES config/vigilant-canine.toml.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/vigilant-canine
    RENAME config.toml.example
)

# systemd service files
# Use pkg-config to detect systemd paths (works on Fedora/Debian/Ubuntu)
pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
pkg_get_variable(TMPFILES_DIR systemd tmpfilesdir)

# Fallback for packaging (DESTDIR set by rpmbuild/dpkg-buildpackage)
if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
    set(SYSTEMD_SYSTEM_UNIT_DIR /usr/lib/systemd/system)
endif()
if(NOT TMPFILES_DIR)
    set(TMPFILES_DIR /usr/lib/tmpfiles.d)
endif()

install(FILES
    systemd/vigilant-canined.service
    systemd/vigilant-canined-api.service
    DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}
)

install(FILES systemd/vigilant-canine.conf
    DESTINATION ${TMPFILES_DIR}
)

# Documentation
install(FILES
    README.md
    docs/architecture.md
    docs/configuration.md
    docs/troubleshooting.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
