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

# BLAKE3 - use system package (blake3-devel)

# Hinder - local build or system package
# Option 1: Use as subdirectory (requires symlink or copy in external/hinder)
# Option 2: Use find_package if installed
# For now, assume external/hinder exists (user can symlink their local Hinder)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/hinder/CMakeLists.txt)
    add_subdirectory(external/hinder)
    set(HINDER_FOUND TRUE)
else()
    message(STATUS "Hinder not found in external/hinder")
    message(STATUS "Please symlink your Hinder source:")
    message(STATUS "  ln -s /var/home/tony/Development/personal/hinder external/hinder")
    set(HINDER_FOUND FALSE)
endif()

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
)

if(HINDER_FOUND)
    target_link_libraries(vigilant-canined PRIVATE hinder::hinder)
endif()

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
install(TARGETS vigilant-canined
    RUNTIME DESTINATION bin
)

# systemd service files (TODO: add when daemon is complete)
# install(FILES systemd/vigilant-canined.service
#     DESTINATION lib/systemd/system
# )

# tmpfiles.d configuration
# install(FILES systemd/vigilant-canine.conf
#     DESTINATION lib/tmpfiles.d
# )
