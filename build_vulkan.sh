#!/bin/bash

# Script to build PixelPhys2D with Vulkan support
set -e

echo "===== Building PixelPhys2D with Vulkan support ====="

# Check for Vulkan SDK
echo "Checking for Vulkan headers and libraries..."
if [ -d "/usr/include/vulkan" ] || [ -d "/usr/local/include/vulkan" ]; then
    echo "Vulkan headers found"
else
    echo "WARNING: Vulkan headers not found. You may need to install the Vulkan SDK."
    echo "On Ubuntu/Debian: sudo apt-get install libvulkan-dev"
    echo "On Fedora: sudo dnf install vulkan-headers vulkan-loader-devel"
    echo "On Arch: sudo pacman -S vulkan-headers"
    echo "Alternatively, download the Vulkan SDK from https://vulkan.lunarg.com/"
fi

# Make sure build directory exists
mkdir -p build
cd build

# Configure with Vulkan support
echo "Running CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_VULKAN=ON

# Build with all available CPU cores
echo "Building the project..."
cmake --build . -j$(nproc)

echo ""
echo "===== Build completed successfully! ====="
echo "Run the game with: ./build/PixelPhys2D"