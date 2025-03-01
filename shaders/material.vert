#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input vertex attributes
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

// Simple time uniform
layout(binding = 0) uniform UniformBufferObject {
    vec4 time;  // x = total time, y = delta time, z = frame count, w = unused
} ubo;

void main() {
    // Most basic transformation
    gl_Position = vec4(inPosition, 0.0, 1.0);
    
    // Pass texture coordinates and color to fragment shader
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}