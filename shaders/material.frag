#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

// Output
layout(location = 0) out vec4 outColor;

// Simple time uniform
layout(binding = 0) uniform UniformBufferObject {
    vec4 time;  // x = total time, y = delta time, z = frame count, w = unused
} ubo;

// Super simplified material push constant
layout(push_constant) uniform MaterialPushConstants {
    uint materialType;  // Just the material type ID
} material;

void main() {
    // Start with fragment color
    vec4 finalColor = fragColor;
    
    // Apply material-specific overrides
    if (material.materialType == 5) { // Fire
        // Make fire orange/red with flickering
        float flicker = sin(ubo.time.x * 10.0) * 0.2 + 0.8;
        finalColor = vec4(1.0, 0.3, 0.0, 1.0) * flicker;
    } 
    else if (material.materialType == 2) { // Water
        // Make water blue
        finalColor = vec4(0.0, 0.5, 1.0, 1.0);
    }
    
    // Output the final color
    outColor = finalColor;
}