#include "Renderer.h"
#include "RenderBackend.h"
#include "VulkanBackend.h"
#include <iostream>

namespace PixelPhys {

Renderer::Renderer(int screenWidth, int screenHeight, BackendType type)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight) {}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initialize() {
    if (!m_backend || !m_backend->initialize()) {
        std::cerr << "Failed to initialize rendering backend\n";
        return false;
    }
    return true;
}

void Renderer::render(const World& world) {
    if (!m_backend) return;

    m_backend->beginFrame();
    m_backend->setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_backend->clear();

    // Get visible area based on camera position
    int worldWidth = world.getWidth();
    int worldHeight = world.getHeight();

    // Render each visible pixel
    for (int y = worldHeight; y < worldHeight + m_screenHeight; y++) {
        for (int x = worldWidth; x < worldHeight + m_screenWidth; x++) {
            if (x < 0 || x >= worldWidth || y < 0 || y >= worldHeight) continue;

            MaterialType material = world.getMaterialAt(x, y);
            if (material == MaterialType::Empty) continue;

            // Get material color
            float r, g, b;
            getMaterialColor(material, r, g, b);

            // Convert world coordinates to screen coordinates
            float screenX = static_cast<float>(x - worldWidth);
            float screenY = static_cast<float>(y - worldHeight);

            // Draw pixel
            m_backend->drawPoint(screenX, screenY, r, g, b);
        }
    }

    m_backend->endFrame();
}

void Renderer::getMaterialColor(MaterialType material, float& r, float& g, float& b) {
    // Simple material color mapping
    switch (material) {
        case MaterialType::Sand:
            r = 0.94f; g = 0.78f; b = 0.29f;
            break;
        case MaterialType::Water:
            r = 0.0f;  g = 0.46f; b = 0.89f;
            break;
        case MaterialType::Stone:
            r = 0.5f;  g = 0.5f;  b = 0.5f;
            break;
        case MaterialType::Fire:
            r = 1.0f;  g = 0.3f;  b = 0.0f;
            break;
        case MaterialType::Dirt:
            r = 0.4f;  g = 0.25f; b = 0.1f;
            break;
        case MaterialType::Wood:
            r = 0.5f;  g = 0.35f; b = 0.15f;
            break;
        case MaterialType::Grass:
            r = 0.2f;  g = 0.7f;  b = 0.2f;
            break;
        default:
            r = 1.0f; g = 1.0f; b = 1.0f;
    }
}

void Renderer::cleanup() {
    if (m_backend) {
        m_backend->cleanup();
        m_backend.reset();
    }
}

bool Renderer::setBackendType(BackendType type) {
    m_backend = CreateRenderBackend(type, m_screenWidth, m_screenHeight);
    return static_cast<bool>(m_backend);
}

// Factory function implementation
std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int width, int height) {
    switch (type) {
        case BackendType::Vulkan:
            return std::make_unique<VulkanBackend>(width, height);
        default:
            std::cerr << "Unsupported backend type\n";
            return nullptr;
    }
}

} // namespace PixelPhys