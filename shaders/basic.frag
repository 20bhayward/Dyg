#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

// Output
layout(location = 0) out vec4 outColor;

// No texture sampling - simplest possible shader

void main() {
    // Just use the vertex color directly
    outColor = fragColor;
}