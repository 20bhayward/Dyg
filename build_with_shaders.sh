#!/bin/bash

# Ensure shader directory exists
mkdir -p shaders/spirv

# Check if glslc is available
if command -v glslc &> /dev/null; then
    echo "Using glslc for shader compilation"
    COMPILER="glslc"
elif command -v glslangValidator &> /dev/null; then
    echo "Using glslangValidator for shader compilation"
    COMPILER="glslangValidator"
else
    echo "Error: No shader compiler found. Please install Vulkan SDK with glslc or glslangValidator."
    exit 1
fi

# Compile all shader files
echo "Compiling shaders..."
./compile_shaders.sh

# Check if shader compilation succeeded
if [ $? -ne 0 ]; then
    echo "Shader compilation failed! Check error messages above."
    exit 1
fi

# Build the Vulkan application using the main build script with proper flags
echo "Building Vulkan application..."
./build.sh --vulkan --clean

# Check if build succeeded
if [ $? -ne 0 ]; then
    echo "Build failed! Check error messages above."
    exit 1
fi

echo "Build completed successfully!"
echo "Running application..."

# Run the application
./build/PixelPhys2D
