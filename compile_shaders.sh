#!/bin/bash

# Check if glslc is available
if command -v glslc &> /dev/null; then
    echo "Using glslc for shader compilation"
    COMPILER="glslc"
elif command -v glslangValidator &> /dev/null; then
    echo "Using glslangValidator for shader compilation"
    COMPILER="glslangValidator"
else
    echo "Error: No shader compiler found. Please install either glslc (from Vulkan SDK) or glslangValidator."
    exit 1
fi

# Create the SPIR-V output directory
mkdir -p shaders/spirv

# Function to compile shader
compile_shader() {
    local input=$1
    local output=$2
    echo "Compiling $input -> $output"
    
    if [ "$COMPILER" = "glslc" ]; then
        glslc -o "$output" "$input"
    else
        glslangValidator -V "$input" -o "$output"
    fi
    
    if [ $? -eq 0 ]; then
        echo "Successfully compiled $input"
    else
        echo "Failed to compile $input"
        exit 1
    fi
}

# Compile all vertex shaders
for shader in shaders/*.vert; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader")
        compile_shader "$shader" "shaders/spirv/${basename}.spv"
    fi
done

# Compile all fragment shaders
for shader in shaders/*.frag; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader")
        compile_shader "$shader" "shaders/spirv/${basename}.spv"
    fi
done

echo "Shader compilation complete!"