#pragma once

#include "RenderBackend.h"
#include "RenderResources.h"
#include <GL/glew.h>
#include <string>
#include <memory>
#include <unordered_map>

namespace PixelPhys {

// OpenGL implementation of Buffer
class OpenGLBuffer : public Buffer {
public:
    OpenGLBuffer(RenderBackend* backend, Type type, size_t size, const void* data);
    ~OpenGLBuffer() override;

    GLuint getGLHandle() const { return m_glBuffer; }

private:
    GLuint m_glBuffer;
};

// OpenGL implementation of Texture
class OpenGLTexture : public Texture {
public:
    OpenGLTexture(RenderBackend* backend, int width, int height, bool hasAlpha);
    ~OpenGLTexture() override;

    void update(const void* data);
    GLuint getGLHandle() const { return m_glTexture; }

private:
    GLuint m_glTexture;
};

// OpenGL implementation of Shader
class OpenGLShader : public Shader {
public:
    OpenGLShader(RenderBackend* backend, const std::string& vertexSource, const std::string& fragmentSource);
    ~OpenGLShader() override;

    GLuint getGLHandle() const { return m_glProgram; }
    
    // Uniform setters
    void setUniform(const std::string& name, float value) override;
    void setUniform(const std::string& name, int value) override;
    void setUniform(const std::string& name, const std::vector<float>& values) override;
    void setUniform(const std::string& name, float x, float y) override;
    void setUniform(const std::string& name, float x, float y, float z) override;
    void setUniform(const std::string& name, float x, float y, float z, float w) override;

private:
    GLuint m_glProgram;
    std::unordered_map<std::string, GLint> m_uniformLocations;

    GLint getUniformLocation(const std::string& name);
    GLuint compileShader(const std::string& source, GLenum type);
};

// OpenGL implementation of RenderTarget
class OpenGLRenderTarget : public RenderTarget {
public:
    OpenGLRenderTarget(RenderBackend* backend, int width, int height, bool hasDepth, bool multisampled);
    ~OpenGLRenderTarget() override;

    GLuint getGLHandle() const { return m_glFramebuffer; }

private:
    GLuint m_glFramebuffer;
    GLuint m_glColorTexture;
    GLuint m_glDepthTexture;
    GLuint m_glColorRenderBuffer;
    GLuint m_glDepthRenderBuffer;
};

// OpenGL implementation of RenderBackend
class OpenGLBackend : public RenderBackend {
public:
    OpenGLBackend(int screenWidth, int screenHeight);
    ~OpenGLBackend() override;

    // Implementation of RenderBackend methods
    bool initialize() override;
    void cleanup() override;
    
    void beginFrame() override;
    void endFrame() override;
    
    std::shared_ptr<Buffer> createVertexBuffer(size_t size, const void* data) override;
    std::shared_ptr<Buffer> createIndexBuffer(size_t size, const void* data) override;
    std::shared_ptr<Buffer> createUniformBuffer(size_t size) override;
    void updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, size_t size) override;
    
    std::shared_ptr<Texture> createTexture(int width, int height, bool hasAlpha) override;
    void updateTexture(std::shared_ptr<Texture> texture, const void* data) override;
    
    std::shared_ptr<Shader> createShader(const std::string& vertexSource, const std::string& fragmentSource) override;
    void bindShader(std::shared_ptr<Shader> shader) override;
    
    std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, bool hasDepth, bool multisampled) override;
    void bindRenderTarget(std::shared_ptr<RenderTarget> target) override;
    void bindDefaultRenderTarget() override;
    
    void drawMesh(std::shared_ptr<Buffer> vertexBuffer, size_t vertexCount, 
                 std::shared_ptr<Buffer> indexBuffer, size_t indexCount) override;
    void drawFullscreenQuad() override;
    
    void setViewport(int x, int y, int width, int height) override;
    void setClearColor(float r, float g, float b, float a) override;
    void clear() override;
    
    void beginShadowPass() override;
    void beginMainPass() override;
    void beginPostProcessPass() override;
    
    void* getNativeHandle() override;
    BackendType getType() const override { return BackendType::OpenGL; }
    
    bool supportsFeature(const std::string& featureName) const override;
    std::string getRendererInfo() const override;

private:
    // OpenGL resources
    GLuint m_defaultVAO;
    GLuint m_fullscreenQuadVBO;
    GLuint m_fullscreenQuadVAO;
    std::shared_ptr<RenderTarget> m_shadowMapTarget;
    std::shared_ptr<RenderTarget> m_mainRenderTarget;
    std::shared_ptr<Shader> m_currentShader;
    
    // Initialize full screen quad for post-processing
    void initializeFullscreenQuad();
};

} // namespace PixelPhys