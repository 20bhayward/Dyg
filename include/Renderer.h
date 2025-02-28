#pragma once

#include "World.h"
#include <SDL2/SDL.h>

// GLEW must be included before any OpenGL headers
#include <GL/glew.h>
#include <GL/gl.h>
#include <vector>

#ifdef USE_OPENVDB
#include <openvdb/openvdb.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/LevelSetSphere.h>
#endif

namespace PixelPhys {

class Renderer {
public:
    Renderer(int screenWidth, int screenHeight);
    ~Renderer();
    
    // Initialize OpenGL resources
    bool initialize();
    
    // Render the world to the screen
    void render(const World& world);
    
    // Clean up OpenGL resources
    void cleanup();
    
private:
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // OpenGL texture ID for the world
    GLuint m_textureID;
    
    // Shader program IDs
    GLuint m_shaderProgram;           // Main rendering shader
    GLuint m_shadowShader;            // Shadow calculation shader
    GLuint m_volumetricLightShader;   // Volumetric lighting and GI shader
    GLuint m_lightPropagationShader;  // Light propagation compute shader (2D lighting)
    GLuint m_bloomShader;             // Bloom effect shader
    GLuint m_postProcessShader;       // Final post-processing shader
    
    // Vertex buffer objects
    GLuint m_vbo;
    GLuint m_vao;
    
    // Framebuffer objects for multi-pass rendering
    GLuint m_mainFBO;             // Main scene render target
    GLuint m_shadowMapFBO;        // Shadow map
    GLuint m_volumetricLightFBO;  // Volumetric lighting and GI
    GLuint m_bloomFBO;            // Bloom effect buffer
    
    // Textures for the framebuffers
    GLuint m_mainColorTexture;
    GLuint m_mainEmissiveTexture;
    GLuint m_mainDepthTexture;    // Depth information for lighting
    GLuint m_shadowMapTexture;
    GLuint m_volumetricLightTexture;
    GLuint m_bloomTexture;
    
#ifdef USE_OPENVDB
    // OpenVDB grid for volumetric lighting
    openvdb::FloatGrid::Ptr m_lightVolume;
    openvdb::FloatGrid::Ptr m_densityVolume;
#endif

    // Light grid for 2D lighting propagation (used when OpenVDB isn't available)
    std::vector<float> m_lightGrid;
    int m_lightGridWidth;
    int m_lightGridHeight;
    
    // Create our shader programs
    bool createShaders();
    
    // Create framebuffers for multi-pass rendering
    void createFramebuffers();
    
    // Create a texture to hold the world pixel data
    bool createTexture(int width, int height);
    
    // Update the texture with world pixel data
    void updateTexture(const World& world);
    
    // Shadow map generation pass
    void renderShadowMap(const World& world);
    
    // Volumetric lighting and global illumination
    void renderVolumetricLighting(const World& world);
    
    // Bloom effect generation pass
    void renderBloomEffect();
    
    // Apply post-processing effects
    void applyPostProcessing();
    
    // Utility function to create and compile shaders
    GLuint compileShader(const char* source, GLenum type);
    
    // Utility to create a shader program from vertex and fragment sources
    GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);
};

} // namespace PixelPhys