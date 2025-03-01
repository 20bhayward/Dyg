#pragma once

#include "World.h"
#include <vector>
#include <string>
#include <memory>

namespace PixelPhys {

// Forward declaration
class Texture;
class Shader;
class RenderTarget;
class Buffer;

// Enumeration for available rendering backends
enum class BackendType {
    Vulkan
};

// Utility class to store graphics options/capabilities
struct GraphicsOptions {
    bool enableVSync = true;
    bool enableMSAA = true;
    int msaaSamples = 4;
    bool enableShadows = true;
    bool enableBloom = true;
    bool enableVolumetricLighting = true;
    bool enablePostProcessing = true;
};

// Base abstract class for all rendering backends
class RenderBackend {
public:
    RenderBackend(int screenWidth, int screenHeight) : 
        m_screenWidth(screenWidth), 
        m_screenHeight(screenHeight) {}
    
    virtual ~RenderBackend() = default;
    
    // Backend initialization and shutdown
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    
    // Frame management
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    
    // Buffer handling
    virtual std::shared_ptr<Buffer> createVertexBuffer(size_t size, const void* data) = 0;
    virtual std::shared_ptr<Buffer> createIndexBuffer(size_t size, const void* data) = 0;
    virtual std::shared_ptr<Buffer> createUniformBuffer(size_t size) = 0;
    virtual void updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, size_t size) = 0;
    
    // Textures
    virtual std::shared_ptr<Texture> createTexture(int width, int height, bool hasAlpha) = 0;
    virtual void updateTexture(std::shared_ptr<Texture> texture, const void* data) = 0;
    
    // Shaders
    virtual std::shared_ptr<Shader> createShader(const std::string& vertexSource, const std::string& fragmentSource) = 0;
    virtual void bindShader(std::shared_ptr<Shader> shader) = 0;
    
    // Render targets
    virtual std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, bool hasDepth, bool multisampled) = 0;
    virtual void bindRenderTarget(std::shared_ptr<RenderTarget> target) = 0;
    virtual void bindDefaultRenderTarget() = 0;
    
    // Drawing
    virtual void drawMesh(std::shared_ptr<Buffer> vertexBuffer, size_t vertexCount, std::shared_ptr<Buffer> indexBuffer, size_t indexCount) = 0;
    virtual void drawFullscreenQuad() = 0;
    
    // State management
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void clear() = 0;
    
    // Multi-pass rendering support
    virtual void beginShadowPass() = 0;
    virtual void beginMainPass() = 0;
    virtual void beginPostProcessPass() = 0;
    
    // Backend-specific operations
    virtual void* getNativeHandle() = 0;
    virtual BackendType getType() const = 0;
    
    // Helper functions
    virtual bool supportsFeature(const std::string& featureName) const = 0;
    virtual std::string getRendererInfo() const = 0;
    
    // Accessors
    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }
    
protected:
    int m_screenWidth;
    int m_screenHeight;
};

// Factory function to create the appropriate backend
std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int screenWidth, int screenHeight);

} // namespace PixelPhys