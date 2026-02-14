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
