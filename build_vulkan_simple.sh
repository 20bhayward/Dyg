#!/bin/bash

# Script to build PixelPhys2D with Vulkan support using the simplified main file
set -e

echo "===== Building PixelPhys2D with Vulkan support (simple version) ====="

# Make sure build directory exists
mkdir -p build_vulkan
cd build_vulkan

# Configure with Vulkan support
echo "Running CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_VULKAN=ON

# Create executable name
EXECUTABLE="PixelPhys2D_Vulkan"

# Compile the Vulkan version with the simplified main
echo "Building Vulkan version..."
g++ -std=c++17 -O2 -o $EXECUTABLE ../src/main_vulkan.cpp ../src/World.cpp ../src/Renderer.cpp ../src/UI.cpp ../src/OpenGLBackend.cpp ../src/VulkanBackend.cpp -I../include -lvulkan -lSDL2 -lGL -lGLEW -DUSE_VULKAN

if [ $? -eq 0 ]; then
  echo ""
  echo "===== Build completed successfully! ====="
  echo "Run the Vulkan version with: ./build_vulkan/PixelPhys2D_Vulkan"
  echo ""
  echo "The Vulkan integration is a basic implementation that demonstrates"
  echo "integration with SDL2 and the rendering system."
else
  echo ""
  echo "===== Build FAILED ====="
  echo "Please fix the errors and try again."
fi