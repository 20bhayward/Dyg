#include "../include/Renderer.h"
#include "../include/RenderBackend.h"
#include "../include/RenderResources.h"
#ifdef USE_VULKAN
#include "../include/VulkanBackend.h"
#else
#include "../include/OpenGLBackend.h"
#endif
#ifdef _WIN32
#include "../include/DirectX12Backend.h"
#endif
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

// Fallback to_string implementation for MSVC compatibility issues
#if defined(_MSC_VER)
namespace std {
    #ifndef _TO_STRING_FALLBACK
    #define _TO_STRING_FALLBACK
    template <typename T>
    std::string to_string(const T& value) {
        std::ostringstream ss;
        ss << value;
        return ss.str();
    }
    #endif
}
#endif

namespace PixelPhys {

Renderer::Renderer(int screenWidth, int screenHeight, BackendType type)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight) {
    
    std::cout << "Creating renderer with screen size: " << screenWidth << "x" << screenHeight << std::endl;
    
    // Set the specified backend type or auto-detect
    BackendType backendType = type;
    if (backendType == BackendType::Auto) {
        backendType = detectBestBackendType();
    }
    setBackendType(backendType);
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initialize() {
    if (!m_backend) {
        std::cerr << "No rendering backend available" << std::endl;
        return false;
    }
    
    // Initialize the backend
    if (!m_backend->initialize()) {
        std::cerr << "Failed to initialize rendering backend" << std::endl;
        return false;
    }
    
    // Create rendering resources (shaders, render targets)
    if (!createRenderingResources()) {
        std::cerr << "Failed to create rendering resources" << std::endl;
        return false;
    }
    
    std::cout << "Renderer initialized successfully using " 
              << m_backend->getRendererInfo() << std::endl;
    return true;
}

bool Renderer::initialize(SDL_Window* window) {
    // This method is primarily for Vulkan which needs the window handle
    if (!m_backend) {
        std::cerr << "No rendering backend available" << std::endl;
        return false;
    }
    
    // For OpenGL we just call the regular initialize
    if (m_backend->getType() != BackendType::Vulkan) {
        return initialize();
    }
    
    // For Vulkan, we would need to pass the window to the backend
    // For now, just call initialize() since our VulkanBackend doesn't take a window yet
    return initialize();
}

void Renderer::beginFrame() {
    if (m_backend) {
        m_backend->beginFrame();
    }
}

void Renderer::endFrame() {
    if (m_backend) {
        m_backend->endFrame();
    }
}

std::shared_ptr<Buffer> Renderer::createVertexBuffer(size_t size, const void* data) {
    if (m_backend) {
        return m_backend->createVertexBuffer(size, data);
    }
    return nullptr;
}

std::shared_ptr<Buffer> Renderer::createIndexBuffer(size_t size, const void* data) {
    if (m_backend) {
        return m_backend->createIndexBuffer(size, data);
    }
    return nullptr;
}

std::shared_ptr<Shader> Renderer::createShader(const std::string& vertexSource, const std::string& fragmentSource) {
    if (m_backend) {
        return m_backend->createShader(vertexSource, fragmentSource);
    }
    return nullptr;
}

void Renderer::renderShadowPass(const World& world) {
    if (!m_backend || !m_shadowShader || !m_shadowMapTarget) {
        return;
    }
    
    m_backend->beginShadowPass();
    m_backend->bindRenderTarget(m_shadowMapTarget);
    m_backend->bindShader(m_shadowShader);
    
    // Set shadow shader uniforms
    if (m_worldTexture) {
        m_shadowShader->setUniform("worldTexture", 0);  // texture unit 0
        m_shadowShader->setUniform("emissiveTexture", 1);  // texture unit 1
    }
    
    // Set player position for shadow calculation
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    m_shadowShader->setUniform("playerPos", playerX, playerY);
    
    // Set world size for coordinate conversion
    m_shadowShader->setUniform("worldSize", 
                              static_cast<float>(world.getWidth()), 
                              static_cast<float>(world.getHeight()));
                
    // Set game time for time-based effects
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    m_shadowShader->setUniform("gameTime", gameTime);
    
    // Draw fullscreen quad
    m_backend->drawFullscreenQuad();
}

void Renderer::renderBloomPass() {
    if (!m_backend || !m_bloomShader || !m_bloomTarget) {
        return;
    }
    
    m_backend->bindRenderTarget(m_bloomTarget);
    m_backend->bindShader(m_bloomShader);
    
    // Set bloom shader uniforms
    if (m_mainRenderTarget) {
        m_bloomShader->setUniform("emissiveTexture", 0);  // texture unit 0
    }
    
    // Set game time for animated effects
    static float gameTime = 0.0f;
    gameTime += 0.016f;
    m_bloomShader->setUniform("gameTime", gameTime);
    
    // Draw fullscreen quad
    m_backend->drawFullscreenQuad();
}

void Renderer::renderVolumetricLightingPass(const World& world) {
    if (!m_backend || !m_volumetricLightShader || !m_volumetricLightTarget) {
        return;
    }
    
    m_backend->bindRenderTarget(m_volumetricLightTarget);
    m_backend->bindShader(m_volumetricLightShader);
    
    // Set shader uniforms for textures
    if (m_mainRenderTarget) {
        m_volumetricLightShader->setUniform("colorTexture", 0);  // texture unit 0
        m_volumetricLightShader->setUniform("emissiveTexture", 1);  // texture unit 1
        m_volumetricLightShader->setUniform("depthTexture", 2);  // texture unit 2
    }
    
    if (m_shadowMapTarget) {
        m_volumetricLightShader->setUniform("shadowTexture", 3);  // texture unit 3
    }
    
    // Set world size
    m_volumetricLightShader->setUniform("worldSize", 
                                        static_cast<float>(world.getWidth()), 
                                        static_cast<float>(world.getHeight()));
    
    // Set light positions - player torch and sun
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    m_volumetricLightShader->setUniform("playerPos", playerX, playerY);
    
    // Set game time for day/night cycle
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    m_volumetricLightShader->setUniform("gameTime", gameTime);
    
    // Day/night cycle calculation
    float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // 0=night, 1=day
    float sunHeight = 0.15 + 0.35 * dayPhase;
    
    // Sun position
    float sunX = world.getWidth() * 0.5f;
    float sunY = world.getHeight() * sunHeight;
    m_volumetricLightShader->setUniform("sunPos", sunX, sunY);
    
    // Global illumination strength
    m_volumetricLightShader->setUniform("giStrength", 0.7f);
    
    // Set light colors
    m_volumetricLightShader->setUniform("playerLightColor", 1.0f, 0.8f, 0.5f); // Warm torch light
    m_volumetricLightShader->setUniform("sunlightColor", 1.0f, 0.9f, 0.7f);  // Warm sunlight
    
    // Draw fullscreen quad
    m_backend->drawFullscreenQuad();
}

void Renderer::renderPostProcessPass() {
    if (!m_backend || !m_postProcessShader) {
        return;
    }
    
    m_backend->beginPostProcessPass();
    m_backend->bindDefaultRenderTarget();
    m_backend->bindShader(m_postProcessShader);
    
    // Set texture uniforms
    if (m_mainRenderTarget) {
        m_postProcessShader->setUniform("colorTexture", 0);  // texture unit 0
    }
    
    if (m_bloomTarget) {
        m_postProcessShader->setUniform("bloomTexture", 1);  // texture unit 1
    }
    
    if (m_shadowMapTarget) {
        m_postProcessShader->setUniform("shadowMapTexture", 2);  // texture unit 2
    }
    
    if (m_volumetricLightTarget) {
        m_postProcessShader->setUniform("volumetricLightTexture", 3);  // texture unit 3
    }
    
    // Set game time for time-based effects
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    m_postProcessShader->setUniform("gameTime", gameTime);
    
    // Draw fullscreen quad
    m_backend->drawFullscreenQuad();
}

void Renderer::renderMainPass(const World& world) {
    if (!m_backend || !m_mainShader || !m_mainRenderTarget) {
        return;
    }
    
    m_backend->beginMainPass();
    m_backend->bindRenderTarget(m_mainRenderTarget);
    m_backend->bindShader(m_mainShader);
    
    // Set main shader uniforms
    if (m_worldTexture) {
        m_mainShader->setUniform("worldTexture", 0);  // texture unit 0
    }
    
    // Set world size
    m_mainShader->setUniform("worldSize", 
                            static_cast<float>(world.getWidth()), 
                            static_cast<float>(world.getHeight()));
    
    // Set player position
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    m_mainShader->setUniform("playerPos", playerX, playerY);
    
    // Set game time
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    m_mainShader->setUniform("gameTime", gameTime);
    
    // Set material properties for all material types
    for (int i = 0; i < static_cast<int>(MaterialType::COUNT); i++) {
        const auto& props = MATERIAL_PROPERTIES[i];
        
        // Visual properties
        std::string prefix = "materials[" + std::to_string(i) + "].";
        m_mainShader->setUniform(prefix + "isEmissive", props.isEmissive ? 1 : 0);
        m_mainShader->setUniform(prefix + "isRefractive", props.isRefractive ? 1 : 0);
        m_mainShader->setUniform(prefix + "isTranslucent", props.isTranslucent ? 1 : 0);
        m_mainShader->setUniform(prefix + "isReflective", props.isReflective ? 1 : 0);
        m_mainShader->setUniform(prefix + "emissiveStrength", static_cast<float>(props.emissiveStrength) / 255.0f);
        
        // Shader effects
        m_mainShader->setUniform(prefix + "hasWaveEffect", props.hasWaveEffect ? 1 : 0);
        m_mainShader->setUniform(prefix + "hasGlowEffect", props.hasGlowEffect ? 1 : 0);
        m_mainShader->setUniform(prefix + "hasParticleEffect", props.hasParticleEffect ? 1 : 0);
        m_mainShader->setUniform(prefix + "hasShimmerEffect", props.hasShimmerEffect ? 1 : 0);
        m_mainShader->setUniform(prefix + "waveSpeed", static_cast<float>(props.waveSpeed) / 255.0f);
        m_mainShader->setUniform(prefix + "waveHeight", static_cast<float>(props.waveHeight) / 255.0f);
        m_mainShader->setUniform(prefix + "glowSpeed", static_cast<float>(props.glowSpeed) / 255.0f);
        m_mainShader->setUniform(prefix + "refractiveIndex", static_cast<float>(props.refractiveIndex) / 255.0f);
        
        // Secondary color
        m_mainShader->setUniform(prefix + "secondaryColor", 
                                static_cast<float>(props.secondaryR) / 255.0f,
                                static_cast<float>(props.secondaryG) / 255.0f,
                                static_cast<float>(props.secondaryB) / 255.0f);
    }
    
    // Draw fullscreen quad
    m_backend->drawFullscreenQuad();
}

void Renderer::render(const World& world) {
    if (!m_backend) {
        return;
    }
    
    // Begin frame
    m_backend->beginFrame();
    
    // Update world texture
    updateWorldTexture(world);
    
    // For Vulkan backend, render the world with material-based shading
    if (m_backend->getType() == BackendType::Vulkan) {
        m_backend->setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        m_backend->clear();
        
        // Bind default render target
        m_backend->bindDefaultRenderTarget();
        
        // Bind material shader for enhanced rendering
        if (m_mainShader) {
            m_backend->bindShader(m_mainShader);
            
            // Get screen dimensions
            int screenWidth = m_backend->getScreenWidth();
            int screenHeight = m_backend->getScreenHeight();
            
            // Get camera information
            int cameraX = static_cast<int>(world.getPlayerX()) - screenWidth / 2;
            int cameraY = static_cast<int>(world.getPlayerY()) - screenHeight / 2;
            
            // Apply custom material-based rendering
            int pixelSize = 1; // Size of each world pixel in screen pixels
            int worldWidth = world.getWidth();
            int worldHeight = world.getHeight();
            
            // Loop through the visible portion of the world and render each pixel with material properties
            int viewportStartX = std::max(0, cameraX);
            int viewportStartY = std::max(0, cameraY);
            int viewportEndX = std::min(worldWidth, cameraX + screenWidth);
            int viewportEndY = std::min(worldHeight, cameraY + screenHeight);
            
            VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend.get());
            
            for (int y = viewportStartY; y < viewportEndY; y++) {
                for (int x = viewportStartX; x < viewportEndX; x++) {
                    // Get material at this world position
                    MaterialType material = world.getMaterialAt(x, y);
                    
                    // Skip empty space
                    if (material == MaterialType::Empty) {
                        continue;
                    }
                    
                    // Convert world coordinates to screen coordinates
                    float screenX = static_cast<float>(x - cameraX);
                    float screenY = static_cast<float>(y - cameraY);
                    
                    // DIRECT COLOR APPROACH: Convert material directly to a fixed color
                    // For specific materials, use custom colors to ensure they're visible
                    float r, g, b;
                    
                    switch (material) {
                        case MaterialType::Fire: 
                            // Bright orange-red for fire
                            r = 1.0f; g = 0.4f; b = 0.0f;
                            break;
                            
                        case MaterialType::Water:
                            // Vibrant blue for water  
                            r = 0.0f; g = 0.4f; b = 0.9f;
                            break;
                            
                        case MaterialType::Lava:
                            // Bright orange for lava
                            r = 1.0f; g = 0.3f; b = 0.0f;
                            break;
                        
                        case MaterialType::Sand:
                            // Golden sand
                            r = 0.95f; g = 0.8f; b = 0.2f;
                            break;
                            
                        case MaterialType::Stone:
                            // Medium gray for stone
                            r = 0.6f; g = 0.6f; b = 0.6f;
                            break;
                            
                        case MaterialType::Wood:
                            // Brown for wood
                            r = 0.6f; g = 0.4f; b = 0.2f;
                            break;
                            
                        case MaterialType::Dirt:
                            // Brown for dirt
                            r = 0.5f; g = 0.3f; b = 0.1f;
                            break;
                            
                        case MaterialType::Glass:
                            // Light blue tint for glass
                            r = 0.8f; g = 0.9f; b = 1.0f;
                            break;
                            
                        case MaterialType::Grass:
                            // Green for grass
                            r = 0.2f; g = 0.8f; b = 0.2f;
                            break;
                            
                        case MaterialType::Coal:
                            // Dark gray/black for coal
                            r = 0.15f; g = 0.15f; b = 0.15f;
                            break;
                            
                        default: {
                            // For all other materials, get the RGBA color
                            const auto& props = MATERIAL_PROPERTIES[static_cast<size_t>(material)];
                            r = props.r / 255.0f;
                            g = props.g / 255.0f;
                            b = props.b / 255.0f;
                        }
                    }
                    
                    // Bypass the material shader system entirely, just draw a colored rectangle
                    vulkanBackend->drawRectangle(screenX, screenY, pixelSize, pixelSize, r, g, b);
                }
            }
            
            // Render player
            world.renderPlayer(1.0f);
        }
        else {
            // Fallback to fullscreen texture rendering if material shader not available
            m_backend->drawFullscreenQuad();
        }
    }
    else {
        // Full rendering pipeline for OpenGL
        
        // Pass 1: Render main scene with material properties
        renderMainPass(world);
        
        // Pass 2: Generate shadow map
        renderShadowPass(world);
        
        // Pass 3: Volumetric lighting and global illumination
        renderVolumetricLightingPass(world);
        
        // Pass 4: Generate bloom effect from emissive materials
        renderBloomPass();
        
        // Pass 5: Final composition with post-processing
        renderPostProcessPass();
        
        // Render player on top of everything
        world.renderPlayer(1.0f);
    }
    
    // End frame
    m_backend->endFrame();
}

void Renderer::cleanup() {
    // Clean up shaders
    m_mainShader.reset();
    m_shadowShader.reset();
    m_volumetricLightShader.reset();
    m_bloomShader.reset();
    m_postProcessShader.reset();
    
    // Clean up render targets
    m_mainRenderTarget.reset();
    m_shadowMapTarget.reset();
    m_volumetricLightTarget.reset();
    m_bloomTarget.reset();
    
    // Clean up textures
    m_worldTexture.reset();
    
    // Clean up the rendering backend
    if (m_backend) {
        m_backend->cleanup();
        m_backend.reset();
    }
}

bool Renderer::setBackendType(BackendType type) {
    std::cout << "Setting backend type to: ";
    switch (type) {
        case BackendType::OpenGL: std::cout << "OpenGL"; break;
        case BackendType::Vulkan: std::cout << "Vulkan"; break;
        case BackendType::DirectX12: std::cout << "DirectX12"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << std::endl;
    
    // Clean up existing backend if any
    if (m_backend) {
        m_backend->cleanup();
        m_backend.reset();
    }
    
    // Create a new backend of the specified type
    m_backend = CreateRenderBackend(type, m_screenWidth, m_screenHeight);
    
    if (!m_backend) {
        std::cerr << "Failed to create rendering backend of type " << static_cast<int>(type) << std::endl;
        return false;
    }
    
    std::cout << "Successfully created " << m_backend->getRendererInfo() << " backend" << std::endl;
    return true;
}

BackendType Renderer::getBackendType() const {
    if (m_backend) {
        return m_backend->getType();
    }
    return BackendType::None;
}

std::string Renderer::getRendererInfo() const {
    if (m_backend) {
        return m_backend->getRendererInfo();
    }
    return "No renderer available";
}

BackendType Renderer::detectBestBackendType() {
    // Use compile-time flags to determine the backend
#ifdef USE_VULKAN
    return BackendType::Vulkan;
#elif defined(_WIN32)
    // On Windows, try DirectX 12 first, then OpenGL
    auto dx12Backend = CreateRenderBackend(BackendType::DirectX12, m_screenWidth, m_screenHeight);
    if (dx12Backend) {
        dx12Backend.reset();
        return BackendType::DirectX12;
    }
    return BackendType::OpenGL;
#else
    // Linux/macOS default to OpenGL
    return BackendType::OpenGL;
#endif
}

void Renderer::updateWorldTexture(const World& world) {
    if (!m_backend) {
        return;
    }
    
    // Create texture if it doesn't exist
    if (!m_worldTexture) {
        m_worldTexture = m_backend->createTexture(world.getWidth(), world.getHeight(), true);
        if (!m_worldTexture) {
            std::cerr << "Failed to create world texture" << std::endl;
            return;
        }
        std::cout << "Created world texture: " << world.getWidth() << "x" << world.getHeight() << std::endl;
    }
    
    // Validate pixel data
    const uint8_t* pixelData = world.getPixelData();
    if (!pixelData) {
        std::cerr << "Error: World returned null pixel data" << std::endl;
        return;
    }
    
    // Debug - check first few pixels
    std::cout << "First pixels RGBA values: ";
    for (int i = 0; i < 4; i++) {
        std::cout << "(" << (int)pixelData[i*4] << "," << (int)pixelData[i*4+1] << "," 
                 << (int)pixelData[i*4+2] << "," << (int)pixelData[i*4+3] << ") ";
    }
    std::cout << std::endl;
    
    // Update texture with world data
    m_backend->updateTexture(m_worldTexture, pixelData);
}

bool Renderer::createRenderingResources() {
    if (!m_backend) {
        return false;
    }
    
    // Create world texture for pixel rendering
    m_worldTexture = m_backend->createTexture(2048, 2048, true);
    if (!m_worldTexture) {
        std::cerr << "Failed to create world texture" << std::endl;
        return false;
    }
    
    // Create main render target
    m_mainRenderTarget = m_backend->createRenderTarget(m_screenWidth, m_screenHeight, true, false);
    if (!m_mainRenderTarget) {
        std::cerr << "Failed to create main render target" << std::endl;
        return false;
    }
    
    // Create shadow map render target
    m_shadowMapTarget = m_backend->createRenderTarget(m_screenWidth, m_screenHeight, false, false);
    if (!m_shadowMapTarget) {
        std::cerr << "Failed to create shadow map render target" << std::endl;
        return false;
    }
    
    // Create volumetric lighting render target
    m_volumetricLightTarget = m_backend->createRenderTarget(m_screenWidth, m_screenHeight, false, false);
    if (!m_volumetricLightTarget) {
        std::cerr << "Failed to create volumetric lighting render target" << std::endl;
        return false;
    }
    
    // Create bloom render target
    m_bloomTarget = m_backend->createRenderTarget(m_screenWidth, m_screenHeight, false, false);
    if (!m_bloomTarget) {
        std::cerr << "Failed to create bloom render target" << std::endl;
        return false;
    }
    
    // Create shaders
    m_mainShader = m_backend->createShader(getMainVertexShaderSource(), getMainFragmentShaderSource());
    if (!m_mainShader) {
        std::cerr << "Failed to create main shader" << std::endl;
        return false;
    }
    
    m_shadowShader = m_backend->createShader(getShadowVertexShaderSource(), getShadowFragmentShaderSource());
    if (!m_shadowShader) {
        std::cerr << "Failed to create shadow shader" << std::endl;
        return false;
    }
    
    m_volumetricLightShader = m_backend->createShader(
        getVolumetricLightVertexShaderSource(), 
        getVolumetricLightFragmentShaderSource());
    if (!m_volumetricLightShader) {
        std::cerr << "Failed to create volumetric lighting shader" << std::endl;
        return false;
    }
    
    m_bloomShader = m_backend->createShader(getBloomVertexShaderSource(), getBloomFragmentShaderSource());
    if (!m_bloomShader) {
        std::cerr << "Failed to create bloom shader" << std::endl;
        return false;
    }
    
    m_postProcessShader = m_backend->createShader(
        getPostProcessVertexShaderSource(), 
        getPostProcessFragmentShaderSource());
    if (!m_postProcessShader) {
        std::cerr << "Failed to create post-process shader" << std::endl;
        return false;
    }
    
    return true;
}

// Common vertex shader source for all rendering passes
std::string Renderer::getMainVertexShaderSource() const {
    return R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main()
        {
            gl_Position = vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        }
    )";
}

// Common vertex shader source for all other passes
std::string Renderer::getShadowVertexShaderSource() const {
    return getMainVertexShaderSource();
}

std::string Renderer::getVolumetricLightVertexShaderSource() const {
    return getMainVertexShaderSource();
}

std::string Renderer::getBloomVertexShaderSource() const {
    return getMainVertexShaderSource();
}

std::string Renderer::getPostProcessVertexShaderSource() const {
    return getMainVertexShaderSource();
}

// Fragment shader sources
std::string Renderer::getMainFragmentShaderSource() const {
    return R"(
        #version 330 core
        layout (location = 0) out vec4 FragColor;
        layout (location = 1) out vec4 EmissiveColor;
        in vec2 TexCoord;
        uniform sampler2D worldTexture;
        uniform vec2 worldSize;
        uniform vec2 playerPos;
        uniform float gameTime;
        
        // Material properties buffer with enhanced visual effects
        struct MaterialInfo {
           // Visual properties
           bool isEmissive;
           bool isRefractive;
           bool isTranslucent;
           bool isReflective;
           float emissiveStrength;
           
           // Special shader effects
           bool hasWaveEffect;
           bool hasGlowEffect;
           bool hasParticleEffect;
           bool hasShimmerEffect;
           float waveSpeed;
           float waveHeight;
           float glowSpeed;
           float refractiveIndex;
           
           // Secondary color for effects
           vec3 secondaryColor;
        };
        uniform MaterialInfo materials[30]; // Max 30 material types
        
        // Basic hash function for random values
        float hash(vec2 p) {
           return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
        }
        
        // ... rest of the main shader ...
    )";
}

std::string Renderer::getShadowFragmentShaderSource() const {
    return R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D worldTexture;
        uniform sampler2D emissiveTexture;
        uniform vec2 playerPos;
        uniform vec2 worldSize;
        uniform float gameTime;
        
        // Helper functions for noise and variety
        float hash(vec2 p) {
           return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
        }
        
        // ... rest of the shadow shader ...
    )";
}

std::string Renderer::getVolumetricLightFragmentShaderSource() const {
    return R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D colorTexture;
        uniform sampler2D emissiveTexture;
        uniform sampler2D depthTexture;
        uniform sampler2D shadowTexture;
        uniform vec2 worldSize;
        uniform vec2 playerPos;
        uniform vec2 sunPos;
        uniform float gameTime;
        uniform float giStrength;
        uniform vec3 playerLightColor;
        uniform vec3 sunlightColor;
        
        // ... rest of the volumetric lighting shader ...
    )";
}

std::string Renderer::getBloomFragmentShaderSource() const {
    return R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D emissiveTexture;
        uniform float gameTime;
        
        // ... rest of the bloom shader ...
    )";
}

std::string Renderer::getPostProcessFragmentShaderSource() const {
    return R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D colorTexture;
        uniform sampler2D bloomTexture;
        uniform sampler2D shadowMapTexture;
        uniform sampler2D volumetricLightTexture;
        uniform float gameTime;
        
        // ... rest of the post-processing shader ...
    )";
}

#ifndef USE_VULKAN
// Shader fragments for OpenGL implementation
const char* shaderHelper = R"(
// Helper functions for shaders
)";
#endif

#ifndef USE_VULKAN
bool PixelPhys::Renderer::createShadersAndPrograms() {
    // Common vertex shader for all post-processing effects
    const char* commonVertexShader = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main() {\n"
        "   gl_Position = vec4(aPos, 1.0);\n"
        "   TexCoord = aTexCoord;\n"
        "}\n";
        
    // Create shader programs
    m_shaderProgram = createShaderProgram(commonVertexShader, getMainFragmentShaderSource().c_str());
    m_shadowShader = createShaderProgram(commonVertexShader, getShadowFragmentShaderSource().c_str());
    m_volumetricLightShader = createShaderProgram(commonVertexShader, getVolumetricLightFragmentShaderSource().c_str());
    m_bloomShader = createShaderProgram(commonVertexShader, getBloomFragmentShaderSource().c_str());
    m_postProcessShader = createShaderProgram(commonVertexShader, getPostProcessFragmentShaderSource().c_str());
    
    // Check if all shaders compiled successfully
    return (m_shaderProgram != 0 && m_shadowShader != 0 && m_volumetricLightShader != 0 &&
            m_bloomShader != 0 && m_postProcessShader != 0);
}
#endif

#ifndef USE_VULKAN
// Fragment shader code snippets
const char* noiseShaderEnd = R"(
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Improved Perlin-style noise function
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }
)";
#endif
#ifndef USE_VULKAN
const char* snoiseShaderPart1 = R"(
float snoise(vec2 v) {
   const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439);
   vec2 i  = floor(v + dot(v, C.yy));
   vec2 x0 = v -   i + dot(i, C.xx);
)";
#endif
#ifndef USE_VULKAN
const char* snoiseShaderPart2 = R"(
   vec2 i1;
   i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
   vec4 x12 = x0.xyxy + C.xxzz;
   x12.xy -= i1;
   i = mod289(i);
)";
#endif
#ifndef USE_VULKAN
const char* snoiseShaderPart3 = R"(
   vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));
   vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
   m = m*m;
   m = m*m;
   vec3 x = 2.0 * fract(p * C.www) - 1.0;
)";
#endif
#ifndef USE_VULKAN
const char* snoiseShaderPart4 = R"(
   vec3 h = abs(x) - 0.5;
   vec3 ox = floor(x + 0.5);
   vec3 a0 = x - ox;
   m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);
   vec3 g;
)";
#endif
#ifndef USE_VULKAN
const char* snoiseShaderPart5 = R"(
   g.x  = a0.x  * x0.x  + h.x  * x0.y;
   g.yz = a0.yz * x12.xz + h.yz * x12.yw;
   return 130.0 * dot(m, g);
}
)";
#endif
#ifndef USE_VULKAN
const char* fbmShaderPart1 = R"(
float fbm(vec2 uv) {
   float value = 0.0;
   float amplitude = 0.5;
   float frequency = 0.0;
   for (int i = 0; i < 6; ++i) {
)";
#endif
#ifndef USE_VULKAN
const char* fbmShaderPart2 = R"(
       value += amplitude * snoise(uv * frequency);
       frequency *= 2.0;
       amplitude *= 0.5;
   }
   return value;
)";
#endif
#ifndef USE_VULKAN
const char* fbmShaderEnd = R"(
}

float random(vec2 st) {
   return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}
)";
#endif
#ifndef USE_VULKAN
const char* godRaysShaderPart1 = R"(
// Function to calculate volumetric light/god rays
float godRays(vec2 uv, vec2 lightPos, float density, float decay, float weight, float exposure) {
   vec2 deltaTextCoord = (uv - lightPos);
   deltaTextCoord *= 1.0 / float(90) * density;
)";
#endif
#ifndef USE_VULKAN
const char* godRaysShaderPart2 = R"(
   vec2 textCoord = uv;
   float illuminationDecay = 1.0;
   float color = 0.0;
   for(int i=0; i < 90; i++) {
       textCoord -= deltaTextCoord;
)";
#endif
#ifndef USE_VULKAN
const char* godRaysShaderPart3 = R"(
       float sampleColor = 0.0;
       // Sample depth-based occlusion
       if(textCoord.y > 0.5) {
           sampleColor = 0.0; // Underground blocks rays
       } else {
)";
#endif
#ifndef USE_VULKAN
const char* godRaysShaderPart4 = R"(
           sampleColor = 1.0 - textCoord.y * 2.0; // Fade with depth
       }
       sampleColor *= illuminationDecay * weight;
       color += sampleColor;
       illuminationDecay *= decay;
)";
#endif
#ifndef USE_VULKAN
const char* godRaysShaderEnd = R"(
   }
   return color * exposure;
}

// Lens flare effect function
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderPart1 = R"(
float lensFlare(vec2 uv, vec2 sunPos) {
   vec2 uvd = uv * length(uv);
   float sunDist = distance(sunPos, uv);
   
   // Base flare
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderPart2 = R"(
   float flareBase = max(0.0, 1.0 - sunDist * 8.0);
   
   // Create multiple flare artifacts
   float f1 = max(0.0, 1.0 - length(uv - sunPos * 0.25) * 4.0);
   float f2 = max(0.0, 1.0 - length(uv - sunPos * 0.5) * 5.0);
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderPart3 = R"(
   float f3 = max(0.0, 1.0 - length(uv - sunPos * 0.8) * 6.0);
   float f4 = max(0.0, 1.0 - length(uv - sunPos * 1.3) * 3.0);
   float f5 = max(0.0, 1.0 - length(uv - sunPos * 1.6) * 9.0);
   
   // Rainbow hue artifacts
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderPart4 = R"(
   float haloEffect = 0.0;
   for(float i=0.0; i<8.0; i++) {
       float haloSize = 0.1 + i*0.1;
       float haloWidth = 0.02;
       float halo = smoothstep(haloSize-haloWidth, haloSize, sunDist) - smoothstep(haloSize, haloSize+haloWidth, sunDist);
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderPart5 = R"(
       haloEffect += halo * (0.5 - abs(0.5-i/8.0));
   }
   
   // Combine effects
   return flareBase * 0.5 + haloEffect * 0.2 + f1 * 0.6 + f2 * 0.35 + f3 * 0.2 + f4 * 0.3 + f5 * 0.2;
)";
#endif
#ifndef USE_VULKAN
const char* lensFlareShaderEnd = R"(
}

vec3 createSky(vec2 uv) {
   // Day/night cycle based on gameTime
   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // 0=night, 1=day
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart1 = R"(
   
   // Calculate sun position
   float sunHeight = 0.15 + 0.35 * dayPhase; // Sun moves from low to high
   vec2 sunPos = vec2(0.5, sunHeight);
   float sunDist = length(uv - sunPos);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart2 = R"(
   
   // Calculate moon position (opposite of sun)
   vec2 moonPos = vec2(0.5, 1.1 - sunHeight);
   float moonDist = length(uv - moonPos);
   
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart3 = R"(
   // Sky gradient - changes based on time of day
   vec3 zenithColor = mix(vec3(0.03, 0.05, 0.2), vec3(0.1, 0.5, 0.9), dayPhase); // Night to day zenith
   vec3 horizonColor = mix(vec3(0.1, 0.12, 0.25), vec3(0.7, 0.75, 0.85), dayPhase); // Night to day horizon
   float gradientFactor = pow(1.0 - uv.y, 2.0); // Exponential gradient
   vec3 skyColor = mix(zenithColor, horizonColor, gradientFactor);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart4 = R"(
   
   // Add atmospheric scattering glow at horizon
   float horizonGlow = pow(gradientFactor, 4.0) * dayPhase;
   vec3 horizonScatter = vec3(0.9, 0.6, 0.35) * horizonGlow;
   skyColor += horizonScatter;
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart5 = R"(
   
   // Add sun with realistic glow
   float sunSize = 0.03;
   float sunSharpness = 50.0;
   float sunGlow = 0.2;
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart6 = R"(
   float sunIntensity = smoothstep(0.0, sunSize, sunDist);
   sunIntensity = 1.0 - pow(clamp(sunIntensity, 0.0, 1.0), sunSharpness);
   
   // Add sun atmospheric glow
   float sunGlowFactor = 1.0 - smoothstep(sunSize, sunSize + sunGlow, sunDist);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart7 = R"(
   sunGlowFactor = pow(sunGlowFactor, 2.0); // Stronger in center
   
   // Create sun color that changes with elevation (more red/orange at horizon)
   vec3 sunColorDisk = mix(vec3(1.0, 0.7, 0.3), vec3(1.0, 0.95, 0.8), sunHeight * 2.0);
   vec3 sunColorGlow = mix(vec3(1.0, 0.5, 0.2), vec3(1.0, 0.8, 0.5), sunHeight * 2.0);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart8 = R"(
   
   // Add moon with subtle glow
   float moonSize = 0.025;
   float moonSharpness = 60.0;
   float moonGlow = 0.1;
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart9 = R"(
   float moonIntensity = smoothstep(0.0, moonSize, moonDist);
   moonIntensity = 1.0 - pow(clamp(moonIntensity, 0.0, 1.0), moonSharpness);
   
   // Moon atmospheric glow
   float moonGlowFactor = 1.0 - smoothstep(moonSize, moonSize + moonGlow, moonDist);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart10 = R"(
   moonGlowFactor = pow(moonGlowFactor, 2.0); // Stronger in center
   vec3 moonColor = vec3(0.9, 0.9, 1.0); // Slightly blue-tinted
   
   // Noise-based clouds using FBM noise
   float cloudCoverage = mix(0.5, 0.75, sin(gameTime * 0.01) * 0.5 + 0.5); // Clouds change over time
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart11 = R"(
   float cloudHeight = 0.25; // Clouds height in sky
   float cloudThickness = 0.2; // Vertical thickness
   float cloudSharpness = 2.0; // Controls transition edge 
   
   // Cloud layer 1: Slow, large clouds
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart12 = R"(
   float cloudTime1 = gameTime * 0.005;
   vec2 cloudPos1 = vec2(uv.x + cloudTime1, uv.y);
   float clouds1 = fbm(cloudPos1 * vec2(2.0, 4.0)) * 0.5 + 0.5;
   float cloudMask1 = smoothstep(cloudHeight - cloudThickness, cloudHeight, uv.y) - 
                      smoothstep(cloudHeight, cloudHeight + cloudThickness, uv.y);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart13 = R"(
   float cloudFactor1 = cloudMask1 * smoothstep(0.5 - cloudCoverage, 0.6, clouds1) * dayPhase;
   
   // Cloud layer 2: Higher frequency for detail
   float cloudTime2 = gameTime * 0.008;
   vec2 cloudPos2 = vec2(uv.x + cloudTime2, uv.y);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart14 = R"(
   float clouds2 = fbm(cloudPos2 * vec2(5.0, 10.0)) * 0.3 + 0.5;
   float cloudMask2 = smoothstep(cloudHeight - cloudThickness * 0.5, cloudHeight, uv.y) - 
                      smoothstep(cloudHeight, cloudHeight + cloudThickness * 0.7, uv.y);
   float cloudFactor2 = cloudMask2 * smoothstep(0.4, 0.6, clouds2) * dayPhase * 0.7;
   
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart15 = R"(
   // Combine cloud layers with varying lighting
   float totalCloudFactor = max(cloudFactor1, cloudFactor2);
   
   // Cloud colors change based on time of day and sun position
   vec3 cloudBrightColor = mix(vec3(0.9, 0.8, 0.7), vec3(1.0, 1.0, 1.0), dayPhase); // Sunset to day
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart16 = R"(
   vec3 cloudDarkColor = mix(vec3(0.1, 0.1, 0.2), vec3(0.7, 0.7, 0.7), dayPhase);  // Night to day
   
   // Simple cloud lighting based on height
   float cloudBrightness = 0.6 + 0.4 * (1.0 - uv.y * 2.0);
   vec3 cloudColor = mix(cloudDarkColor, cloudBrightColor, cloudBrightness);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart17 = R"(
   
   // Add stars at night (less visible during day)
   float starThreshold = 0.98;
   float starBrightness = (1.0 - dayPhase) * 0.8; // Stars fade during day
   float stars = step(starThreshold, random(uv * 500.0)) * starBrightness;
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart18 = R"(
   vec3 starColor = vec3(0.9, 0.9, 1.0) * stars;
   
   // Add subtle nebulae in night sky
   vec3 nebulaColor = vec3(0.1, 0.05, 0.2) * (1.0 - dayPhase) * fbm(uv * 5.0);
   
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart19 = R"(
   // Calculate god rays from sun
   float godRaysEffect = 0.0;
   if (dayPhase > 0.2) { // Only during day/sunset
       godRaysEffect = godRays(uv, sunPos, 0.5, 0.95, 0.3, 0.3) * dayPhase;
   }
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart20 = R"(
   
   // Calculate lens flare (only when sun is visible)
   float lensFlareEffect = 0.0;
   if (sunHeight > 0.1 && dayPhase > 0.3) {
       lensFlareEffect = lensFlare(uv, sunPos) * dayPhase * 0.5;
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart21 = R"(
   }
   
   // Combine all sky elements
   vec3 finalSky = skyColor;
   
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart22 = R"(
   // Add stars and nebulae (at night)
   finalSky += starColor + nebulaColor;
   
   // Add sun disk and glow
   finalSky = mix(finalSky, sunColorDisk, sunIntensity * dayPhase);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart23 = R"(
   finalSky = mix(finalSky, sunColorGlow, sunGlowFactor * dayPhase);
   
   // Add moon (at night)
   finalSky = mix(finalSky, moonColor, moonIntensity * (1.0 - dayPhase) * 0.9);
   finalSky = mix(finalSky, moonColor * 0.5, moonGlowFactor * (1.0 - dayPhase) * 0.7);
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart24 = R"(
   
   // Add clouds
   finalSky = mix(finalSky, cloudColor, totalCloudFactor);
   
   // Add god rays and lens flare
)";
#endif
#ifndef USE_VULKAN
const char* skyShaderPart25 = R"(
   finalSky += vec3(1.0, 0.9, 0.7) * godRaysEffect * 0.4;
   finalSky += vec3(1.0, 0.8, 0.6) * lensFlareEffect * 0.6;
   
   return finalSky;
}
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart1 = R"(
void main()
{
   vec4 texColor = texture(worldTexture, TexCoord);
   
   // Determine material type from texture color (in a real game, this would be passed as data)
   int materialId = 0; // Default to empty
   if (texColor.a > 0.0) {
       // Here we would derive the material ID somehow
       // For now, we'll use a simple heuristic
       if (texColor.r > 0.9 && texColor.g > 0.4 && texColor.b < 0.3) {
           materialId = 5; // Fire
       } else if (texColor.r < 0.1 && texColor.g > 0.4 && texColor.b > 0.8) {
           materialId = 2; // Water
       } else if (texColor.r < 0.2 && texColor.g < 0.2 && texColor.b < 0.2) {
           materialId = 15; // Coal
       } else if (texColor.r < 0.2 && texColor.g > 0.5 && texColor.b < 0.3) {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart2 = R"(
           materialId = 16; // Moss
       }
   }
   
   // Draw sky for empty space
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart3 = R"(
   if (texColor.a < 0.01) {
       // Make sky visible in the whole world (not just top half)
       // Use a simplified sky rendering when performance is an issue
       #ifdef SIMPLIFIED_SKY
           // Simple gradient sky (better performance)
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart4 = R"(
           float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5;
           vec3 zenithColor = mix(vec3(0.03, 0.05, 0.2), vec3(0.1, 0.5, 0.9), dayPhase);
           vec3 horizonColor = mix(vec3(0.1, 0.12, 0.25), vec3(0.7, 0.75, 0.85), dayPhase);
           vec3 skyColor = mix(zenithColor, horizonColor, pow(1.0 - TexCoord.y, 2.0));
           
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart5 = R"(
           // Add a simple sun
           float sunHeight = 0.15 + 0.35 * dayPhase;
           vec2 sunPos = vec2(0.5, sunHeight);
           float sunDist = length(TexCoord - sunPos);
           float sunGlow = 1.0 - smoothstep(0.03, 0.08, sunDist);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart6 = R"(
           vec3 sunColor = mix(vec3(1.0, 0.7, 0.3), vec3(1.0, 0.95, 0.8), sunHeight * 2.0);
           skyColor += sunColor * sunGlow * dayPhase;
       #else
           // Full sky with all effects
           vec3 skyColor = createSky(TexCoord);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart7 = R"(
       #endif
       
       // Gradually fade sky into a dark cave background for underground areas
       if (TexCoord.y > 0.5) {
           float fadeDepth = smoothstep(0.5, 1.0, TexCoord.y);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart8 = R"(
           vec3 caveColor = vec3(0.05, 0.04, 0.08); // Dark cave/underground
           skyColor = mix(skyColor, caveColor, fadeDepth);
       }
       
       FragColor = vec4(skyColor, 1.0);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart9 = R"(
       EmissiveColor = vec4(0.0); // No emission for sky/empty
       return;
   }
   
   // Retrieve all material properties for advanced effects
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart10 = R"(
   bool isEmissive = false;
   bool isRefractive = false;
   bool isTranslucent = false;
   bool isReflective = false;
   float emissiveStrength = 0.0;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart11 = R"(
   bool hasWaveEffect = false;
   bool hasGlowEffect = false;
   bool hasParticleEffect = false;
   bool hasShimmerEffect = false;
   float waveSpeed = 0.0;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart12 = R"(
   float waveHeight = 0.0;
   float glowSpeed = 0.0;
   float refractiveIndex = 0.0;
   vec3 secondaryColor = vec3(1.0);
   
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart13 = R"(
   // Use material properties if available
   if (materialId > 0 && materialId < 30) {
       // Visual properties
       isEmissive = materials[materialId].isEmissive;
       isRefractive = materials[materialId].isRefractive;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart14 = R"(
       isTranslucent = materials[materialId].isTranslucent;
       isReflective = materials[materialId].isReflective;
       emissiveStrength = materials[materialId].emissiveStrength;
       
       // Effect properties
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart15 = R"(
       hasWaveEffect = materials[materialId].hasWaveEffect;
       hasGlowEffect = materials[materialId].hasGlowEffect;
       hasParticleEffect = materials[materialId].hasParticleEffect;
       hasShimmerEffect = materials[materialId].hasShimmerEffect;
       waveSpeed = materials[materialId].waveSpeed;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart16 = R"(
       waveHeight = materials[materialId].waveHeight;
       glowSpeed = materials[materialId].glowSpeed;
       refractiveIndex = materials[materialId].refractiveIndex;
       
       // Secondary color
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart17 = R"(
       secondaryColor = materials[materialId].secondaryColor;
   }
   
   // Apply wave effect (for water, lava, etc.)
   if (hasWaveEffect) {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart18 = R"(
       float time = gameTime * waveSpeed * 0.1;
       float waveX = sin(TexCoord.x * 40.0 + time) * waveHeight * 0.01;
       float waveY = sin(TexCoord.y * 30.0 + time * 0.7) * waveHeight * 0.01;
       
       // Calculate wave distortion
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart19 = R"(
       vec2 waveOffset = vec2(waveX, waveY);
       
       // Blend between base color and secondary color based on wave pattern
       float waveBlend = (sin(TexCoord.x * 20.0 + time * 2.0) * 0.5 + 0.5) * 
                         (sin(TexCoord.y * 15.0 + time) * 0.5 + 0.5);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart20 = R"(
       texColor.rgb = mix(texColor.rgb, secondaryColor * texColor.rgb, waveBlend * 0.3);
   }
   
   // Apply glow effect (for fire, lava, toxic materials, etc.)
   if (hasGlowEffect) {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart21 = R"(
       float glowPulse = sin(gameTime * glowSpeed * 0.1) * 0.5 + 0.5;
       emissiveStrength *= (0.8 + glowPulse * 0.4); // Modulate emissive strength
       
       // Add some color variation for more interesting glow
       texColor.rgb = mix(texColor.rgb, secondaryColor, glowPulse * 0.2);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart22 = R"(
   }
   
   // Apply shimmer effect (for water, ice, etc.)
   if (hasShimmerEffect) {
       // Use a simplified noise approach based on the existing noise function from earlier in the shader
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart23 = R"(
       float time = gameTime * 0.2;
       vec2 coord1 = vec2(TexCoord.x * 80.0, TexCoord.y * 80.0 + time);
       vec2 coord2 = vec2(TexCoord.x * 90.0 + time * 0.5, TexCoord.y * 70.0);
       
       // Use hash-based noise (we already have a hash function in this shader)
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart24 = R"(
       float noise1 = hash(floor(coord1) * 543.3199) * 0.5 + 0.5;
       float noise2 = hash(floor(coord2) * 914.3321) * 0.5 + 0.5;
       float sparkle = pow(noise1 * noise2, 8.0) * 0.7;
       
       // Add sparkles to the texture
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart25 = R"(
       texColor.rgb += sparkle * secondaryColor * refractiveIndex * 0.04;
   }
   
   // Apply refraction effect (for water, glass, etc.)
   if (isRefractive) {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart26 = R"(
       float time = gameTime * 0.05;
       float distortion = refractiveIndex * 0.01 * sin(TexCoord.x * 30.0 + time) * sin(TexCoord.y * 40.0 + time);
       texColor.rgb *= 1.1 + distortion; // Distortion effect
   }
   
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart27 = R"(
   // Apply reflective properties (for glass, metal, etc.)
   if (isReflective) {
       // Simulate environment reflection based on view angle
       float viewAngle = abs(dot(normalize(vec2(0.5, 0.5) - TexCoord), vec2(0.0, 1.0)));
       float reflection = pow(1.0 - viewAngle, 2.0) * refractiveIndex * 0.03;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart28 = R"(
       texColor.rgb += reflection * secondaryColor;
   }
   
   // Apply basic lighting (full implementation will come from shadow map)
   vec3 lighting = vec3(0.4, 0.4, 0.5); // Base ambient light
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart29 = R"(
   
   // Biome detection - simplified for this version
   // In a full implementation, biome data would come from the world
   int biomeType = 0; // 0 = default, 1 = forest, 2 = desert, 3 = tundra, 4 = swamp
   
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart30 = R"(
   // Simple biome detection based on x position for demo
   float worldPosX = TexCoord.x * worldSize.x;
   if (worldPosX < worldSize.x * 0.2) biomeType = 1; // Forest on the left
   else if (worldPosX < worldSize.x * 0.4) biomeType = 0; // Default in middle-left
   else if (worldPosX < worldSize.x * 0.6) biomeType = 2; // Desert in middle
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart31 = R"(
   else if (worldPosX < worldSize.x * 0.8) biomeType = 3; // Tundra in middle-right
   else biomeType = 4; // Swamp on the right
   
   // Time of day and biome-specific fog
   vec3 fogColor;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart32 = R"(
   float fogDensity = 0.0;
   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // Same as in createSky()
   
   // Base fog color determined by time of day
   if (dayPhase < 0.3) { // Night
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart33 = R"(
       fogColor = vec3(0.05, 0.05, 0.1); // Dark blue night fog
       fogDensity = 0.02;
   } else if (dayPhase < 0.4) { // Dawn
       fogColor = vec3(0.4, 0.3, 0.3); // Pinkish dawn fog
       fogDensity = 0.015;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart34 = R"(
   } else if (dayPhase < 0.6) { // Day
       fogColor = vec3(0.8, 0.85, 0.9); // Light day fog
       fogDensity = 0.005;
   } else if (dayPhase < 0.7) { // Dusk
       fogColor = vec3(0.5, 0.3, 0.1); // Orange dusk fog
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart35 = R"(
       fogDensity = 0.015;
   } else { // Night
       fogColor = vec3(0.05, 0.05, 0.1); // Dark blue night fog
       fogDensity = 0.02;
   }
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart36 = R"(
   
   // Modify fog based on biome
   if (biomeType == 1) { // Forest
       fogColor *= vec3(0.7, 0.9, 0.7); // Greener
       fogDensity *= 1.2; // Denser
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart37 = R"(
   } else if (biomeType == 2) { // Desert
       fogColor *= vec3(1.0, 0.9, 0.7); // Yellowish
       fogDensity *= 0.3; // Much clearer
   } else if (biomeType == 3) { // Tundra
       fogColor *= vec3(0.8, 0.9, 1.0); // Blueish
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart38 = R"(
       fogDensity *= 0.7; // Clearer
   } else if (biomeType == 4) { // Swamp
       fogColor *= vec3(0.6, 0.7, 0.5); // Greenish brown
       fogDensity *= 1.5; // Thicker
   }
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart39 = R"(
   
   // Depth-based darkness (darker as you go deeper)
   float depthFactor = smoothstep(0.0, 1.0, TexCoord.y);
   lighting *= mix(1.0, 0.3, depthFactor); // Darker with depth
   
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart40 = R"(
   // Apply fog to lighting - stronger in the distance and depth
   float distFromCenter = length(vec2(0.5, 0.2) - TexCoord) * 2.0;
   float fogFactor = 1.0 - exp(-fogDensity * (distFromCenter + depthFactor) * 5.0);
   lighting = mix(lighting, fogColor, clamp(fogFactor, 0.0, 0.5));
   
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart41 = R"(
   // Apply translucency effect
   float alpha = texColor.a;
   if (isTranslucent) {
       // Make translucent materials let more light through
       lighting = mix(lighting, vec3(1.0), 0.3);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart42 = R"(
       
       // Create depth-based transparency for better layering effect
       float depthFactor = 1.0 - TexCoord.y * 0.4; // More transparent at surface
       alpha = mix(texColor.a * 0.7, texColor.a, depthFactor);
   }
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart43 = R"(
   
   // Handle particle effects (fire, smoke, etc.)
   if (hasParticleEffect) {
       // Create particle motion effect using time-based texture coordinates
       float particleTime = gameTime * 0.1;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart44 = R"(
       vec2 particleOffset = vec2(
           sin(TexCoord.y * 10.0 + particleTime) * 0.01,
           cos(TexCoord.x * 8.0 + particleTime * 0.8) * 0.01
       );
       
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart45 = R"(
       // Use a simple procedural noise based on sine waves
       float nx = sin(TexCoord.x * 40.0 + particleTime * 2.0);
       float ny = sin(TexCoord.y * 30.0 + particleTime);
       float nz = sin((TexCoord.x + TexCoord.y) * 20.0 - particleTime * 1.5);
       float particleNoise = (nx * ny * nz * 0.5 + 0.5);
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart46 = R"(
       
       // Modulate color and alpha based on particle effect
       texColor.rgb = mix(texColor.rgb, secondaryColor, particleNoise * 0.3);
       alpha *= (0.7 + particleNoise * 0.5); // Varies transparency for particle effect
   }
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart47 = R"(
   
   // Write to color and emissive buffers
   FragColor = vec4(texColor.rgb * lighting, alpha);
   
   if (isEmissive) {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart48 = R"(
       // Add flickering to emissive materials with glow effect
       float emissiveFactor = emissiveStrength;
       if (hasGlowEffect) {
           // Simple hash-based flickering
           float time = gameTime * glowSpeed * 0.05;
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart49 = R"(
           float flicker = hash(vec2(floor(time * 10.0), floor(TexCoord.y * 20.0))) * 0.5 + 0.5;
           // Smooth out the flicker
           flicker = mix(0.8, 1.2, flicker);
           emissiveFactor *= flicker;
       }
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPart50 = R"(
       
       // Create emissive color based on material and its secondary color
       vec3 emissiveColor = mix(texColor.rgb, secondaryColor, 0.2) * emissiveFactor;
       EmissiveColor = vec4(emissiveColor, alpha);
   } else {
)";
#endif
#ifndef USE_VULKAN
const char* mainShaderPartEnd = R"(
       EmissiveColor = vec4(0.0);
   }
}
)";
#endif
    
    // Advanced shadow & lighting calculation shader 
    const char* shadowFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D worldTexture;\n"
        "uniform sampler2D emissiveTexture;\n"
        "uniform vec2 playerPos;\n"
        "uniform vec2 worldSize;\n"
        "uniform float gameTime;\n"
        
        // Helper functions for noise and variety
        "float hash(vec2 p) {\n"
        "   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        
        "float noise(vec2 p) {\n"
        "   vec2 i = floor(p);\n"
        "   vec2 f = fract(p);\n"
        "   f = f * f * (3.0 - 2.0 * f);\n"
        "   float a = hash(i + vec2(0.0, 0.0));\n"
        "   float b = hash(i + vec2(1.0, 0.0));\n"
        "   float c = hash(i + vec2(0.0, 1.0));\n"
        "   float d = hash(i + vec2(1.0, 1.0));\n"
        "   return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   vec4 texColor = texture(worldTexture, TexCoord);\n"
        "   vec4 emissive = texture(emissiveTexture, TexCoord);\n"
        "   \n"
        "   // Day/night cycle calculation (synced with sky shader)\n"
        "   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5;\n"
        "   float sunHeight = 0.15 + 0.35 * dayPhase;\n"
        "   vec2 sunPos = vec2(0.5, sunHeight);\n"
        "   \n"
        "   // Check if pixel is solid/opaque for shadow casting\n"
        "   bool isSolid = texColor.a > 0.7;\n"
        "   bool isWater = false;\n"
        "   \n"
        "   // Detect water by color - approximation for now\n"
        "   if (texColor.b > 0.5 && texColor.r < 0.2 && texColor.a > 0.4) {\n"
        "       isWater = true;\n"
        "   }\n"
        "   \n"
        "   // Convert texture coordinates to world space\n"
        "   vec2 worldPos = TexCoord * worldSize;\n"
        "   \n"
        "   // Distance from player (light source)\n"
        "   float playerDist = length(worldPos - playerPos);\n"
        "   \n"
        "   // Calculate basic ambient light based on depth\n"
        "   float ambientBase = mix(0.05, 0.35, (1.0 - TexCoord.y * 2.0));\n"
        "   ambientBase = max(0.02, ambientBase);\n" // Ensure minimum ambient visibility
        "   \n"
        "   // Sun contribution to global ambient (affects mainly surface/sky areas)\n"
        "   float sunAmbient = dayPhase * 0.5 * max(0.0, 1.0 - TexCoord.y * 3.0);\n"
        "   ambientBase += sunAmbient;\n"
        "   \n"
        "   // Noise-based variation for underground ambient light\n"
        "   float ambientNoise = noise(worldPos * 0.002 + gameTime * 0.01) * 0.04;\n"
        "   \n"
        "   // Cave atmosphere effect - subtle dust particles and ambient flicker\n"
        "   float caveAtmosphere = noise(worldPos * 0.01 + gameTime * 0.02) * 0.02;\n"
        "   float timeBlink = sin(gameTime * 2.0 + worldPos.x * 0.1) * 0.01;\n"
        "   \n"
        "   // Calculate ambient occlusion for concave areas\n"
        "   float ao = 0.0;\n"
        "   const int AO_SAMPLES = 6;\n"
        "   const float AO_RADIUS = 0.01;\n"
        "   \n"
        "   if (!isSolid && !isWater) {\n"
        "       int solidCount = 0;\n"
        "       for (int i = 0; i < AO_SAMPLES; i++) {\n"
        "           float angle = float(i) / float(AO_SAMPLES) * 6.28;\n"
        "           vec2 offset = vec2(cos(angle), sin(angle)) * AO_RADIUS;\n"
        "           vec2 samplePos = TexCoord + offset;\n"
        "           if (texture(worldTexture, samplePos).a > 0.7) {\n"
        "               solidCount++;\n"
        "           }\n"
        "       }\n"
        "       ao = float(solidCount) / float(AO_SAMPLES) * 0.25;\n"
        "   }\n"
        "   \n"
        "   // Simple 2D shadow calculation with soft shadows\n"
        "   float shadowFactor = 1.0;\n"
        "   \n"
        "   // Direction from current pixel to player\n"
        "   vec2 toPlayer = normalize(playerPos - worldPos);\n"
        "   \n"
        "   // Calculate player light color (warm torch-like light)\n"
        "   vec3 playerLightColor = vec3(1.0, 0.8, 0.5);\n"
        "   float playerLightIntensity = 1.2;\n"
        "   \n"
        "   // Add subtle torch flicker effect\n"
        "   float flicker = 0.9 + 0.1 * sin(gameTime * 10.0 + hash(worldPos) * 10.0);\n"
        "   playerLightIntensity *= flicker;\n"
        "   \n"
        "   // Cast ray toward player to see if in shadow - advanced soft shadows\n"
        "   const int MAX_STEPS = 48;\n"
        "   const float STEP_SIZE = 2.0; // pixels\n"
        "   \n"
        "   if (isSolid) {\n"
        "       // Solid objects cast shadows but aren't in shadow themselves\n"
        "       shadowFactor = 0.0;\n"
        "   } else {\n"
        "       // Soft shadow accumulation\n"
        "       float softShadow = 0.0;\n"
        "       float rayLength = 0.0;\n"
        "       \n"
        "       // Ray march toward player\n"
        "       vec2 pos = worldPos;\n"
        "       bool inShadow = false;\n"
        "       \n"
        "       for (int i = 0; i < MAX_STEPS; i++) {\n"
        "           pos += toPlayer * STEP_SIZE;\n"
        "           rayLength += STEP_SIZE;\n"
        "           \n"
        "           // Check if we reached the player\n"
        "           if (length(pos - playerPos) < 6.0) {\n"
        "               if (!inShadow) {\n"
        "                   softShadow = 1.0;\n"
        "               }\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Convert back to texture coordinates\n"
        "           vec2 texPos = pos / worldSize;\n"
        "           \n"
        "           // Check if outside texture bounds\n"
        "           if (texPos.x < 0.0 || texPos.x > 1.0 || texPos.y < 0.0 || texPos.y > 1.0) {\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Sample with slight noise for varied shadows\n"
        "           float jitter = hash(pos * 0.1 + gameTime) * 0.5;\n"
        "           vec2 jitteredPos = texPos + vec2(jitter, jitter) * 0.001;\n"
        "           \n"
        "           // Check if we hit a solid object\n"
        "           float alpha = texture(worldTexture, jitteredPos).a;\n"
        "           if (alpha > 0.7) {\n"
        "               // Hit something solid, in shadow\n"
        "               float penumbra = rayLength / 300.0; // Wider penumbra over distance\n"
        "               softShadow = max(softShadow, noise(pos * 0.1) * penumbra);\n"
        "               inShadow = true;\n"
        "           }\n"
        "       }\n"
        "       \n"
        "       shadowFactor = softShadow;\n"
        "   }\n"
        "   \n"
        "   // Add distance-based falloff (torch-like light)\n"
        "   float torchRadius = 200.0;\n"
        "   float distFalloff = 1.0 / (1.0 + pow(playerDist/torchRadius, 2.5));\n"
        "   \n"
        "   // Directional light from the sun (only affects surface areas)\n"
        "   float sunContribution = 0.0;\n"
        "   if (TexCoord.y < 0.4) { // Only near surface\n"
        "       float surfaceDepth = TexCoord.y / 0.4; // 0 at top, 1 at depth cutoff\n"
        "       \n"
        "       // Cast ray toward sun to check for shadows from terrain\n"
        "       vec2 toSun = normalize(vec2(0.0, -1.0)); // Sun rays from top\n"
        "       vec2 sunCheckPos = worldPos;\n"
        "       bool inSunShadow = false;\n"
        "       \n"
        "       // Shorter ray for sun shadow (doesn't need to go as far)\n"
        "       for (int i = 0; i < 24; i++) {\n"
        "           sunCheckPos += toSun * 4.0;\n"
        "           vec2 sunTexPos = sunCheckPos / worldSize;\n"
        "           \n"
        "           // Check if outside world bounds or reached top\n"
        "           if (sunTexPos.y <= 0.0) {\n"
        "               // Reached sky, not in shadow\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           if (sunTexPos.x < 0.0 || sunTexPos.x > 1.0 || sunTexPos.y > 1.0) {\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Check if we hit terrain that blocks sun\n"
        "           if (texture(worldTexture, sunTexPos).a > 0.7) {\n"
        "               inSunShadow = true;\n"
        "               break;\n"
        "           }\n"
        "       }\n"
        "       \n"
        "       // Calculate sun contribution based on day phase and shadow\n"
        "       if (!inSunShadow) {\n"
        "           sunContribution = dayPhase * (1.0 - surfaceDepth) * 0.8;\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Water caustics effect when underwater and near surface\n"
        "   float caustics = 0.0;\n"
        "   if (isWater) {\n"
        "       // Calculate based on nearby water surfaces\n"
        "       float causticsNoise = noise(worldPos * 0.05 + vec2(gameTime * 0.1, 0));\n"
        "       caustics = pow(causticsNoise, 2.0) * 0.3 * dayPhase;\n"
        "   }\n"
        "   \n"
        "   // Add contribution from emissive materials with glow radius\n"
        "   float emissiveStrength = length(emissive.rgb);\n"
        "   float emissiveContribution = emissiveStrength;\n"
        "   \n"
        "   // Add emissive glow radius effect\n"
        "   if (emissiveStrength < 0.01) {\n"
        "       // If current pixel isn't emissive, check for nearby emissive sources\n"
        "       const int GLOW_SAMPLES = 16;\n"
        "       const float GLOW_RADIUS = 0.03;\n"
        "       \n"
        "       for (int i = 0; i < GLOW_SAMPLES; i++) {\n"
        "           float angle = float(i) / float(GLOW_SAMPLES) * 6.28;\n"
        "           float sampleDist = hash(vec2(angle, gameTime)) * GLOW_RADIUS;\n"
        "           vec2 offset = vec2(cos(angle), sin(angle)) * sampleDist;\n"
        "           vec2 samplePos = TexCoord + offset;\n"
        "           \n"
        "           vec4 sampleEmissive = texture(emissiveTexture, samplePos);\n"
        "           float sampleStrength = length(sampleEmissive.rgb);\n"
        "           \n"
        "           if (sampleStrength > 0.1) {\n"
        "               float glowFalloff = 1.0 - sampleDist/GLOW_RADIUS;\n"
        "               emissiveContribution = max(emissiveContribution, sampleStrength * glowFalloff * 0.4);\n"
        "           }\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Calculate refracted light through water (realistic underwater light shafts)\n"
        "   float waterShafts = 0.0;\n"
        "   if (isWater && TexCoord.y > 0.1 && TexCoord.y < 0.45) {\n"
        "       // Calculate shaft intensity based on depth and noise\n"
        "       float depthFactor = 1.0 - (TexCoord.y - 0.1) / 0.35;\n"
        "       float shaftNoise = noise(worldPos * 0.01 + vec2(0, gameTime * 0.02));\n"
        "       waterShafts = shaftNoise * depthFactor * dayPhase * 0.2;\n"
        "   }\n"
        "   \n"
        "   // Calculate final lighting\n"
        "   float playerLight = shadowFactor * distFalloff * playerLightIntensity;\n"
        "   float totalLighting = ambientBase + ambientNoise - ao + playerLight + sunContribution + caustics + waterShafts + caveAtmosphere + timeBlink;\n"
        "   \n"
        "   // Add emissive sources after ambient/shadows\n"
        "   totalLighting = max(totalLighting, emissiveContribution);\n"
        "   \n"
        "   // Apply water light attenuation (deeper = darker/bluer)\n"
        "   if (isWater) {\n"
        "       // Underwater lighting is more strongly affected by depth\n"
        "       float waterDepthAttenuation = max(0.2, 1.0 - TexCoord.y * 3.0);\n"
        "       totalLighting *= waterDepthAttenuation;\n"
        "       \n"
        "       // Add blue tint to final lighting for underwater effect\n"
        "       totalLighting = max(0.1, min(1.0, totalLighting));\n"
        "       FragColor = vec4(totalLighting * 0.7, totalLighting * 0.8, totalLighting, 1.0);\n"
        "       return;\n"
        "   }\n"
        "   \n"
        "   // Output shadow map (1.0 = fully lit, 0.0 = in shadow)\n"
        "   totalLighting = clamp(totalLighting, 0.0, 1.0);\n"
        "   FragColor = vec4(vec3(totalLighting), 1.0);\n"
        "}\n";
    
    // Enhanced bloom effect shader with two-pass blur
    const char* bloomFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D emissiveTexture;\n"
        "uniform float gameTime;\n"
        
        "// Helper function for dynamic bloom intensity\n"
        "float hash(vec2 p) {\n"
        "   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   // Get base emissive color\n"
        "   vec4 baseEmission = texture(emissiveTexture, TexCoord);\n"
        "   float luminance = dot(baseEmission.rgb, vec3(0.299, 0.587, 0.114));\n"
        "   \n"
        "   // Apply threshold to isolate bright areas\n"
        "   float threshold = 0.4;\n"
        "   float softThreshold = 0.1;\n"
        "   float brightness = max(0.0, luminance - threshold) / softThreshold;\n"
        "   brightness = min(brightness, 1.0);\n"
        "   vec4 brightColor = baseEmission * brightness;\n"
        "   \n"
        "   // Skip further processing if not bright enough\n"
        "   if (brightness <= 0.0) {\n"
        "       FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
        "       return;\n"
        "   }\n"
        "   \n"
        "   // Two-pass Gaussian blur for better performance and quality\n"
        "   // First pass: horizontal blur\n"
        "   vec4 hBlurColor = vec4(0.0);\n"
        "   float hTotalWeight = 0.0;\n"
        "   \n"
        "   // Dynamic bloom radius (changes with emissive brightness)\n"
        "   float baseBloomRadius = 0.02;\n"
        "   float bloomRadiusMultiplier = 1.0 + luminance * 0.5;\n"
        "   float bloomRadius = baseBloomRadius * bloomRadiusMultiplier;\n"
        "   \n"
        "   // Add subtle animation to bloom radius\n"
        "   float timeVariation = 1.0 + 0.1 * sin(gameTime * 2.0 + hash(TexCoord) * 6.28);\n"
        "   bloomRadius *= timeVariation;\n"
        "   \n"
        "   // First pass: horizontal blur with variable sampling\n"
        "   const int samplesH = 24;\n"
        "   for (int x = -samplesH/2; x < samplesH/2; x++) {\n"
        "       float offset = float(x) * bloomRadius / float(samplesH);\n"
        "       float weight = exp(-offset * offset * 4.0);\n"
        "       vec2 sampleCoord = TexCoord + vec2(offset, 0.0);\n"
        "       \n"
        "       // Apply color-based weighting (brighter pixels contribute more)\n"
        "       vec4 sampleColor = texture(emissiveTexture, sampleCoord);\n"
        "       float sampleLum = dot(sampleColor.rgb, vec3(0.299, 0.587, 0.114));\n"
        "       float lumWeight = max(0.2, sampleLum);\n"
        "       \n"
        "       hBlurColor += sampleColor * weight * lumWeight;\n"
        "       hTotalWeight += weight * lumWeight;\n"
        "   }\n"
        "   vec4 horizontalBlur = hBlurColor / max(0.001, hTotalWeight);\n"
        "   \n"
        "   // Second pass: vertical blur on the result of horizontal blur\n"
        "   vec4 vBlurColor = vec4(0.0);\n"
        "   float vTotalWeight = 0.0;\n"
        "   const int samplesV = 24;\n"
        "   \n"
        "   // Apply vertical sampling - simulate vertical blur pass\n"
        "   for (int y = -samplesV/2; y < samplesV/2; y++) {\n"
        "       float offset = float(y) * bloomRadius / float(samplesV);\n"
        "       float weight = exp(-offset * offset * 4.0);\n"
        "       vec2 sampleCoord = TexCoord + vec2(0.0, offset);\n"
        "       \n"
        "       // Use the horizontalBlur as input for vertical blur\n"
        "       // In a real two-pass implementation, we'd read from the horizontal blur texture\n"
        "       // But since we're simulating it in a single pass, we recompute the horizontal blur\n"
        "       vec4 hBlur = vec4(0.0);\n"
        "       float hWeight = 0.0;\n"
        "       \n"
        "       for (int x = -samplesH/4; x < samplesH/4; x++) { // Use fewer samples for performance\n"
        "           float h_offset = float(x) * bloomRadius / float(samplesH/2);\n"
        "           float h_weight = exp(-h_offset * h_offset * 4.0);\n"
        "           vec2 h_sampleCoord = sampleCoord + vec2(h_offset, 0.0);\n"
        "           hBlur += texture(emissiveTexture, h_sampleCoord) * h_weight;\n"
        "           hWeight += h_weight;\n"
        "       }\n"
        "       \n"
        "       hBlur = hBlur / max(0.001, hWeight);\n"
        "       vBlurColor += hBlur * weight;\n"
        "       vTotalWeight += weight;\n"
        "   }\n"
        "   \n"
        "   // Normalize the vertical blur result\n"
        "   vec4 finalBlur = vBlurColor / max(0.001, vTotalWeight);\n"
        "   \n"
        "   // Apply color adjustments to bloom - can make fire more orange, lights more yellow, etc.\n"
        "   float colorShift = hash(TexCoord + gameTime * 0.01) * 0.1;\n"
        "   vec3 warmShift = vec3(1.0 + colorShift, 1.0, 1.0 - colorShift * 0.5);\n"
        "   \n"
        "   // Enhance bloom based on material type (can be expanded further)\n"
        "   vec3 bloomColor = finalBlur.rgb;\n"
        "   \n"
        "   // Detect if it's likely fire/lava (reddish-yellow)\n"
        "   if (baseEmission.r > baseEmission.g && baseEmission.g > baseEmission.b) {\n"
        "       // Fire-like bloom enhancement (warmer, more orange)\n"
        "       bloomColor *= warmShift * 1.2;\n"
        "   }\n"
        "   \n"
        "   // Output final bloom with increased intensity\n"
        "   FragColor = vec4(bloomColor * 1.5, finalBlur.a);\n"
        "}\n";
    
    // Final post-processing shader with advanced effects
    const char* postProcessingFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D colorTexture;\n"
        "uniform sampler2D bloomTexture;\n"
        "uniform sampler2D shadowMapTexture;\n"
        "uniform float gameTime;\n"
        
        "// Advanced noise helpers\n"
        "float hash13(vec3 p3) {\n"
        "   p3 = fract(p3 * 0.1031);\n"
        "   p3 += dot(p3, p3.yzx + 33.33);\n"
        "   return fract((p3.x + p3.y) * p3.z);\n"
        "}\n"
        
        "float simplex_noise(vec3 p) {\n"
        "   float n = hash13(floor(p));\n"
        "   p = fract(p);\n"
        "   return n * 2.0 - 1.0;\n"
        "}\n"
        
        "// Advanced tone mapping (ACES filmic curve approximation)\n"
        "vec3 ACESToneMapping(vec3 color, float adapted_lum) {\n"
        "   const float A = 2.51;\n"
        "   const float B = 0.03;\n"
        "   const float C = 2.43;\n"
        "   const float D = 0.59;\n"
        "   const float E = 0.14;\n"
        "   color *= adapted_lum;\n"
        "   return (color * (A * color + B)) / (color * (C * color + D) + E);\n"
        "}\n"
        
        "vec3 adjustContrast(vec3 color, float contrast) {\n"
        "   return 0.5 + (contrast * (color - 0.5));\n"
        "}\n"
        
        "vec3 adjustSaturation(vec3 color, float saturation) {\n"
        "   const vec3 luminance = vec3(0.2126, 0.7152, 0.0722);\n"
        "   float grey = dot(color, luminance);\n"
        "   return mix(vec3(grey), color, saturation);\n"
        "}\n"
        
        "vec3 adjustExposure(vec3 color, float exposure) {\n"
        "   return 1.0 - exp(-color * exposure);\n"
        "}\n"
        
        "// Dynamic vignette that changes with light conditions\n"
        "vec3 dynamicVignette(vec3 color, vec2 uv, float lightLevel) {\n"
        "   // Make vignette stronger in dark areas, weaker in well-lit areas\n"
        "   float baseVigStrength = 1.2 - lightLevel * 0.8;\n"
        "   float vigStrength = clamp(baseVigStrength, 0.3, 1.2);\n"
        "   \n"
        "   // Dynamic vignette shape - depth affects shape\n"
        "   float aspect = 1.77; // Assuming 16:9 aspect ratio\n"
        "   vec2 vignetteCenter = vec2(0.5, 0.5);\n"
        "   vec2 coord = (uv - vignetteCenter) * vec2(aspect, 1.0);\n"
        "   \n"
        "   // Non-uniform vignette (stronger at bottom)\n"
        "   float vigShape = length(coord) + abs(coord.y) * 0.2;\n"
        "   float vignette = smoothstep(0.0, 1.4, 1.0 - vigShape * vigStrength);\n"
        "   \n"
        "   // Blue tint in shadows for more atmospheric look\n"
        "   vec3 shadowTint = vec3(0.9, 0.95, 1.05);\n"
        "   vec3 vignetteColor = mix(color * shadowTint, color, vignette);\n"
        "   \n"
        "   return vignetteColor;\n"
        "}\n"
        
        "// Chromatic aberration effect - stronger at screen edges\n"
        "vec3 chromaticAberration(sampler2D tex, vec2 uv, float strength) {\n"
        "   // Distance from center affects aberration strength\n"
        "   float dist = length(uv - 0.5);\n"
        "   float distPower = pow(dist, 1.5) * 2.0;\n"
        "   float aberrationStrength = strength * distPower;\n"
        "   \n"
        "   // Direction from center\n"
        "   vec2 dir = normalize(uv - 0.5);\n"
        "   \n"
        "   // Sample with color channel offsets\n"
        "   vec3 result;\n"
        "   result.r = texture(tex, uv - dir * aberrationStrength).r;\n"
        "   result.g = texture(tex, uv).g;\n"
        "   result.b = texture(tex, uv + dir * aberrationStrength).b;\n"
        "   \n"
        "   return result;\n"
        "}\n"
        
        "// Dynamic film grain that's stronger in darker areas\n"
        "float dynamicFilmGrain(vec2 uv, float time, float luminance) {\n"
        "   // More grain in shadows, less in highlights\n"
        "   float grainStrength = mix(0.08, 0.01, luminance);\n"
        "   \n"
        "   // Animated noise\n"
        "   float noise = simplex_noise(vec3(uv * 500.0, time * 0.5));\n"
        "   return noise * grainStrength;\n"
        "}\n"
        
        "// Light leaks and flares for more cinematic look\n"
        "vec3 lightLeaks(vec2 uv, float time) {\n"
        "   // Slow moving light leak pattern\n"
        "   vec2 leakUV = uv * vec2(0.7, 0.4) + vec2(time * 0.01, time * 0.005);\n"
        "   float leak = pow(simplex_noise(vec3(leakUV, time * 0.1)) * 0.5 + 0.5, 2.0);\n"
        "   \n"
        "   // Light leak is stronger at screen edges\n"
        "   float edgeMask = 1.0 - smoothstep(0.7, 1.0, distance(uv, vec2(0.5)));\n"
        "   leak *= (1.0 - edgeMask) * 0.3;\n"
        "   \n"
        "   // Warm colored light leak\n"
        "   return vec3(leak * 1.0, leak * 0.7, leak * 0.5);\n"
        "}\n"
        
        "// Color grading LUT simulation\n"
        "vec3 colorGrade(vec3 color) {\n"
        "   // Cinematic color grading - slightly teal shadows, golden highlights\n"
        "   vec3 shadows = vec3(0.9, 1.05, 1.1);  // Slightly blue-green shadows\n"
        "   vec3 midtones = vec3(1.0, 1.0, 1.0);  // Neutral midtones\n"
        "   vec3 highlights = vec3(1.1, 1.05, 0.9); // Golden/warm highlights\n"
        "   \n"
        "   // Apply color grading based on luminance\n"
        "   float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
        "   return color * mix(mix(shadows, midtones, smoothstep(0.0, 0.4, luminance)),\n"
        "                      highlights, smoothstep(0.4, 1.0, luminance));\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   // Get base colors with chromatic aberration\n"
        "   vec3 baseColorRGB = chromaticAberration(colorTexture, TexCoord, 0.005);\n"
        "   float baseAlpha = texture(colorTexture, TexCoord).a;\n"
        "   vec4 baseColor = vec4(baseColorRGB, baseAlpha);\n"
        "   vec4 bloom = texture(bloomTexture, TexCoord);\n"
        "   \n"
        "   // Get shadow information, potentially with color (for underwater etc.)\n"
        "   vec4 shadowInfo = texture(shadowMapTexture, TexCoord);\n"
        "   float shadow = shadowInfo.r; // Main shadow/lighting value\n"
        "   \n"
        "   // Check if this is likely water by examining shadow's color channels\n"
        "   bool isLikelyWater = (shadowInfo.b > shadowInfo.r && shadowInfo.g > shadowInfo.r && shadowInfo.a > 0.5);\n"
        "   \n"
        "   // Apply shadowmap to base color\n"
        "   vec3 litColor;\n"
        "   if (isLikelyWater) {\n"
        "       // Special underwater coloring (maintain the blue tint from shadow shader)\n"
        "       litColor = baseColor.rgb * shadowInfo.rgb;\n"
        "   } else {\n"
        "       litColor = baseColor.rgb * (shadow * 0.85 + 0.15);\n" // 0.15 is ambient light\n"
        "   }\n"
        "   \n"
        "   // Get overall scene luminance for effects that depend on light level\n"
        "   float sceneLuminance = shadow;\n"
        "   \n"
        "   // Add bloom effect with dynamic intensity\n"
        "   // More bloom in dark areas for dramatic effect\n"
        "   float bloomIntensity = 2.0 - sceneLuminance * 0.8;\n"
        "   vec3 finalColor = litColor + bloom.rgb * bloomIntensity;\n"
        "   \n"
        "   // Add subtle light leaks in well-lit areas\n"
        "   if (sceneLuminance > 0.5) {\n"
        "       finalColor += lightLeaks(TexCoord, gameTime) * sceneLuminance;\n"
        "   }\n"
        "   \n"
        "   // Advanced tonemapping - preserve colors better\n"
        "   float exposure = 1.0 + sin(gameTime * 0.1) * 0.1; // Subtle exposure fluctuation\n"
        "   finalColor = ACESToneMapping(finalColor, exposure);\n"
        "   \n"
        "   // Color grading for cinematic look\n"
        "   finalColor = colorGrade(finalColor);\n"
        "   \n"
        "   // Apply dynamic contrast based on scene luminance\n"
        "   float contrastAmount = 1.2 - sceneLuminance * 0.2; // More contrast in dark scenes\n"
        "   finalColor = adjustContrast(finalColor, contrastAmount);\n"
        "   \n"
        "   // Saturation adjustment (more saturated in well-lit areas)\n"
        "   float saturationAmount = 1.0 + sceneLuminance * 0.2;\n"
        "   finalColor = adjustSaturation(finalColor, saturationAmount);\n"
        "   \n"
        "   // Apply dynamic vignette based on light levels\n"
        "   finalColor = dynamicVignette(finalColor, TexCoord, sceneLuminance);\n"
        "   \n"
        "   // Dynamic film grain - stronger in darker areas\n"
        "   finalColor += dynamicFilmGrain(TexCoord, gameTime, sceneLuminance);\n"
        "   \n"
        "   // Apply a subtle animated noise to very dark areas to simulate film noise\n"
        "   float darkAreaNoise = 0.0;\n"
        "   if (sceneLuminance < 0.2) {\n"
        "       darkAreaNoise = simplex_noise(vec3(TexCoord * 200.0, gameTime)) * 0.03 * (1.0 - sceneLuminance * 5.0);\n"
        "       finalColor += vec3(darkAreaNoise);\n"
        "   }\n"
        "   \n"
        "   // Ensure sky/empty areas aren't affected by certain effects\n"
        "   if (baseColor.a < 0.01) {\n"
        "       // Just recover the original color without grain and other effects\n"
        "       finalColor = baseColorRGB + bloom.rgb * 1.2;\n"
        "       finalColor = ACESToneMapping(finalColor, exposure);\n"
        "   }\n"
        "   \n"
        "   // Output final color with original alpha\n"
        "   FragColor = vec4(finalColor, baseColor.a);\n"
        "}\n";
    
#ifndef USE_VULKAN
// Helper to create shaders (OpenGL specific)
static GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint program = 0;

    // Create vertex shader
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    
    // Check compilation
    GLint success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }
    
    // Create and compile fragment shader
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    // Check compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    // Create shader program
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check linking
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }
    
    // Clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}
#endif

bool PixelPhys::Renderer::createTexture(int /*width*/, int /*height*/) {
#ifndef USE_VULKAN
    // We're not using textures in this simple renderer
    return true;
#else
    return true;
#endif
}

#ifndef USE_VULKAN
GLuint PixelPhys::Renderer::compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check for compilation errors
    GLint success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint PixelPhys::Renderer::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    if (!vertexShader) return 0;
    
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check for linking errors
    GLint success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }
    
    // Cleanup
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}
#endif

#ifndef USE_VULKAN
void PixelPhys::Renderer::createFramebuffers() {
    // Create main framebuffer for scene rendering
    glGenFramebuffers(1, &m_mainFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_mainFBO);
    
    // Create main color texture
    glGenTextures(1, &m_mainColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mainColorTexture, 0);
    
    // Create emissive texture (for bloom)
    glGenTextures(1, &m_mainEmissiveTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainEmissiveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_mainEmissiveTexture, 0);
    
    // Create depth texture for lighting calculations
    glGenTextures(1, &m_mainDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_screenWidth, m_screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_mainDepthTexture, 0);
    
    // Setup multiple render targets
    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Main framebuffer is not complete!" << std::endl;
    }
    
    // Create shadow map framebuffer
    glGenFramebuffers(1, &m_shadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    
    // Create shadow map texture
    glGenTextures(1, &m_shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_screenWidth, m_screenHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_shadowMapTexture, 0);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow map framebuffer is not complete!" << std::endl;
    }
    
    // Create volumetric lighting framebuffer
    glGenFramebuffers(1, &m_volumetricLightFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_volumetricLightFBO);
    
    // Create volumetric lighting texture - using RGBA16F for HDR lighting
    glGenTextures(1, &m_volumetricLightTexture);
    glBindTexture(GL_TEXTURE_2D, m_volumetricLightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_volumetricLightTexture, 0);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Volumetric light framebuffer is not complete!" << std::endl;
    }
    
    // Create bloom effect framebuffer
    glGenFramebuffers(1, &m_bloomFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO);
    
    // Create bloom texture
    glGenTextures(1, &m_bloomTexture);
    glBindTexture(GL_TEXTURE_2D, m_bloomTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTexture, 0);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Bloom framebuffer is not complete!" << std::endl;
    }
    
    // Reset to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Check for errors
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer creation failed with status: " << status << std::endl;
    }
}
#else
void PixelPhys::Renderer::createFramebuffers() {
    // With Vulkan, framebuffers are created by the backend
}
#endif

#ifndef USE_VULKAN
void PixelPhys::Renderer::updateTexture(const World& world) {
    // Create texture if it doesn't exist
    if (m_textureID == 0) {
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        
        // Set texture parameters for pixel-perfect rendering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Use NEAREST filtering to maintain crisp pixel edges like Noita
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    // Upload the pixel data from the world
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, world.getWidth(), world.getHeight(), 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, world.getPixelData());
}
#else
void PixelPhys::Renderer::updateTexture(const World& world) {
    // In Vulkan mode, we use the backend to update textures
    if (m_worldTexture && m_backend) {
        m_backend->updateTexture(m_worldTexture, world.getPixelData());
    }
}
#endif

// Implementation of the factory function to create render backends
namespace PixelPhys {

std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int screenWidth, int screenHeight) {
    switch (type) {
        case BackendType::OpenGL:
#ifndef USE_VULKAN
            return std::make_unique<OpenGLBackend>(screenWidth, screenHeight);
#else
            std::cerr << "OpenGL backend not available when built with USE_VULKAN" << std::endl;
            return nullptr;
#endif
            
        case BackendType::Vulkan:
#ifdef USE_VULKAN
            return std::make_unique<VulkanBackend>(screenWidth, screenHeight);
#else
            std::cerr << "Vulkan backend not available in this build" << std::endl;
            return nullptr;
#endif
            
        case BackendType::DirectX12:
#ifdef _WIN32
            return std::make_unique<DirectX12Backend>(screenWidth, screenHeight);
#else
            std::cerr << "DirectX12 backend only available on Windows" << std::endl;
            return nullptr;
#endif
            
        default:
            std::cerr << "Unknown backend type requested" << std::endl;
            return nullptr;
    }
}

}
} // namespace PixelPhys