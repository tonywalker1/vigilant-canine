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
