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

void Renderer::render(const World& world, int cameraX, int cameraY) {
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
    
    // Force a 2x zoom by using a pixel size of 2.0
    const float pixelSize = 2.0f;  // Each world pixel is 2 screen pixels - effectively zoomed in
    
    // Calculate how much world space we can display with 2x pixels
    int visibleWorldWidth = m_screenWidth / pixelSize;
    int visibleWorldHeight = m_screenHeight / pixelSize;
    
    // Define viewport bounds
    int startX = cameraX;
    int startY = cameraY;
    int endX = std::min(worldWidth, cameraX + visibleWorldWidth);
    int endY = std::min(worldHeight, cameraY + visibleWorldHeight);
    
    std::cout << "Rendering world from (" << startX << "," << startY 
              << ") to (" << endX << "," << endY << ")" << std::endl;
              
    // First try to use chunks for rendering (much faster)
    // Update active chunks based on camera position
    const_cast<World&>(world).updatePlayerPosition(cameraX + m_screenWidth/2, cameraY + m_screenHeight/2);
    
    // Get the active chunks list
    const auto& activeChunks = world.getActiveChunks();
    
    // Calculate chunk size in pixels
    int chunkPixelWidth = world.getChunkWidth();
    int chunkPixelHeight = world.getChunkHeight();
    
    // Try to render the active chunks first (much more efficient)
    std::cout << "Rendering " << activeChunks.size() << " active chunks" << std::endl;
    bool renderedChunks = false;
    
    if (activeChunks.size() > 0) {
        renderedChunks = true;
        for (const auto& chunkCoord : activeChunks) {
            // Calculate world position of this chunk
            int chunkWorldX = chunkCoord.x * chunkPixelWidth;
            int chunkWorldY = chunkCoord.y * chunkPixelHeight;
            
            // Check if chunk is visible in viewport
            if (chunkWorldX + chunkPixelWidth < cameraX || chunkWorldX >= cameraX + m_screenWidth ||
                chunkWorldY + chunkPixelHeight < cameraY || chunkWorldY >= cameraY + m_screenHeight) {
                continue;  // Skip if not visible
            }
            
            // Get chunk from ChunkManager
            Chunk* chunk = const_cast<World&>(world).getChunkByCoords(chunkCoord.x, chunkCoord.y);
            if (!chunk) continue;
            
            // Calculate visible area of this chunk
            int startCX = std::max(0, cameraX - chunkWorldX);
            int startCY = std::max(0, cameraY - chunkWorldY);
            int endCX = std::min(chunkPixelWidth, cameraX + m_screenWidth - chunkWorldX);
            int endCY = std::min(chunkPixelHeight, cameraY + m_screenHeight - chunkWorldY);
            
            // Render visible pixels in this chunk
            for (int cy = startCY; cy < endCY; cy += 2) {
                for (int cx = startCX; cx < endCX; cx += 2) {
                    // Get material directly from the chunk
                    MaterialType material = chunk->get(cx, cy);
                    if (material == MaterialType::Empty) continue;
                    
                    // Calculate world coordinates
                    int wx = chunkWorldX + cx;
                    int wy = chunkWorldY + cy;
                    
                    // Determine color based on material type using MATERIAL_PROPERTIES
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    
                    // Use the material properties to get the correct color
                    if (material != MaterialType::Empty) {
                        const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                        // Convert from 0-255 to 0.0-1.0 for Vulkan
                        r = props.r / 255.0f;
                        g = props.g / 255.0f;
                        b = props.b / 255.0f;
                    }
                    
                    // Apply a simple pattern based on coordinates for variety
                    int patternX = (wx / 8) % 2;
                    int patternY = (wy / 8) % 2;
                    
                    if ((patternX + patternY) % 2 == 0) {
                        // Lighter shade in checkerboard pattern
                        r = std::min(1.0f, r * 1.2f);
                        g = std::min(1.0f, g * 1.2f);
                        b = std::min(1.0f, b * 1.2f);
                    }
                    
                    // Calculate screen position with zoom
                    float screenX = (wx - cameraX) * pixelSize;
                    float screenY = (wy - cameraY) * pixelSize;
                    
                    // Draw rectangles with the zoomed size
                    vulkanBackend->drawRectangle(
                        screenX, 
                        screenY,
                        2.0f * pixelSize, 2.0f * pixelSize,  // Scale with pixel size
                        r, g, b
                    );
                }
            }
            
            // Draw chunk boundary for debugging
            float chunkScreenX = (chunkWorldX - cameraX) * pixelSize;
            float chunkScreenY = (chunkWorldY - cameraY) * pixelSize;
            
            vulkanBackend->drawRectangle(
                chunkScreenX, chunkScreenY,
                chunkPixelWidth * pixelSize, pixelSize,  // Scale top border
                0.5f, 0.0f, 0.0f
            );
        }
    }
    
    // Fallback to direct rendering if no chunks were rendered
    if (!renderedChunks) {
        std::cout << "Falling back to direct pixel rendering" << std::endl;
        // Direct rendering from the world as a fallback
        for (int y = startY; y < endY; y += 2) {
            for (int x = startX; x < endX; x += 2) {
                // Get material directly from the world
                MaterialType material = world.get(x, y);
                if (material == MaterialType::Empty) continue;
                
                // Determine color based on material type using MATERIAL_PROPERTIES
                float r = 0.0f, g = 0.0f, b = 0.0f;
                
                // Use the material properties to get the correct color
                if (material != MaterialType::Empty) {
                    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                    // Convert from 0-255 to 0.0-1.0 for Vulkan
                    r = props.r / 255.0f;
                    g = props.g / 255.0f;
                    b = props.b / 255.0f;
                }
                
                // Apply a simple pattern based on coordinates for variety
                int patternX = (x / 8) % 2;
                int patternY = (y / 8) % 2;
                
                if ((patternX + patternY) % 2 == 0) {
                    // Lighter shade in checkerboard pattern
                    r = std::min(1.0f, r * 1.2f);
                    g = std::min(1.0f, g * 1.2f);
                    b = std::min(1.0f, b * 1.2f);
                }
                
                // Calculate screen position with zoom
                float screenX = (x - cameraX) * pixelSize;
                float screenY = (y - cameraY) * pixelSize;
                
                // Draw rectangles with the zoomed size
                vulkanBackend->drawRectangle(
                    screenX, 
                    screenY,
                    2.0f * pixelSize, 2.0f * pixelSize,  // Scale with pixel size
                    r, g, b
                );
            }
        }
    }
    
    // Add a grid overlay aligned with chunk boundaries
    
    // Calculate the offset for the first visible chunk grid line
    int firstVisibleChunkX = (cameraX / chunkPixelWidth) * chunkPixelWidth;
    int firstVisibleChunkY = (cameraY / chunkPixelHeight) * chunkPixelHeight;
    
    // Draw vertical grid lines aligned with chunk boundaries
    for (int x = firstVisibleChunkX; x < cameraX + visibleWorldWidth; x += chunkPixelWidth) {
        float screenX = (x - cameraX) * pixelSize;
        vulkanBackend->drawRectangle(
            screenX, 0,
            pixelSize, m_screenHeight,  // Scale grid line width
            0.5f, 0.5f, 0.5f
        );
    }
    
    // Draw horizontal grid lines aligned with chunk boundaries
    for (int y = firstVisibleChunkY; y < cameraY + visibleWorldHeight; y += chunkPixelHeight) {
        float screenY = (y - cameraY) * pixelSize;
        vulkanBackend->drawRectangle(
            0, screenY,
            m_screenWidth, pixelSize,  // Scale grid line height
            0.5f, 0.5f, 0.5f
        );
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