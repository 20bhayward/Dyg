#include "Renderer.h"
#include "RenderBackend.h"
#include "VulkanBackend.h"
#include <iostream>

namespace PixelPhys {

Renderer::Renderer(int screenWidth, int screenHeight, BackendType type)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight) {
    setBackendType(type);
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initialize() {
    return true;  // Initialization happens in initialize(SDL_Window*)
}

bool Renderer::initialize(SDL_Window* window) {
    // Create Vulkan backend
    m_backend = std::make_unique<VulkanBackend>(m_screenWidth, m_screenHeight);
    
    if (!m_backend || !m_backend->initialize()) {
        std::cerr << "Failed to initialize rendering backend\n";
        return false;
    }
    return true;
}

void Renderer::render(const World& world, int cameraX, int cameraY) {
    if (!m_backend) return;

    // Get Vulkan backend
    auto* vulkanBackend = static_cast<VulkanBackend*>(m_backend.get());
    if (!vulkanBackend) return;

    // Begin rendering
    beginFrame();
    
    // Set a dark blue background
    m_backend->setClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    m_backend->clear();

    // Get world dimensions
    int worldWidth = world.getWidth();
    int worldHeight = world.getHeight();
    
    // Fixed pixel size for true voxel/pixel simulation
    const float pixelSize = 2.0f;  // Each world pixel is 2x2 screen pixels for better visibility
    
    // Calculate visible area based on screen size and camera position
    int visibleCellsX = m_screenWidth / pixelSize;
    int visibleCellsY = m_screenHeight / pixelSize;
    
    // Determine view boundaries based on camera position
    int startX = cameraX;
    int startY = cameraY;
    int endX = startX + visibleCellsX;
    int endY = startY + visibleCellsY;
    
    // Clamp to world bounds
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(worldWidth, endX);
    endY = std::min(worldHeight, endY);

    // Render each visible pixel
    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            MaterialType material = world.get(x, y);
            if (material == MaterialType::Empty) continue;

            // Get material color
            float r, g, b;
            getMaterialColor(material, r, g, b);

            // Draw each world pixel as a fixed-size rectangle on the screen
            vulkanBackend->drawRectangle(
                (x - startX) * pixelSize, 
                (y - startY) * pixelSize,
                pixelSize, pixelSize,  // Fixed size for each pixel
                r, g, b
            );
        }
    }

    // End rendering
    endFrame();
}

void Renderer::getMaterialColor(MaterialType material, float& r, float& g, float& b) {
    // Get material color directly from the MATERIAL_PROPERTIES table
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    r = props.r / 255.0f;
    g = props.g / 255.0f;
    b = props.b / 255.0f;
}

void Renderer::cleanup() {
    if (m_backend) {
        m_backend->cleanup();
        m_backend.reset();
    }
}

bool Renderer::setBackendType(BackendType type) {
    if (type == BackendType::Vulkan) {
        m_backend = std::make_unique<VulkanBackend>(m_screenWidth, m_screenHeight);
        return static_cast<bool>(m_backend);
    }
    
    std::cerr << "Unsupported backend type\n";
    return false;
}

// Proxy methods for frame management
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

} // namespace PixelPhys