//
// vigilant-canine - Distribution Type Detection
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Tony Walker
//

#ifndef VIGILANT_CANINE_DISTRO_DETECTOR_H
#define VIGILANT_CANINE_DISTRO_DETECTOR_H

#include <core/types.h>

#include <expected>
#include <string>

namespace vigilant_canine {

    //
    // Distro identification information.
    //
    struct DistroInfo {
        DistroType type;
        std::string name;           // Distribution name (e.g., "Fedora", "Ubuntu")
        std::string version;        // Version string (e.g., "39", "24.04")
        std::string variant;        // Variant (e.g., "Workstation", "Silverblue")
    };

    //
    // Detect the distribution type and gather basic information.
    //
    // This function determines whether the system is:
    // - traditional: Standard package manager-based distro (Fedora Workstation, Ubuntu, etc.)
    // - ostree: OSTree/rpm-ostree based (Fedora Silverblue/Kinoite, RHEL for Edge)
    // - btrfs_snapshot: Btrfs snapshot-based (openSUSE MicroOS/Aeon)
    //
    // Detection strategy:
    // 1. Check for ostree: /ostree directory + ostree command availability
    // 2. Check for btrfs snapshots: root on btrfs + snapper/transactional-update
    // 3. Otherwise: traditional
    //
    // Postconditions:
    //   - On success: returns DistroInfo with type and metadata
    //   - On failure: returns error message
    //
    [[nodiscard]] auto detect_distro() -> std::expected<DistroInfo, std::string>;

    //
    // Check if ostree is present and functional.
    //
    [[nodiscard]] auto is_ostree_system() -> bool;

    //
    // Check if this is a btrfs snapshot-based system.
    //
    [[nodiscard]] auto is_btrfs_snapshot_system() -> bool;

    //
    // Parse /etc/os-release to get distribution metadata.
    //
    // Postconditions:
    //   - On success: returns name, version, and variant from os-release
    //   - On failure: returns error message
    //
    [[nodiscard]] auto parse_os_release()
        -> std::expected<DistroInfo, std::string>;

    //
    // Convert DistroType to string.
    //
    [[nodiscard]] auto to_string(DistroType type) -> std::string;

}  // namespace vigilant_canine

#endif  // VIGILANT_CANINE_DISTRO_DETECTOR_H
