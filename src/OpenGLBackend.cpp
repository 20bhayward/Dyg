#include "../include/OpenGLBackend.h"
#ifdef USE_VULKAN
#include "../include/VulkanBackend.h"
#endif
#include <iostream>
#include <vector>
#include <cstring>

namespace PixelPhys {

// OpenGLBuffer implementation
OpenGLBuffer::OpenGLBuffer(RenderBackend* backend, Type type, size_t size, const void* data)
    : Buffer(backend, type, size), m_glBuffer(0) {
    
    GLenum bufferType = GL_ARRAY_BUFFER;
    GLenum usage = GL_STATIC_DRAW;
    
    switch (type) {
        case Type::Vertex:
            bufferType = GL_ARRAY_BUFFER;
            break;
        case Type::Index:
            bufferType = GL_ELEMENT_ARRAY_BUFFER;
            break;
        case Type::Uniform:
            bufferType = GL_UNIFORM_BUFFER;
            usage = GL_DYNAMIC_DRAW;
            break;
    }
    
    glGenBuffers(1, &m_glBuffer);
    glBindBuffer(bufferType, m_glBuffer);
    glBufferData(bufferType, size, data, usage);
    
    // Set native handle to the GL buffer ID (cast to void* for storage)
    m_nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(m_glBuffer));
}

OpenGLBuffer::~OpenGLBuffer() {
    if (m_glBuffer != 0) {
        glDeleteBuffers(1, &m_glBuffer);
        m_glBuffer = 0;
    }
}

// OpenGLTexture implementation
OpenGLTexture::OpenGLTexture(RenderBackend* backend, int width, int height, bool hasAlpha)
    : Texture(backend, width, height, hasAlpha), m_glTexture(0) {
    
    glGenTextures(1, &m_glTexture);
    glBindTexture(GL_TEXTURE_2D, m_glTexture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Initialize with empty data
    GLenum format = hasAlpha ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
    
    m_nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(m_glTexture));
}

OpenGLTexture::~OpenGLTexture() {
    if (m_glTexture != 0) {
        glDeleteTextures(1, &m_glTexture);
        m_glTexture = 0;
    }
}

void OpenGLTexture::update(const void* data) {
    glBindTexture(GL_TEXTURE_2D, m_glTexture);
    GLenum format = m_hasAlpha ? GL_RGBA : GL_RGB;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, format, GL_UNSIGNED_BYTE, data);
}

// OpenGLShader implementation
OpenGLShader::OpenGLShader(RenderBackend* backend, const std::string& vertexSource, const std::string& fragmentSource)
    : Shader(backend, vertexSource, fragmentSource), m_glProgram(0) {
    
    // Compile shader stages
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        std::cerr << "Failed to compile shaders" << std::endl;
        if (vertexShader != 0) glDeleteShader(vertexShader);
        if (fragmentShader != 0) glDeleteShader(fragmentShader);
        return;
    }
    
    // Create and link program
    m_glProgram = glCreateProgram();
    glAttachShader(m_glProgram, vertexShader);
    glAttachShader(m_glProgram, fragmentShader);
    glLinkProgram(m_glProgram);
    
    // Check for linking errors
    GLint success;
    char infoLog[512];
    glGetProgramiv(m_glProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_glProgram, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteProgram(m_glProgram);
        m_glProgram = 0;
    }
    
    // Clean up shader objects
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    m_nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(m_glProgram));
}

OpenGLShader::~OpenGLShader() {
    if (m_glProgram != 0) {
        glDeleteProgram(m_glProgram);
        m_glProgram = 0;
    }
}

GLuint OpenGLShader::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    // Check for compilation errors
    GLint success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLint OpenGLShader::getUniformLocation(const std::string& name) {
    // Check if location is already cached
    auto it = m_uniformLocations.find(name);
    if (it != m_uniformLocations.end()) {
        return it->second;
    }
    
    // Get new location
    GLint location = glGetUniformLocation(m_glProgram, name.c_str());
    if (location == -1) {
        std::cerr << "Warning: Uniform '" << name << "' not found in shader program" << std::endl;
    }
    
    // Cache for future use
    m_uniformLocations[name] = location;
    return location;
}

void OpenGLShader::setUniform(const std::string& name, float value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void OpenGLShader::setUniform(const std::string& name, int value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void OpenGLShader::setUniform(const std::string& name, const std::vector<float>& values) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1fv(location, values.size(), values.data());
    }
}

void OpenGLShader::setUniform(const std::string& name, float x, float y) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2f(location, x, y);
    }
}

void OpenGLShader::setUniform(const std::string& name, float x, float y, float z) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, x, y, z);
    }
}

void OpenGLShader::setUniform(const std::string& name, float x, float y, float z, float w) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, x, y, z, w);
    }
}

// OpenGLRenderTarget implementation
OpenGLRenderTarget::OpenGLRenderTarget(RenderBackend* backend, int width, int height, bool hasDepth, bool multisampled)
    : RenderTarget(backend, width, height, hasDepth, multisampled),
      m_glFramebuffer(0), m_glColorTexture(0), m_glDepthTexture(0),
      m_glColorRenderBuffer(0), m_glDepthRenderBuffer(0) {
    
    // Create framebuffer
    glGenFramebuffers(1, &m_glFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_glFramebuffer);
    
    if (multisampled) {
        // Multisampled render target
        // Create multisampled color attachment
        glGenRenderbuffers(1, &m_glColorRenderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_glColorRenderBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_glColorRenderBuffer);
        
        if (hasDepth) {
            // Create multisampled depth attachment
            glGenRenderbuffers(1, &m_glDepthRenderBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, m_glDepthRenderBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_glDepthRenderBuffer);
        }
    } else {
        // Regular (non-multisampled) render target with texture attachments
        // Create color texture attachment
        glGenTextures(1, &m_glColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_glColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_glColorTexture, 0);
        
        // Create color texture wrapper
        m_colorTexture = std::make_shared<OpenGLTexture>(backend, width, height, true);
        static_cast<OpenGLTexture*>(m_colorTexture.get())->m_glTexture = m_glColorTexture;
        
        if (hasDepth) {
            // Create depth texture attachment
            glGenTextures(1, &m_glDepthTexture);
            glBindTexture(GL_TEXTURE_2D, m_glDepthTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_glDepthTexture, 0);
            
            // Create depth texture wrapper
            m_depthTexture = std::make_shared<OpenGLTexture>(backend, width, height, false);
            static_cast<OpenGLTexture*>(m_depthTexture.get())->m_glTexture = m_glDepthTexture;
        }
    }
    
    // Check if framebuffer is complete
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete! Status: " << status << std::endl;
    }
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    m_nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(m_glFramebuffer));
}

OpenGLRenderTarget::~OpenGLRenderTarget() {
    if (m_glFramebuffer != 0) {
        glDeleteFramebuffers(1, &m_glFramebuffer);
    }
    
    if (m_glColorTexture != 0) {
        glDeleteTextures(1, &m_glColorTexture);
    }
    
    if (m_glDepthTexture != 0) {
        glDeleteTextures(1, &m_glDepthTexture);
    }
    
    if (m_glColorRenderBuffer != 0) {
        glDeleteRenderbuffers(1, &m_glColorRenderBuffer);
    }
    
    if (m_glDepthRenderBuffer != 0) {
        glDeleteRenderbuffers(1, &m_glDepthRenderBuffer);
    }
}

// OpenGLBackend implementation
OpenGLBackend::OpenGLBackend(int screenWidth, int screenHeight)
    : RenderBackend(screenWidth, screenHeight),
      m_defaultVAO(0), m_fullscreenQuadVBO(0), m_fullscreenQuadVAO(0) {
}

OpenGLBackend::~OpenGLBackend() {
    cleanup();
}

bool OpenGLBackend::initialize() {
    // Check GLEW initialization
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW initialization error: " << glewGetErrorString(err) << std::endl;
        return false;
    }
    
    // Set up a default VAO
    glGenVertexArrays(1, &m_defaultVAO);
    glBindVertexArray(m_defaultVAO);
    
    // Set up OpenGL state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize fullscreen quad for post-processing
    initializeFullscreenQuad();
    
    // Create render targets for shadow maps and main scene
    m_shadowMapTarget = createRenderTarget(m_screenWidth, m_screenHeight, true, false);
    m_mainRenderTarget = createRenderTarget(m_screenWidth, m_screenHeight, true, false);
    
    return true;
}

void OpenGLBackend::cleanup() {
    m_shadowMapTarget.reset();
    m_mainRenderTarget.reset();
    
    if (m_fullscreenQuadVBO != 0) {
        glDeleteBuffers(1, &m_fullscreenQuadVBO);
        m_fullscreenQuadVBO = 0;
    }
    
    if (m_fullscreenQuadVAO != 0) {
        glDeleteVertexArrays(1, &m_fullscreenQuadVAO);
        m_fullscreenQuadVAO = 0;
    }
    
    if (m_defaultVAO != 0) {
        glDeleteVertexArrays(1, &m_defaultVAO);
        m_defaultVAO = 0;
    }
}

void OpenGLBackend::beginFrame() {
    glBindVertexArray(m_defaultVAO);
}

void OpenGLBackend::endFrame() {
    // Nothing to do here for OpenGL
}

std::shared_ptr<Buffer> OpenGLBackend::createVertexBuffer(size_t size, const void* data) {
    return std::make_shared<OpenGLBuffer>(this, Buffer::Type::Vertex, size, data);
}

std::shared_ptr<Buffer> OpenGLBackend::createIndexBuffer(size_t size, const void* data) {
    return std::make_shared<OpenGLBuffer>(this, Buffer::Type::Index, size, data);
}

std::shared_ptr<Buffer> OpenGLBackend::createUniformBuffer(size_t size) {
    return std::make_shared<OpenGLBuffer>(this, Buffer::Type::Uniform, size, nullptr);
}

void OpenGLBackend::updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, size_t size) {
    if (!buffer) return;
    
    auto glBuffer = std::dynamic_pointer_cast<OpenGLBuffer>(buffer);
    if (!glBuffer) {
        std::cerr << "Invalid buffer type" << std::endl;
        return;
    }
    
    GLenum bufferType;
    switch (buffer->getType()) {
        case Buffer::Type::Vertex:
            bufferType = GL_ARRAY_BUFFER;
            break;
        case Buffer::Type::Index:
            bufferType = GL_ELEMENT_ARRAY_BUFFER;
            break;
        case Buffer::Type::Uniform:
            bufferType = GL_UNIFORM_BUFFER;
            break;
        default:
            std::cerr << "Unknown buffer type" << std::endl;
            return;
    }
    
    glBindBuffer(bufferType, glBuffer->getGLHandle());
    
    // If size is the same, use glBufferSubData for efficiency
    if (size <= buffer->getSize()) {
        glBufferSubData(bufferType, 0, size, data);
    } else {
        // Recreate the buffer if it's too small
        glBufferData(bufferType, size, data, bufferType == GL_UNIFORM_BUFFER ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
}

std::shared_ptr<Texture> OpenGLBackend::createTexture(int width, int height, bool hasAlpha) {
    return std::make_shared<OpenGLTexture>(this, width, height, hasAlpha);
}

void OpenGLBackend::updateTexture(std::shared_ptr<Texture> texture, const void* data) {
    if (!texture) return;
    
    auto glTexture = std::dynamic_pointer_cast<OpenGLTexture>(texture);
    if (!glTexture) {
        std::cerr << "Invalid texture type" << std::endl;
        return;
    }
    
    glTexture->update(data);
}

std::shared_ptr<Shader> OpenGLBackend::createShader(const std::string& vertexSource, const std::string& fragmentSource) {
    return std::make_shared<OpenGLShader>(this, vertexSource, fragmentSource);
}

void OpenGLBackend::bindShader(std::shared_ptr<Shader> shader) {
    m_currentShader = shader;
    
    if (!shader) {
        glUseProgram(0);
        return;
    }
    
    auto glShader = std::dynamic_pointer_cast<OpenGLShader>(shader);
    if (!glShader) {
        std::cerr << "Invalid shader type" << std::endl;
        return;
    }
    
    glUseProgram(glShader->getGLHandle());
}

std::shared_ptr<RenderTarget> OpenGLBackend::createRenderTarget(int width, int height, bool hasDepth, bool multisampled) {
    return std::make_shared<OpenGLRenderTarget>(this, width, height, hasDepth, multisampled);
}

void OpenGLBackend::bindRenderTarget(std::shared_ptr<RenderTarget> target) {
    if (!target) {
        bindDefaultRenderTarget();
        return;
    }
    
    auto glTarget = std::dynamic_pointer_cast<OpenGLRenderTarget>(target);
    if (!glTarget) {
        std::cerr << "Invalid render target type" << std::endl;
        return;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, glTarget->getGLHandle());
    setViewport(0, 0, target->getWidth(), target->getHeight());
}

void OpenGLBackend::bindDefaultRenderTarget() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    setViewport(0, 0, m_screenWidth, m_screenHeight);
}

void OpenGLBackend::drawMesh(std::shared_ptr<Buffer> vertexBuffer, size_t vertexCount, 
                             std::shared_ptr<Buffer> indexBuffer, size_t indexCount) {
    if (!vertexBuffer) return;
    
    auto glVertexBuffer = std::dynamic_pointer_cast<OpenGLBuffer>(vertexBuffer);
    if (!glVertexBuffer) {
        std::cerr << "Invalid vertex buffer type" << std::endl;
        return;
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, glVertexBuffer->getGLHandle());
    
    // Set up vertex attributes - assuming a specific layout here
    // In a real implementation, this would be more flexible
    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1); // Texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    
    if (indexBuffer) {
        // Indexed rendering
        auto glIndexBuffer = std::dynamic_pointer_cast<OpenGLBuffer>(indexBuffer);
        if (!glIndexBuffer) {
            std::cerr << "Invalid index buffer type" << std::endl;
            return;
        }
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glIndexBuffer->getGLHandle());
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    } else {
        // Non-indexed rendering
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
    
    // Cleanup
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

void OpenGLBackend::drawFullscreenQuad() {
    glBindVertexArray(m_fullscreenQuadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(m_defaultVAO);
}

void OpenGLBackend::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void OpenGLBackend::setClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void OpenGLBackend::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLBackend::beginShadowPass() {
    bindRenderTarget(m_shadowMapTarget);
    clear();
}

void OpenGLBackend::beginMainPass() {
    bindRenderTarget(m_mainRenderTarget);
    clear();
}

void OpenGLBackend::beginPostProcessPass() {
    bindDefaultRenderTarget();
    clear();
}

void* OpenGLBackend::getNativeHandle() {
    // OpenGL doesn't have a single "context" handle to return
    // In a real implementation, this might return SDL_GLContext or similar
    return nullptr;
}

bool OpenGLBackend::supportsFeature(const std::string& featureName) const {
    // Check for supported features
    if (featureName == "shadows") return true;
    if (featureName == "bloom") return true;
    if (featureName == "volumetricLighting") return true;
    if (featureName == "postProcessing") return true;
    
    return false;
}

std::string OpenGLBackend::getRendererInfo() const {
    std::string info;
    
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    info = "OpenGL Renderer\n";
    info += "Vendor: " + std::string(vendor ? vendor : "Unknown") + "\n";
    info += "Renderer: " + std::string(renderer ? renderer : "Unknown") + "\n";
    info += "Version: " + std::string(version ? version : "Unknown") + "\n";
    info += "GLSL Version: " + std::string(glslVersion ? glslVersion : "Unknown");
    
    return info;
}

void OpenGLBackend::initializeFullscreenQuad() {
    // Create a VAO for the fullscreen quad
    glGenVertexArrays(1, &m_fullscreenQuadVAO);
    glBindVertexArray(m_fullscreenQuadVAO);
    
    // Create a VBO for the quad
    glGenBuffers(1, &m_fullscreenQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenQuadVBO);
    
    // Define quad vertices (position + texcoord)
    float vertices[] = {
        // positions        // texture coords
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, // top left
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f  // top right
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    
    // Texture coord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Reset to default VAO
    glBindVertexArray(m_defaultVAO);
}

// Factory function implementation
std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int screenWidth, int screenHeight) {
    switch (type) {
        case BackendType::OpenGL:
            return std::make_unique<OpenGLBackend>(screenWidth, screenHeight);
        case BackendType::Vulkan:
            #ifdef USE_VULKAN
            return std::make_unique<VulkanBackend>(screenWidth, screenHeight);
            #else
            std::cerr << "Vulkan backend requires USE_VULKAN to be defined during compilation" << std::endl;
            return nullptr;
            #endif
        case BackendType::DirectX12:
            // DirectX12 backend not implemented yet
            std::cerr << "DirectX12 backend not implemented yet" << std::endl;
            return nullptr;
        default:
            std::cerr << "Unknown backend type" << std::endl;
            return nullptr;
    }
}

} // namespace PixelPhys