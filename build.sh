#!/bin/bash

# Unified build script for PixelPhys2D
set -e

# Default values
BUILD_TYPE="Release"
USE_VULKAN=false
CLEAN_BUILD=false
VERBOSE=false
BUILD_DIR="build"
RUN_AFTER_BUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --vulkan|-v)
      USE_VULKAN=true
      shift
      ;;
    --debug|-d)
      BUILD_TYPE="Debug"
      shift
      ;;
    --clean|-c)
      CLEAN_BUILD=true
      shift
      ;;
    --verbose)
      VERBOSE=true
      shift
      ;;
    --run|-r)
      RUN_AFTER_BUILD=true
      shift
      ;;
    --help|-h)
      echo "Usage: ./build.sh [options]"
      echo "Options:"
      echo "  --vulkan, -v     Build with Vulkan support"
      echo "  --debug, -d      Build with debug configuration"
      echo "  --clean, -c      Do a clean build (removes build directory)"
      echo "  --verbose        Show verbose build output"
      echo "  --run, -r        Run the application after building"
      echo "  --help, -h       Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help to see available options"
      exit 1
      ;;
  esac
done

# Setup build directory
if $CLEAN_BUILD; then
  echo "Performing clean build..."
  rm -rf $BUILD_DIR
fi

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Generate cmake flags
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if $USE_VULKAN; then
  echo "Building with Vulkan support..."
  
  # Check for Vulkan SDK
  if [ -d "/usr/include/vulkan" ] || [ -d "/usr/local/include/vulkan" ]; then
    echo "Vulkan headers found"
  else
    echo "WARNING: Vulkan headers not found. You may need to install the Vulkan SDK."
    echo "On Ubuntu/Debian: sudo apt-get install libvulkan-dev"
    echo "On Fedora: sudo dnf install vulkan-headers vulkan-loader-devel"
    echo "On Arch: sudo pacman -S vulkan-headers"
    echo "Alternatively, download the Vulkan SDK from https://vulkan.lunarg.com/"
  fi
  
  CMAKE_FLAGS="$CMAKE_FLAGS -DUSE_VULKAN=ON"
else
  echo "Building with OpenGL support..."
fi

# Configure with CMake
echo "Running CMake configuration with $CMAKE_FLAGS..."
cmake .. $CMAKE_FLAGS

# Set make verbosity
MAKE_FLAGS=""
if $VERBOSE; then
  MAKE_FLAGS="VERBOSE=1"
fi

# Build with all available CPU cores
echo "Building the project..."
cmake --build . -j$(nproc) $MAKE_FLAGS

# Report build success
if [ $? -eq 0 ]; then
  echo ""
  echo "===== Build completed successfully! ====="
  
  # Run if requested
  if $RUN_AFTER_BUILD; then
    if $USE_VULKAN; then
      echo "Running PixelPhys2D with Vulkan backend..."
    else
      echo "Running PixelPhys2D with OpenGL backend..."
    fi
    
    ./PixelPhys2D
  else
    # Just show run instructions
    echo "Run the application with: $BUILD_DIR/PixelPhys2D"
  fi
else
  echo ""
  echo "===== Build FAILED ====="
  echo "Please fix the errors and try again."
  exit 1
fi