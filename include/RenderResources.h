#pragma once

#include "RenderBackend.h"
#include <string>
#include <vector>
#include <memory>

namespace PixelPhys {

// Forward declaration for cross-references
class RenderBackend;

// Basic buffer class to represent vertex, index, and uniform buffers
class Buffer {
public:
    enum class Type {
        Vertex,
        Index,
        Uniform
    };

    Buffer(RenderBackend* backend, Type type, size_t size) : 
        m_backend(backend), m_type(type), m_size(size), m_nativeHandle(nullptr) {}
    
    virtual ~Buffer() = default;

    Type getType() const { return m_type; }
    size_t getSize() const { return m_size; }
    void* getNativeHandle() const { return m_nativeHandle; }

protected:
    RenderBackend* m_backend;
    Type m_type;
    size_t m_size;
    void* m_nativeHandle;
};

// Texture resource
class Texture {
public:
    Texture(RenderBackend* backend, int width, int height, bool hasAlpha) :
        m_backend(backend), m_width(width), m_height(height), m_hasAlpha(hasAlpha),
        m_nativeHandle(nullptr) {}
    
    virtual ~Texture() = default;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool hasAlpha() const { return m_hasAlpha; }
    void* getNativeHandle() const { return m_nativeHandle; }

protected:
    RenderBackend* m_backend;
    int m_width;
    int m_height;
    bool m_hasAlpha;
    void* m_nativeHandle;
};

// Shader program resource
class Shader {
public:
    Shader(RenderBackend* backend, const std::string& vertexSource, const std::string& fragmentSource) :
        m_backend(backend), m_vertexSource(vertexSource), m_fragmentSource(fragmentSource),
        m_nativeHandle(nullptr) {}
    
    virtual ~Shader() = default;

    const std::string& getVertexSource() const { return m_vertexSource; }
    const std::string& getFragmentSource() const { return m_fragmentSource; }
    void* getNativeHandle() const { return m_nativeHandle; }

    // Set uniform values
    virtual void setUniform(const std::string& name, float value) = 0;
    virtual void setUniform(const std::string& name, int value) = 0;
    virtual void setUniform(const std::string& name, const std::vector<float>& values) = 0;
    virtual void setUniform(const std::string& name, float x, float y) = 0;
    virtual void setUniform(const std::string& name, float x, float y, float z) = 0;
    virtual void setUniform(const std::string& name, float x, float y, float z, float w) = 0;

protected:
    RenderBackend* m_backend;
    std::string m_vertexSource;
    std::string m_fragmentSource;
    void* m_nativeHandle;
};

// Render target (framebuffer) resource
class RenderTarget {
public:
    RenderTarget(RenderBackend* backend, int width, int height, bool hasDepth, bool multisampled) :
        m_backend(backend), m_width(width), m_height(height), 
        m_hasDepth(hasDepth), m_multisampled(multisampled),
        m_nativeHandle(nullptr), m_colorTexture(nullptr), m_depthTexture(nullptr) {}
    
    virtual ~RenderTarget() = default;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool hasDepth() const { return m_hasDepth; }
    bool isMultisampled() const { return m_multisampled; }
    void* getNativeHandle() const { return m_nativeHandle; }
    
    // Get the color and depth textures of this render target
    std::shared_ptr<Texture> getColorTexture() const { return m_colorTexture; }
    std::shared_ptr<Texture> getDepthTexture() const { return m_depthTexture; }

protected:
    RenderBackend* m_backend;
    int m_width;
    int m_height;
    bool m_hasDepth;
    bool m_multisampled;
    void* m_nativeHandle;
    std::shared_ptr<Texture> m_colorTexture;
    std::shared_ptr<Texture> m_depthTexture;
};

} // namespace PixelPhys