#pragma once

#include "World.h"
#include "RenderBackend.h"
#include "RenderResources.h"
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <vector>

#ifndef USE_VULKAN
#include <GL/glew.h>
#endif

namespace PixelPhys {

class Renderer {
public:
    Renderer(int screenWidth, int screenHeight, BackendType type = BackendType::OpenGL);
    ~Renderer();
    
    // Initialize rendering resources
    bool initialize();
    
    // Initialize with an existing window (for Vulkan)
    bool initialize(SDL_Window* window);
    
    // Begin frame rendering (for direct rendering mode)
    void beginFrame();
    
    // End frame rendering (for direct rendering mode)
    void endFrame();
    
    // Render the world to the screen
    void render(const World& world);
    
    // Clean up rendering resources
    void cleanup();
    
    // Set rendering backend manually (default is platform-dependent)
    // Returns true if successful, false if the backend type is not supported
    bool setBackendType(BackendType type);
    
    // Get current backend type
    BackendType getBackendType() const;
    
    // Get access to the backend for direct rendering
    RenderBackend* getBackend() const { return m_backend.get(); }
    
    // Create rendering resources
    std::shared_ptr<Buffer> createVertexBuffer(size_t size, const void* data);
    std::shared_ptr<Buffer> createIndexBuffer(size_t size, const void* data);
    std::shared_ptr<Shader> createShader(const std::string& vertexSource, const std::string& fragmentSource);
    
    // Get information about the current renderer
    std::string getRendererInfo() const;
    
private:
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // Rendering backend
    std::unique_ptr<RenderBackend> m_backend;
    
    // Texture resource for the world data
    std::shared_ptr<Texture> m_worldTexture;
    
    // Shader programs
    std::shared_ptr<Shader> m_mainShader;          // Main rendering shader
    std::shared_ptr<Shader> m_shadowShader;        // Shadow calculation shader
    std::shared_ptr<Shader> m_volumetricLightShader; // Volumetric lighting shader
    std::shared_ptr<Shader> m_bloomShader;         // Bloom effect shader
    std::shared_ptr<Shader> m_postProcessShader;   // Final post-processing shader
    
    // Render targets for multi-pass rendering
    std::shared_ptr<RenderTarget> m_mainRenderTarget;    // Main scene
    std::shared_ptr<RenderTarget> m_shadowMapTarget;     // Shadow map
    std::shared_ptr<RenderTarget> m_volumetricLightTarget; // Volumetric lighting 
    std::shared_ptr<RenderTarget> m_bloomTarget;         // Bloom effect
    
#ifndef USE_VULKAN
    // OpenGL specific resources
    GLuint m_textureID = 0;          // Main world texture ID
    GLuint m_shaderProgram = 0;      // Main shader program
    GLuint m_shadowShader = 0;       // Shadow calculation shader
    GLuint m_volumetricLightShader = 0; // Volumetric lighting shader
    GLuint m_bloomShader = 0;        // Bloom effect shader
    GLuint m_postProcessShader = 0;  // Final post-processing shader
    
    // OpenGL framebuffer objects
    GLuint m_mainFBO = 0;
    GLuint m_shadowFBO = 0;
    GLuint m_volumetricLightFBO = 0;
    GLuint m_bloomFBO = 0;
    
    // OpenGL textures
    GLuint m_mainColorTexture = 0;
    GLuint m_mainDepthTexture = 0;
    GLuint m_shadowDepthTexture = 0;
    GLuint m_volumetricLightTexture = 0;
    GLuint m_bloomTexture = 0;
#endif
    
    // Create shaders and render targets based on the active backend
    bool createRenderingResources();
    
    // Create texture (implementation varies by backend)
    bool createTexture(int width, int height);
    
    // Create framebuffers for multi-pass rendering
    void createFramebuffers();
    
    // Update texture with new data
    void updateTexture(const World& world);
    
    // Update the world texture with the latest world data
    void updateWorldTexture(const World& world);
    
    // Define shader sources
    std::string getMainVertexShaderSource() const;
    std::string getMainFragmentShaderSource() const;
    std::string getShadowVertexShaderSource() const;
    std::string getShadowFragmentShaderSource() const;
    std::string getVolumetricLightVertexShaderSource() const;
    std::string getVolumetricLightFragmentShaderSource() const;
    std::string getBloomVertexShaderSource() const;
    std::string getBloomFragmentShaderSource() const;
    std::string getPostProcessVertexShaderSource() const;
    std::string getPostProcessFragmentShaderSource() const;
    
    // Rendering passes
    void renderMainPass(const World& world);
    void renderShadowPass(const World& world);
    void renderVolumetricLightingPass(const World& world);
    void renderBloomPass();
    void renderPostProcessPass();
    
#ifndef USE_VULKAN
    // OpenGL shader compilation helpers
    bool createShadersAndPrograms();
#endif
    
    // Helper to select the best rendering backend based on the system
    BackendType detectBestBackendType();
};

} // namespace PixelPhys