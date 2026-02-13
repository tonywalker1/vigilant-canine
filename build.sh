#!/usr/bin/env bash

set -e  # Exit on first error

# Default flags: gcc, release, with tests
FLAGS="${1:--grt}"

# Remove leading dash if present
FLAGS="${FLAGS#-}"

# Parse flags
USE_GCC=false
USE_CLANG=false
USE_DEBUG=false
USE_RELEASE=false
VERBOSE=false
RUN_TESTS=false

for (( i=0; i<${#FLAGS}; i++ )); do
    flag="${FLAGS:$i:1}"
    case $flag in
        g) USE_GCC=true ;;
        c) USE_CLANG=true ;;
        d) USE_DEBUG=true ;;
        r) USE_RELEASE=true ;;
        v) VERBOSE=true ;;
        t) RUN_TESTS=true ;;
        *)
           echo "Unknown flag: -$flag" >&2
           echo "Usage: $0 [-gcdrvt]" >&2
           echo "  -g    Use gcc" >&2
           echo "  -c    Use clang" >&2
           echo "  -d    Debug build" >&2
           echo "  -r    Release build" >&2
           echo "  -v    Verbose output" >&2
           echo "  -t    Run tests" >&2
           echo "Default: -grt (gcc, release, with tests)" >&2
           exit 1 ;;
    esac
done

# Build arrays of compilers and build types
COMPILERS=()
[[ "$USE_GCC" == true ]] && COMPILERS+=(gcc)
[[ "$USE_CLANG" == true ]] && COMPILERS+=(clang)

BUILD_TYPES=()
[[ "$USE_DEBUG" == true ]] && BUILD_TYPES+=(Debug)
[[ "$USE_RELEASE" == true ]] && BUILD_TYPES+=(Release)

# If no compilers specified, default to gcc
[[ ${#COMPILERS[@]} -eq 0 ]] && COMPILERS=(gcc)

# If no build types specified, default to Release
[[ ${#BUILD_TYPES[@]} -eq 0 ]] && BUILD_TYPES=(Release)

# Verbose flag for cmake
CMAKE_VERBOSE=""
BUILD_VERBOSE=""
if [[ "$VERBOSE" == true ]]; then
    CMAKE_VERBOSE="--verbose"
    BUILD_VERBOSE="--verbose"
fi

# Project root (where this script lives)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Build each combination
for compiler in "${COMPILERS[@]}"; do
    for build_type in "${BUILD_TYPES[@]}"; do
        # Determine compiler executables
        if [[ "$compiler" == "gcc" ]]; then
            CC_COMPILER="gcc"
            CXX_COMPILER="g++"
        else
            CC_COMPILER="clang"
            CXX_COMPILER="clang++"
        fi

        # Build directory name
        BUILD_DIR_NAME="${compiler}-$(echo "$build_type" | tr '[:upper:]' '[:lower:]')"
        BUILD_DIR="${PROJECT_ROOT}/build/${BUILD_DIR_NAME}"

        echo "============================================="
        echo "Building: ${BUILD_DIR_NAME}"
        echo "============================================="

        # 1. Remove old build directory
        rm -rf "$BUILD_DIR"

        # 2. Create new build directory
        mkdir -p "$BUILD_DIR"

        # 3. Enter build directory
        pushd "$BUILD_DIR" > /dev/null

        # 4. Configure cmake
        echo "Configuring CMake..."
        cmake ${CMAKE_VERBOSE} \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DCMAKE_C_COMPILER="$CC_COMPILER" \
            -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
            ../..

        # 5. Build
        echo "Building..."
        cmake --build . -j$(nproc) ${BUILD_VERBOSE}

        # 6. Run tests if requested
        if [[ "$RUN_TESTS" == true ]]; then
            echo "Running tests..."
            ctest --output-on-failure
        fi

        # 7. Return to project root
        popd > /dev/null

        echo "✓ ${BUILD_DIR_NAME} complete"
        echo ""
    done
done

echo "============================================="
echo "All builds complete!"
echo "============================================="
