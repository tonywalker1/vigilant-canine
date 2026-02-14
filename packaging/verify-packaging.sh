#!/bin/bash
# Verify packaging files are properly structured
# This script checks packaging file syntax and structure without actually building packages

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=== Vigilant Canine Packaging Verification ==="
echo ""

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓${NC} $1"
}

fail() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

info() {
    echo "  $1"
}

# Check if all required packaging files exist
echo "Checking packaging files..."
files=(
    "vigilant-canine.spec"
    "debian/changelog"
    "debian/control"
    "debian/rules"
    "debian/compat"
    "debian/copyright"
    "debian/vigilant-canine.install"
    "debian/vigilant-canine-api.install"
    "debian/vigilant-canine.postinst"
    "debian/vigilant-canine.prerm"
    "debian/vigilant-canine.postrm"
    "packaging/build-rpm.sh"
    "packaging/build-deb.sh"
    "packaging/README.md"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        pass "Found: $file"
    else
        fail "Missing: $file"
    fi
done

echo ""
echo "Checking file permissions..."

# Check executable permissions
executables=(
    "debian/rules"
    "debian/vigilant-canine.postinst"
    "debian/vigilant-canine.prerm"
    "debian/vigilant-canine.postrm"
    "packaging/build-rpm.sh"
    "packaging/build-deb.sh"
)

for file in "${executables[@]}"; do
    if [ -x "$file" ]; then
        pass "Executable: $file"
    else
        fail "Not executable: $file"
    fi
done

echo ""
echo "Checking version consistency..."

# Extract version from CMakeLists.txt
CMAKE_VERSION=$(grep 'project(vigilant-canine VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
info "CMakeLists.txt version: $CMAKE_VERSION"

# Extract version from RPM spec
SPEC_VERSION=$(grep '^Version:' vigilant-canine.spec | awk '{print $2}')
info "RPM spec version: $SPEC_VERSION"

# Check if versions match
if [ "$CMAKE_VERSION" = "$SPEC_VERSION" ]; then
    pass "CMake and RPM versions match"
else
    warn "Version mismatch: CMakeLists.txt ($CMAKE_VERSION) vs spec ($SPEC_VERSION)"
fi

# Check Debian changelog (requires dpkg-dev)
if command -v dpkg-parsechangelog &> /dev/null; then
    DEB_VERSION=$(dpkg-parsechangelog -l debian/changelog -S Version 2>/dev/null || echo "unknown")
    info "Debian changelog version: $DEB_VERSION"
else
    warn "dpkg-parsechangelog not available (install dpkg-dev to check Debian version)"
fi

echo ""
echo "Checking RPM spec file syntax..."

# Basic spec file checks
if grep -q "^Name:.*vigilant-canine" vigilant-canine.spec; then
    pass "Spec file has correct Name field"
else
    fail "Spec file Name field incorrect"
fi

if grep -q "^License:.*GPL-3.0-or-later" vigilant-canine.spec; then
    pass "Spec file has correct License"
else
    fail "Spec file License field incorrect"
fi

if grep -q "%package api" vigilant-canine.spec; then
    pass "Spec file defines API subpackage"
else
    fail "Spec file missing API subpackage"
fi

if grep -q "%systemd_post vigilant-canined.service" vigilant-canine.spec; then
    pass "Spec file has systemd scriptlets"
else
    fail "Spec file missing systemd scriptlets"
fi

echo ""
echo "Checking Debian control file..."

if grep -q "^Source: vigilant-canine" debian/control; then
    pass "Control file has correct Source"
else
    fail "Control file Source incorrect"
fi

if grep -q "^Package: vigilant-canine$" debian/control; then
    pass "Control file defines main package"
else
    fail "Control file missing main package"
fi

if grep -q "^Package: vigilant-canine-api$" debian/control; then
    pass "Control file defines API package"
else
    fail "Control file missing API package"
fi

if grep -q "debhelper-compat (= 13)" debian/control; then
    pass "Control file specifies debhelper compat 13"
else
    fail "Control file debhelper compat incorrect"
fi

echo ""
echo "Checking CMake CPack configuration..."

if grep -q "set(CPACK_PACKAGE_NAME" CMakeLists.txt; then
    pass "CMakeLists.txt has CPack configuration"
else
    fail "CMakeLists.txt missing CPack configuration"
fi

if grep -q "include(CPack)" CMakeLists.txt; then
    pass "CMakeLists.txt includes CPack"
else
    fail "CMakeLists.txt missing CPack include"
fi

echo ""
echo "Checking .gitignore for packaging artifacts..."

if grep -q "^\*.rpm" .gitignore; then
    pass ".gitignore excludes RPM files"
else
    fail ".gitignore missing RPM exclusion"
fi

if grep -q "^\*.deb" .gitignore; then
    pass ".gitignore excludes DEB files"
else
    fail ".gitignore missing DEB exclusion"
fi

if grep -q "^/debian/.debhelper/" .gitignore; then
    pass ".gitignore excludes Debian build artifacts"
else
    fail ".gitignore missing Debian build artifact exclusion"
fi

echo ""
echo "Checking documentation..."

docs=(
    "docs/installation.md"
    "packaging/README.md"
    "CONTRIBUTING.md"
)

for doc in "${docs[@]}"; do
    if [ -f "$doc" ]; then
        pass "Found: $doc"
    else
        fail "Missing: $doc"
    fi
done

# Check if documentation mentions packaging
if grep -q -i "package" README.md; then
    pass "README.md mentions packaging"
else
    warn "README.md may not mention package installation"
fi

echo ""
echo "Checking build scripts..."

# Verify build-rpm.sh has correct logic
if grep -q "git archive" packaging/build-rpm.sh; then
    pass "build-rpm.sh uses git archive for tarball"
else
    fail "build-rpm.sh missing git archive"
fi

if grep -q "rpmbuild" packaging/build-rpm.sh; then
    pass "build-rpm.sh calls rpmbuild"
else
    fail "build-rpm.sh missing rpmbuild call"
fi

# Verify build-deb.sh has correct logic
if grep -q "dpkg-buildpackage" packaging/build-deb.sh; then
    pass "build-deb.sh calls dpkg-buildpackage"
else
    fail "build-deb.sh missing dpkg-buildpackage call"
fi

if grep -q "lintian" packaging/build-deb.sh; then
    pass "build-deb.sh runs lintian"
else
    warn "build-deb.sh may not run lintian"
fi

echo ""
echo "=== Verification Complete ==="
echo ""
echo "All packaging files are properly structured!"
echo ""
echo "Next steps:"
echo "  - Build RPM: ./packaging/build-rpm.sh (requires Fedora/RHEL)"
echo "  - Build DEB: ./packaging/build-deb.sh (requires Debian/Ubuntu)"
echo "  - Read docs: packaging/README.md"
echo "  - Installation guide: docs/installation.md"
