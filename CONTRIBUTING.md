# Contributing to Vigilant Canine

Thank you for your interest in contributing to Vigilant Canine! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Building and Testing](#building-and-testing)
- [Code Style](#code-style)
- [Submitting Changes](#submitting-changes)
- [Packaging](#packaging)
- [Documentation](#documentation)

## Code of Conduct

This project follows a simple code of conduct: be respectful, constructive, and helpful. We're all here to make security software better for average users.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/yourusername/vigilant-canine.git
   cd vigilant-canine
   ```
3. Add upstream remote:
   ```bash
   git remote add upstream https://github.com/tony/vigilant-canine.git
   ```
4. Create a topic branch for your work:
   ```bash
   git checkout -b feature/my-new-feature
   ```

## Development Environment

### System Dependencies

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ cmake pkg-config \
                 sqlite-devel systemd-devel \
                 openssl-devel blake3-devel \
                 audit-libs-devel hinder-devel \
                 clang-tools-extra  # For clang-format and clang-tidy
```

**Ubuntu/Debian:**
```bash
sudo apt install g++ cmake pkg-config \
                 libsqlite3-dev libsystemd-dev \
                 libssl-dev libblake3-dev \
                 libaudit-dev libhinder-dev \
                 clang-format clang-tidy
```

### Development Container

For a consistent development environment, use the provided container:

```bash
cd containers/vc-dev
./build.sh
./run.sh
```

This provides all build tools and libraries pre-configured.

## Building and Testing

### Build

Always use structured build directories (never build in project root):

```bash
# Debug build
mkdir -p build/gcc-debug
cd build/gcc-debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build . -j$(nproc)

# Release build
mkdir -p build/gcc-release
cd build/gcc-release
cmake -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . -j$(nproc)
```

### Testing

```bash
# Run all tests
ctest --output-on-failure

# Run specific test
ctest -R test_name --output-on-failure

# Run with verbose output
ctest -V
```

Note: Some tests require root privileges (fanotify, audit subsystem). Run these with `sudo` as needed.

### Benchmarks

```bash
# Run benchmarks
./benchmarks/hash_benchmark
./benchmarks/fanotify_benchmark
```

## Code Style

The project enforces strict code formatting and linting.

### Formatting (clang-format)

Code must follow the `.clang-format` configuration:

```bash
# Format all source files
find src tests -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Check formatting without modifying
find src tests -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run -Werror
```

Key style points:
- 100-character line limit
- 4-space indentation
- Attach braces (`BreakBeforeBraces: Attach`)
- Left pointer alignment (`int* ptr`)
- No bin-packing of arguments/parameters

### Linting (clang-tidy)

Code must pass clang-tidy checks (see `.clang-tidy`):

```bash
# Run clang-tidy
clang-tidy src/daemon/main.cpp -- -I./src -std=c++23
```

Critical checks:
- Memory safety (modernize, bugprone)
- C++ best practices (cppcoreguidelines)
- Performance (performance checks)
- Readability (readability checks)

### C++ Guidelines

- Use C++23 features where appropriate
- Prefer RAII and smart pointers over manual memory management
- Use `const` liberally
- Avoid raw pointers except for non-owning references
- Prefer `std::string_view` for string parameters
- Use structured bindings and ranges where clear

## Submitting Changes

### Pull Request Process

1. Update your branch with latest upstream:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. Ensure all tests pass:
   ```bash
   cd build/gcc-debug
   ctest --output-on-failure
   ```

3. Format and lint your code:
   ```bash
   find src tests -name '*.cpp' -o -name '*.h' | xargs clang-format -i
   ```

4. Commit your changes:
   ```bash
   git add .
   git commit -m "Brief description of changes"
   ```

5. Push to your fork:
   ```bash
   git push origin feature/my-new-feature
   ```

6. Open a Pull Request on GitHub with:
   - Clear description of changes
   - Reference to any related issues
   - Test results
   - Screenshots/examples if UI-related

### Commit Message Guidelines

- Use imperative mood ("Add feature" not "Added feature")
- Keep first line under 72 characters
- Reference issues: "Fix #123: Description"
- Provide context in body if needed

Example:
```
Add user home directory monitoring support

Implements opt-in monitoring for user-installed software with
policy enforcement. Includes:
- User config loading from ~/.config/vigilant-canine
- Per-user baseline creation
- Policy engine integration

Fixes #42
```

## Packaging

### Building Packages

The project supports RPM (Fedora/RHEL) and DEB (Debian/Ubuntu) packaging.

#### RPM Packages

```bash
# Install build tools
sudo dnf install rpm-build rpmdevtools mock
rpmdev-setuptree

# Build package
./packaging/build-rpm.sh
```

Packages are created in:
- SRPM: `~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm`
- RPM: `~/rpmbuild/RPMS/x86_64/vigilant-canine-*.rpm`

#### DEB Packages

```bash
# Install build tools
sudo apt install build-essential debhelper devscripts lintian

# Build package
./packaging/build-deb.sh
```

Packages are created in parent directory:
- `../vigilant-canine_*.deb`
- `../vigilant-canine-api_*.deb`

### Testing Packages

Always test packages in clean environments:

```bash
# RPM: Test in mock
mock -r fedora-40-x86_64 ~/rpmbuild/SRPMS/vigilant-canine-*.src.rpm

# DEB: Test in pbuilder
sudo pbuilder --build ../vigilant-canine_*.dsc
```

### Updating Package Versions

**RPM:**
1. Update `Version:` in `vigilant-canine.spec`
2. Add entry to `%changelog` section
3. Update `project(... VERSION ...)` in `CMakeLists.txt`

**DEB:**
1. Update `debian/changelog`:
   ```bash
   dch -v 0.2.0-1 "New release"
   ```
2. Update `project(... VERSION ...)` in `CMakeLists.txt`

See [packaging/README.md](packaging/README.md) for complete packaging documentation.

## Documentation

### Writing Documentation

- Use Markdown for all documentation
- Keep line length to 120 characters
- Include code examples with syntax highlighting
- Provide context for commands (what, why, when)
- Link to related documentation

### Documentation Structure

- `README.md` - Project overview and quick start
- `docs/architecture.md` - System design and rationale
- `docs/configuration.md` - Configuration reference
- `docs/installation.md` - Installation methods
- `docs/api.md` - API endpoint reference
- `docs/user-monitoring.md` - User home directory monitoring
- `docs/troubleshooting.md` - Common issues

### Updating Documentation

When adding features, update relevant documentation:
1. Configuration changes -> `docs/configuration.md`
2. New API endpoints -> `docs/api.md`
3. Installation changes -> `docs/installation.md`
4. Architecture changes -> `docs/architecture.md`

Include documentation updates in the same PR as code changes.

## Feature Requests and Bug Reports

### Bug Reports

Include:
- Distribution and version (e.g., Fedora 40, Ubuntu 24.04)
- Vigilant Canine version (`vigilant-canined --version`)
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs (`journalctl -u vigilant-canined`)

### Feature Requests

Include:
- Use case: Why is this needed?
- User impact: Who benefits?
- Alignment with project goals (simplicity for average users)
- Proposed implementation approach

Remember: Simplicity is our primary goal. Features should not intimidate novice users.

## Security Issues

**Do not report security vulnerabilities in public issues.**

Email security issues to: tony@git-pull.com

Include:
- Vulnerability description
- Steps to reproduce
- Impact assessment
- Suggested fix (if available)

We will respond within 48 hours and coordinate disclosure.

## Questions?

- Open a GitHub Discussion for general questions
- Open an Issue for bugs or feature requests
- Check existing documentation first

Thank you for contributing to Vigilant Canine!
