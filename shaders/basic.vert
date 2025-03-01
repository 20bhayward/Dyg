#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input vertex attributes - very simple 2D positions
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

// Simple 2D transformation matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // Most basic transformation
    gl_Position = vec4(inPosition, 0.0, 1.0);
    
    // Just pass the texture coordinates and color to the fragment shader
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}