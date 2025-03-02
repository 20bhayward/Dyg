#include "Renderer.h"
#include "RenderBackend.h"
#include "VulkanBackend.h"
#include <iostream>
#include <random>
#include <cstdlib>
#include <ctime>

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
    
    // Set a black background for better contrast
    m_backend->setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_backend->clear();

    // Get world dimensions
    int worldWidth = world.getWidth();
    int worldHeight = world.getHeight();
    
    // Base pixel size for true voxel/pixel simulation, adjusted by zoom level
    const float basePixelSize = 1.5f;  // Each world pixel is 1.5x1.5 screen pixels for better resolution
    const float pixelSize = std::floor(basePixelSize * zoomLevel);  // Apply zoom factor with discrete steps to ensure grid alignment
    
    // Calculate visible area based on screen size, camera position, and zoom level
    int visibleCellsX = static_cast<int>(m_screenWidth / pixelSize);
    int visibleCellsY = static_cast<int>(m_screenHeight / pixelSize);
    
    // Determine view boundaries based on camera position
    int startX = cameraX;
    int startY = cameraY;
    int endX = startX + visibleCellsX;
    int endY = startY + visibleCellsY;
    
    // Debug boundaries
    std::cout << "Rendering from (" << startX << "," << startY << ") to (" << endX << "," << endY 
              << ") - World size: " << worldWidth << "x" << worldHeight << std::endl;
    
    // Clamp to world bounds
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(worldWidth, endX);
    endY = std::min(worldHeight, endY);

    // Create a new hash seed every render to ensure consistency
    std::mt19937 posRng(12345);

    // Render each visible pixel
    for (int y = startY; y < endY; ++y) {  // Normal iteration (world coordinates have 0 at top)
        for (int x = startX; x < endX; ++x) {
            MaterialType material = world.get(x, y);
            if (material == MaterialType::Empty) continue;

            // Get material color with position-based variations
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Create deterministic position-based hash for consistent variations
            int hash = (x * 13 + y * 37) ^ 0x8F3A71C5;  // Simple hash function
            int rVar = (hash % 31) - 15;   // -15 to 15 variation (more moderate)
            int gVar = ((hash >> 8) % 31) - 15;
            int bVar = ((hash >> 16) % 31) - 15;
            
            // Material-specific modifications
            // Apply moderate variations to certain materials
            switch (material) {
                case MaterialType::Stone:
                    // Make stone more gray with moderate variation
                    rVar = gVar = bVar = (hash % 41) - 20;
                    break;
                case MaterialType::Grass:
                    // Grass has moderate green variations
                    gVar = (hash % 31) - 15;  // More green variation
                    break;
                case MaterialType::TopSoil:
                case MaterialType::Dirt:
                    // Soil has brown variation
                    rVar = (hash % 25) - 12;
                    gVar = ((hash >> 4) % 21) - 10;
                    bVar = ((hash >> 8) % 17) - 8;
                    break;
                case MaterialType::Gravel:
                    // Gravel has distinct texture
                    rVar = gVar = bVar = (hash % 37) - 18;
                    break;
                case MaterialType::DenseRock:
                    // Dense rock has blue-gray variations
                    rVar = (hash % 25) - 12;
                    gVar = (hash % 25) - 12;
                    bVar = (hash % 33) - 16; // Slight blue tint variation
                    break;
                default:
                    // Default variation is already set
                    break;
            }
            
            // Apply color variation with proper clamping
            int rInt = std::max(0, std::min(255, int(props.r) + rVar));
            int gInt = std::max(0, std::min(255, int(props.g) + gVar));
            int bInt = std::max(0, std::min(255, int(props.b) + bVar));
            
            // Convert to float for Vulkan
            float r = rInt / 255.0f;
            float g = gInt / 255.0f;
            float b = bInt / 255.0f;

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
    // Get the raw pixel data directly from the world for the given material
    // We need to generate random variations for each pixel based on position
    
    // Get base color with some variation
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    
    // Create variation based on random 
    static std::mt19937 rng(12345); // Fixed seed for consistent colors
    std::uniform_int_distribution<int> varDist(-50, 50);
    
    // Apply strong variation to each color component
    int rVar = varDist(rng);
    int gVar = varDist(rng);
    int bVar = varDist(rng);
    
    // Calculate final color with variation
    int rInt = std::max(0, std::min(255, int(props.r) + rVar));
    int gInt = std::max(0, std::min(255, int(props.g) + gVar));
    int bInt = std::max(0, std::min(255, int(props.b) + bVar));
    
    // Convert to float in range [0, 1]
    r = rInt / 255.0f;
    g = gInt / 255.0f;
    b = bInt / 255.0f;
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