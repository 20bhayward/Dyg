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

void Renderer::render(const World& world, int cameraX, int cameraY, float zoomLevel) {
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
    
    // Base pixel size for true voxel/pixel simulation, adjusted by zoom level
    const float basePixelSize = 2.0f;  // Each world pixel is 2x2 screen pixels for better visibility
    const float pixelSize = std::floor(basePixelSize * zoomLevel);  // Apply zoom factor with discrete steps to ensure grid alignment
    
    // Calculate visible area based on screen size, camera position, and zoom level
    int visibleCellsX = static_cast<int>(m_screenWidth / pixelSize);
    int visibleCellsY = static_cast<int>(m_screenHeight / pixelSize);
    
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
    for (int y = startY; y < endY; ++y) {  // Normal iteration (world coordinates have 0 at top)
        for (int x = startX; x < endX; ++x) {
            MaterialType material = world.get(x, y);
            if (material == MaterialType::Empty) continue;

            // Get material color
            float r, g, b;
            getMaterialColor(material, r, g, b);

            // Calculate screen position with zoom level applied
            float screenX = (x - startX) * pixelSize;
            float screenY = (y - startY) * pixelSize; // No inversion needed

            // Draw each world pixel as a zoomed rectangle on the screen with a slight gap for grid
            float gridGap = 1.0f;  // Gap between cells to create grid effect
            vulkanBackend->drawRectangle(
                screenX, 
                screenY,
                pixelSize - gridGap, pixelSize - gridGap,  // Size with gap for grid
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