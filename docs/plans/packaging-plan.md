# RPM and DEB Packaging Plan

## Context

The vigilant-canine daemon is feature-complete with systemd integration and manual installation via `cmake --install`. However, distribution packaging (RPM for Fedora/RHEL, DEB for Debian/Ubuntu) is needed to:

1. **Simplify installation** - `dnf install vigilant-canine` or `apt install vigilant-canine`
2. **Handle dependencies automatically** - Package manager resolves build/runtime deps
3. **Enable distribution inclusion** - Packages can be submitted to Fedora/Debian repos
4. **Provide clean uninstallation** - Proper cleanup of files and services
5. **Support updates** - Version upgrades with automatic config preservation

This plan creates packaging infrastructure for both RPM and DEB formats following distribution best practices.

## Technology Decisions

### RPM Packaging (Fedora/RHEL/CentOS)

**Tool: rpmbuild with .spec file**
- Standard for Fedora/RHEL ecosystem
- Integrates with Mock for clean build environments
- Supports Source RPMs (SRPM) for distribution submission

**Build approach:**
- Spec file in repository root: `vigilant-canine.spec`
- Out-of-tree build (CMake build in %build section)
- Release builds only (CMAKE_BUILD_TYPE=Release)

**Version source: CMakeLists.txt**
- Parse `project(vigilant-canine VERSION x.y.z)` for version
- Spec file uses `%{version}` macro
- Automatic tarball creation via `git archive`

### DEB Packaging (Debian/Ubuntu)

**Tool: dpkg-buildpackage with debian/ directory**
- Standard for Debian/Ubuntu ecosystem
- Integrates with pbuilder/sbuild for clean builds
- Supports source packages for PPA/repository submission

**Build approach:**
- Debian directory in repository: `debian/`
- Uses dh (debhelper) for automation
- CMake integration via dh-cmake

**Version source: debian/changelog**
- Separate changelog for Debian packaging
- Follows Debian versioning: `x.y.z-1` (upstream-debian)

## Architecture

### Directory Structure

```
vigilant-canine/
├── CMakeLists.txt                  # Already has install targets
├── vigilant-canine.spec            # RPM spec file (new)
├── debian/                         # DEB packaging directory (new)
│   ├── changelog                   # Version history
│   ├── control                     # Dependencies, description
│   ├── rules                       # Build instructions (Makefile)
│   ├── compat                      # Debhelper compatibility level
│   ├── copyright                   # License information
│   ├── vigilant-canined.service    # Symlink to systemd/vigilant-canined.service
│   ├── vigilant-canined-api.service # Symlink to systemd/vigilant-canined-api.service
│   ├── vigilant-canine.tmpfiles    # Symlink to systemd/vigilant-canine.conf
│   ├── postinst                    # Post-installation script
│   ├── prerm                       # Pre-removal script
│   └── postrm                      # Post-removal script
├── packaging/                      # Build scripts (new)
│   ├── build-rpm.sh                # RPM build automation
│   ├── build-deb.sh                # DEB build automation
│   └── README.md                   # Packaging documentation
└── .gitignore                      # Ignore build artifacts
```

### Package Naming

**RPM (Fedora/RHEL):**
- Package name: `vigilant-canine`
- Binary package: `vigilant-canine-0.1.0-1.fc40.x86_64.rpm`
- Source package: `vigilant-canine-0.1.0-1.fc40.src.rpm`

**DEB (Debian/Ubuntu):**
- Package name: `vigilant-canine`
- Binary package: `vigilant-canine_0.1.0-1_amd64.deb`
- Source package: `vigilant-canine_0.1.0-1.dsc`

### Dependencies

**Build Dependencies (BuildRequires/Build-Depends):**
- C++23 compiler (gcc-c++ >= 15 or clang >= 18)
- CMake >= 3.25
- pkg-config
- SQLite3 development headers
- systemd development headers
- OpenSSL development headers (libcrypto)
- BLAKE3 development headers
- libaudit development headers
- libauparse development headers

**Runtime Dependencies (Requires/Depends):**
- SQLite3 library
- systemd (for journal, D-Bus)
- OpenSSL (libcrypto)
- BLAKE3 library
- libaudit
- libauparse

## Implementation Steps

### Step 1: Create RPM Spec File (2 days)

**File:** `vigilant-canine.spec`

```spec
Name:           vigilant-canine
Version:        0.1.0
Release:        1%{?dist}
Summary:        Simple host-level Intrusion Detection System

License:        GPL-3.0-or-later
URL:            https://github.com/tony/vigilant-canine
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ >= 15
BuildRequires:  cmake >= 3.25
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(libblake3)
BuildRequires:  pkgconfig(audit)
BuildRequires:  pkgconfig(auparse)

Requires:       sqlite
Requires:       systemd
Requires:       openssl-libs
Requires:       blake3
Requires:       audit-libs

%description
Vigilant Canine is a simple, host-level Intrusion Detection System (IDS)
designed for average users. It monitors file integrity, system logs, and
the Linux audit subsystem to detect suspicious activity, alerting users
through desktop notifications and an optional REST API.

%package api
Summary:        REST API daemon for Vigilant Canine
Requires:       %{name} = %{version}-%{release}

%description api
Optional REST API daemon that exposes Vigilant Canine alerts and baselines
over a Unix domain socket for integration with web dashboards and external
tools.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DVC_BUILD_TESTS=OFF
%cmake_build

%install
%cmake_install

# Create runtime directories (tmpfiles.d will populate at boot)
mkdir -p %{buildroot}%{_localstatedir}/lib/%{name}
mkdir -p %{buildroot}%{_sysconfdir}/%{name}

%check
# Tests disabled in package build (requires root for fanotify/audit)
# Run in mock or dedicated test environment

%post
%systemd_post vigilant-canined.service

%preun
%systemd_preun vigilant-canined.service

%postun
%systemd_postun_with_restart vigilant-canined.service

%post api
%systemd_post vigilant-canined-api.service

%preun api
%systemd_preun vigilant-canined-api.service

%postun api
%systemd_postun_with_restart vigilant-canined-api.service

%files
%license LICENSE
%doc README.md docs/*.md
%{_bindir}/vigilant-canined
%{_unitdir}/vigilant-canined.service
%{_tmpfilesdir}/vigilant-canine.conf
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/config.toml.example
%dir %{_localstatedir}/lib/%{name}

%files api
%{_bindir}/vigilant-canined-api
%{_unitdir}/vigilant-canined-api.service

%changelog
* Thu Feb 13 2026 Tony Narlock <tony@git-pull.com> - 0.1.0-1
- Initial package release
- File integrity monitoring via fanotify
- Log analysis via systemd journal
- Audit subsystem integration
- REST API daemon
- systemd integration with security hardening
```

**Key features:**
- Subpackage for API daemon (optional component)
- Uses macros for standard paths (`%{_bindir}`, `%{_unitdir}`, etc.)
- `config(noreplace)` preserves user configs during upgrades
- systemd scriptlets handle service lifecycle
- Tests disabled in package build (require root privileges)

**Tests:**
- Build SRPM: `rpmbuild -bs vigilant-canine.spec`
- Build RPM in mock: `mock -r fedora-40-x86_64 vigilant-canine*.src.rpm`
- Lint spec: `rpmlint vigilant-canine.spec`

**Acceptance:** Spec file passes rpmlint, builds in mock, RPMs install cleanly

### Step 2: Create Debian Packaging Files (3 days)

**Files to create:**

**1. debian/changelog**
```
vigilant-canine (0.1.0-1) unstable; urgency=medium

  * Initial release (Closes: #XXXXXX)
  * File integrity monitoring via fanotify
  * Log analysis via systemd journal
  * Audit subsystem integration
  * REST API daemon
  * systemd integration with security hardening

 -- Tony Narlock <tony@git-pull.com>  Thu, 13 Feb 2026 12:00:00 -0500
```

**2. debian/control**
```
Source: vigilant-canine
Section: admin
Priority: optional
Maintainer: Tony Narlock <tony@git-pull.com>
Build-Depends: debhelper-compat (= 13),
               cmake (>= 3.25),
               g++ (>= 15) | clang (>= 18),
               pkg-config,
               libsqlite3-dev,
               libsystemd-dev,
               libssl-dev,
               libblake3-dev,
               libaudit-dev
Standards-Version: 4.6.2
Homepage: https://github.com/tony/vigilant-canine
Vcs-Browser: https://github.com/tony/vigilant-canine
Vcs-Git: https://github.com/tony/vigilant-canine.git
Rules-Requires-Root: no

Package: vigilant-canine
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends},
         systemd,
         libsqlite3-0,
         libssl3,
         libblake3-0,
         libaudit1
Description: Simple host-level Intrusion Detection System
 Vigilant Canine monitors file integrity, system logs, and the Linux audit
 subsystem to detect suspicious activity on a single host.
 .
 Features:
  - File integrity monitoring via fanotify
  - Log analysis via systemd journal with pattern matching
  - Audit subsystem integration for process/privilege tracking
  - Desktop notifications via D-Bus
  - Privilege separation architecture
 .
 Designed for home users and small offices with simplicity as the primary goal.

Package: vigilant-canine-api
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends},
         vigilant-canine (= ${binary:Version})
Description: REST API daemon for Vigilant Canine
 Optional REST API daemon that exposes Vigilant Canine alerts, baselines,
 and events over a Unix domain socket for integration with web dashboards
 and external tools.
```

**3. debian/rules**
```makefile
#!/usr/bin/make -f

export DEB_BUILD_MAINT_OPTIONS = hardening=+all
export DEB_CFLAGS_MAINT_APPEND  = -Wall -pedantic
export DEB_CXXFLAGS_MAINT_APPEND  = -Wall -pedantic
export DEB_LDFLAGS_MAINT_APPEND = -Wl,--as-needed

%:
	dh $@ --buildsystem=cmake

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=Release \
		-DVC_BUILD_TESTS=OFF

override_dh_auto_test:
	# Tests require root privileges (fanotify, audit)
	# Skip in package build

override_dh_install:
	dh_install
	# Install config example
	install -D -m 644 config/vigilant-canine.toml.example \
		debian/vigilant-canine/etc/vigilant-canine/config.toml.example
```

**4. debian/compat**
```
13
```

**5. debian/copyright**
```
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: vigilant-canine
Upstream-Contact: Tony Narlock <tony@git-pull.com>
Source: https://github.com/tony/vigilant-canine

Files: *
Copyright: 2026 Tony Narlock
License: GPL-3.0-or-later

Files: debian/*
Copyright: 2026 Tony Narlock
License: GPL-3.0-or-later

License: GPL-3.0-or-later
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
 .
 On Debian systems, the complete text of the GNU General Public License
 version 3 can be found in "/usr/share/common-licenses/GPL-3".
```

**6. debian/vigilant-canine.install**
```
usr/bin/vigilant-canined
usr/lib/systemd/system/vigilant-canined.service
usr/lib/tmpfiles.d/vigilant-canine.conf
```

**7. debian/vigilant-canine-api.install**
```
usr/bin/vigilant-canined-api
usr/lib/systemd/system/vigilant-canined-api.service
```

**8. debian/vigilant-canine.postinst**
```bash
#!/bin/sh
set -e

#DEBHELPER#

# Create runtime directories
if [ "$1" = "configure" ]; then
    systemd-tmpfiles --create /usr/lib/tmpfiles.d/vigilant-canine.conf || true
fi

exit 0
```

**9. debian/vigilant-canine.prerm**
```bash
#!/bin/sh
set -e

#DEBHELPER#

exit 0
```

**10. debian/vigilant-canine.postrm**
```bash
#!/bin/sh
set -e

#DEBHELPER#

# Remove runtime directories on purge
if [ "$1" = "purge" ]; then
    rm -rf /var/lib/vigilant-canine
    rm -rf /etc/vigilant-canine
fi

exit 0
```

**Tests:**
- Build package: `dpkg-buildpackage -us -uc`
- Lint package: `lintian ../vigilant-canine_*.deb`
- Test install: `sudo dpkg -i ../vigilant-canine_*.deb`
- Test removal: `sudo apt remove vigilant-canine`

**Acceptance:** Package builds, passes lintian, installs cleanly, services start

### Step 3: Create Build Automation Scripts (1 day)

**File: packaging/build-rpm.sh**
```bash
#!/bin/bash
# Build RPM packages for Vigilant Canine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Parse version from CMakeLists.txt
VERSION=$(grep 'project(vigilant-canine VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
echo "Building version: $VERSION"

# Create build directories
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create source tarball
git archive --format=tar.gz --prefix="vigilant-canine-$VERSION/" \
    -o ~/rpmbuild/SOURCES/vigilant-canine-$VERSION.tar.gz HEAD

# Copy spec file
cp vigilant-canine.spec ~/rpmbuild/SPECS/

# Build SRPM
rpmbuild -bs ~/rpmbuild/SPECS/vigilant-canine.spec

# Build RPM (or use mock for clean environment)
if command -v mock &> /dev/null; then
    echo "Building with mock for clean environment..."
    mock -r fedora-$(rpm -E %fedora)-$(uname -m) \
        ~/rpmbuild/SRPMS/vigilant-canine-$VERSION-*.src.rpm
else
    echo "Building with rpmbuild (install 'mock' for cleaner builds)..."
    rpmbuild -bb ~/rpmbuild/SPECS/vigilant-canine.spec
fi

echo "Build complete!"
echo "SRPM: ~/rpmbuild/SRPMS/vigilant-canine-$VERSION-*.src.rpm"
if command -v mock &> /dev/null; then
    echo "RPM: /var/lib/mock/fedora-$(rpm -E %fedora)-$(uname -m)/result/*.rpm"
else
    echo "RPM: ~/rpmbuild/RPMS/*/vigilant-canine-*.rpm"
fi
```

**File: packaging/build-deb.sh**
```bash
#!/bin/bash
# Build DEB packages for Vigilant Canine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Check for build dependencies
if ! command -v dpkg-buildpackage &> /dev/null; then
    echo "Error: dpkg-buildpackage not found. Install: sudo apt install dpkg-dev"
    exit 1
fi

# Parse version from debian/changelog
VERSION=$(dpkg-parsechangelog -S Version)
echo "Building version: $VERSION"

# Clean previous builds
debian/rules clean || true

# Build package
dpkg-buildpackage -us -uc -b

# Lint package
if command -v lintian &> /dev/null; then
    echo "Running lintian..."
    lintian ../vigilant-canine_*.deb || true
fi

echo "Build complete!"
echo "Binary packages:"
ls -lh ../vigilant-canine*.deb
```

**File: packaging/README.md**
```markdown
# Packaging Documentation

## Building RPM Packages

### Prerequisites (Fedora/RHEL)
```bash
sudo dnf install rpm-build rpmdevtools mock
rpmdev-setuptree
```

### Build
```bash
./packaging/build-rpm.sh
```

Packages will be in:
- SRPM: `~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm`
- RPM: `/var/lib/mock/*/result/` (if using mock) or `~/rpmbuild/RPMS/`

### Install
```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm
```

## Building DEB Packages

### Prerequisites (Debian/Ubuntu)
```bash
sudo apt install build-essential debhelper devscripts lintian
```

### Build
```bash
./packaging/build-deb.sh
```

Packages will be in parent directory:
- `../vigilant-canine_*.deb`
- `../vigilant-canine-api_*.deb`

### Install
```bash
sudo apt install ../vigilant-canine_*.deb
```

## Version Updates

### RPM
Edit `vigilant-canine.spec`:
1. Update `Version:` field
2. Add changelog entry with `%changelog`

### DEB
Update changelog:
```bash
dch -v 0.2.0-1 "New release"
```

## Clean Builds

### RPM with Mock
Mock provides isolated build environment:
```bash
mock -r fedora-40-x86_64 ~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm
```

### DEB with pbuilder
```bash
sudo pbuilder --build ../vigilant-canine_*.dsc
```

## Submitting to Repositories

### Fedora (COPR)
1. Create COPR project: https://copr.fedorainfracloud.org/
2. Upload SRPM
3. Build for Fedora releases

### Ubuntu (PPA)
1. Create Launchpad account
2. Set up PPA
3. Upload source package:
```bash
debuild -S
dput ppa:yourusername/vigilant-canine ../vigilant-canine_*.changes
```

## Testing

### RPM Installation Test
```bash
# Install
sudo dnf install vigilant-canine-*.rpm

# Verify files
rpm -ql vigilant-canine

# Verify service
sudo systemctl status vigilant-canined

# Remove
sudo dnf remove vigilant-canine
```

### DEB Installation Test
```bash
# Install
sudo apt install ./vigilant-canine_*.deb

# Verify files
dpkg -L vigilant-canine

# Verify service
sudo systemctl status vigilant-canined

# Remove
sudo apt remove vigilant-canine
```
```

**Tests:**
- Execute build scripts on Fedora and Ubuntu
- Verify packages build without errors
- Test installation from built packages

**Acceptance:** Scripts work on both distros, produce installable packages

### Step 4: Update .gitignore (15 minutes)

Add packaging artifacts to `.gitignore`:

```gitignore
# Packaging artifacts
*.rpm
*.deb
*.dsc
*.changes
*.buildinfo
*.tar.xz
*.orig.tar.gz
/debian/.debhelper/
/debian/vigilant-canine/
/debian/vigilant-canine-api/
/debian/files
/debian/*.log
/debian/*.substvars
/obj-*/

# RPM build
/rpmbuild/
```

**Tests:**
- Build packages, verify artifacts not tracked by git
- `git status` should be clean after builds

**Acceptance:** Packaging artifacts excluded from version control

### Step 5: Update CMakeLists.txt for Packaging (1 day)

**Modifications needed:**

1. **Add version parsing from spec/changelog**
2. **CPack configuration for tarball generation**
3. **Optional: CPack RPM/DEB generators (alternative to spec/debian)**

**Add to CMakeLists.txt (after project() command):**

```cmake
# Packaging support
set(CPACK_PACKAGE_NAME "vigilant-canine")
set(CPACK_PACKAGE_VENDOR "Tony Narlock")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple host-level Intrusion Detection System")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Source tarball generation (for spec/debian)
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/"
    "/build/"
    "/\\.vscode/"
    "/\\.idea/"
    "\\.swp$"
    "\\.swo$"
    "~$"
)

include(CPack)
```

This enables: `make package_source` to create tarballs for packaging.

**Tests:**
- `cmake -B build && cd build && make package_source`
- Verify tarball created and excludes .git

**Acceptance:** Source tarball generation works

### Step 6: Documentation Updates (1 day)

**Files to modify:**

1. **README.md** - Add installation section for packages
2. **docs/installation.md** - Create comprehensive installation guide
3. **CONTRIBUTING.md** - Add packaging section

**README.md addition:**

```markdown
## Installation

### From Package Repository (Recommended)

#### Fedora/RHEL
```bash
# Add COPR repository (once available)
sudo dnf copr enable yourusername/vigilant-canine

# Install
sudo dnf install vigilant-canine
```

#### Ubuntu/Debian
```bash
# Add PPA (once available)
sudo add-apt-repository ppa:yourusername/vigilant-canine
sudo apt update

# Install
sudo apt install vigilant-canine
```

### From Binary Package

Download the appropriate package for your distribution from the releases page.

#### Fedora/RHEL
```bash
sudo dnf install vigilant-canine-*.rpm
```

#### Ubuntu/Debian
```bash
sudo apt install ./vigilant-canine_*.deb
```

### From Source

See [Building from Source](#building) for manual compilation and installation.
```

**Create docs/installation.md** with complete installation instructions for all methods.

**Tests:**
- Verify documentation accuracy
- Test example commands

**Acceptance:** Documentation complete, examples work

## Critical Files Summary

**New files:**
1. `vigilant-canine.spec` - RPM spec file
2. `debian/changelog` - Debian changelog
3. `debian/control` - Debian package metadata
4. `debian/rules` - Build rules
5. `debian/compat` - Debhelper version
6. `debian/copyright` - License information
7. `debian/vigilant-canine.install` - Main package files
8. `debian/vigilant-canine-api.install` - API package files
9. `debian/vigilant-canine.postinst` - Post-install script
10. `debian/vigilant-canine.prerm` - Pre-removal script
11. `debian/vigilant-canine.postrm` - Post-removal script
12. `packaging/build-rpm.sh` - RPM build script
13. `packaging/build-deb.sh` - DEB build script
14. `packaging/README.md` - Packaging documentation
15. `docs/installation.md` - Installation guide

**Modified files:**
1. `.gitignore` - Exclude packaging artifacts
2. `CMakeLists.txt` - Add CPack configuration
3. `README.md` - Add installation section
4. `CONTRIBUTING.md` - Add packaging guidelines

## Verification Steps

### RPM Verification (Fedora 40)

```bash
# 1. Build package
cd packaging
./build-rpm.sh

# 2. Verify package contents
rpm -qpl ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm

# Should include:
# /usr/bin/vigilant-canined
# /usr/lib/systemd/system/vigilant-canined.service
# /usr/lib/tmpfiles.d/vigilant-canine.conf
# /etc/vigilant-canine/config.toml.example

# 3. Check dependencies
rpm -qp --requires ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm

# 4. Lint spec file
rpmlint vigilant-canine.spec
rpmlint ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm

# 5. Test installation
sudo dnf install ~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm

# 6. Verify service
sudo systemctl status vigilant-canined
sudo systemctl start vigilant-canined
sudo journalctl -u vigilant-canined -n 20

# 7. Verify files installed
rpm -ql vigilant-canine

# 8. Test removal
sudo dnf remove vigilant-canine

# 9. Verify cleanup
ls /var/lib/vigilant-canine  # Should not exist
ls /etc/vigilant-canine       # Should not exist
```

### DEB Verification (Ubuntu 24.04)

```bash
# 1. Build package
cd packaging
./build-deb.sh

# 2. Verify package contents
dpkg-deb -c ../vigilant-canine_*.deb

# Should include:
# ./usr/bin/vigilant-canined
# ./usr/lib/systemd/system/vigilant-canined.service
# ./usr/lib/tmpfiles.d/vigilant-canine.conf
# ./etc/vigilant-canine/config.toml.example

# 3. Check dependencies
dpkg-deb -I ../vigilant-canine_*.deb | grep Depends

# 4. Lint package
lintian ../vigilant-canine_*.deb

# 5. Test installation
sudo apt install ../vigilant-canine_*.deb

# 6. Verify service
sudo systemctl status vigilant-canined
sudo systemctl start vigilant-canined
sudo journalctl -u vigilant-canined -n 20

# 7. Verify files installed
dpkg -L vigilant-canine

# 8. Test upgrade (modify version, rebuild, install again)
# Config should be preserved

# 9. Test removal
sudo apt remove vigilant-canine

# 10. Test purge
sudo apt purge vigilant-canine
ls /var/lib/vigilant-canine  # Should not exist
ls /etc/vigilant-canine       # Should not exist
```

### Cross-Distribution Testing

Test on multiple distributions using containers:

```bash
# Fedora 40
podman run -it --rm -v $(pwd):/work:z fedora:40 bash
cd /work && ./packaging/build-rpm.sh

# Ubuntu 24.04
podman run -it --rm -v $(pwd):/work:z ubuntu:24.04 bash
cd /work && ./packaging/build-deb.sh
```

## Success Criteria

- [ ] RPM spec file complete and passes rpmlint
- [ ] Debian packaging files complete and pass lintian
- [ ] Build scripts work on Fedora and Ubuntu
- [ ] Packages install cleanly without errors
- [ ] All files installed to correct locations
- [ ] Services start after installation
- [ ] Config files preserved during upgrades
- [ ] Clean removal/purge works correctly
- [ ] Dependencies automatically resolved
- [ ] Documentation complete with working examples
- [ ] .gitignore excludes packaging artifacts
- [ ] Source tarballs generated correctly

## Timeline Estimate

- **Step 1:** RPM spec file - 2 days
- **Step 2:** Debian packaging - 3 days
- **Step 3:** Build scripts - 1 day
- **Step 4:** .gitignore - 15 minutes
- **Step 5:** CMakeLists.txt - 1 day
- **Step 6:** Documentation - 1 day

**Total: ~8 days** (2 weeks with testing and refinement)

## Future Work (Not in Scope)

- Automated CI/CD for package builds (GitHub Actions)
- Submission to official Fedora/Debian repositories
- Multi-architecture builds (ARM64, i686)
- Flatpak/Snap packaging
- Windows installer (if cross-platform support added)
- Automatic version bumping from git tags
- Package signing and GPG keys
