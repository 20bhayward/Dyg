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
    
    // Force a 4x zoom by using a pixel size of 4.0 (doubled from 2.0)
    const float pixelSize = 4.0f;  // Each world pixel is 4 screen pixels - effectively zoomed in more
    
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
            for (int cy = startCY; cy < endCY; cy += 1) {
                for (int cx = startCX; cx < endCX; cx += 1) {
                    // Get material directly from the chunk
                    MaterialType material = chunk->get(cx, cy);
                    if (material == MaterialType::Empty) continue;
                    
                    // Calculate world coordinates
                    int wx = chunkWorldX + cx;
                    int wy = chunkWorldY + cy;
                    
                    // Determine color based on material type using MATERIAL_PROPERTIES
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    
                    // Get material color with position-based variations
                    getMaterialColor(material, r, g, b, wx, wy);
                    
                    // Calculate screen position with zoom
                    float screenX = (wx - cameraX) * pixelSize;
                    float screenY = (wy - cameraY) * pixelSize;
                    
                    // Draw rectangles with the zoomed size - now 1x1 world pixels
                    vulkanBackend->drawRectangle(
                        screenX, 
                        screenY,
                        pixelSize, pixelSize,  // Scale with pixel size (1x1 pixels)
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
        for (int y = startY; y < endY; y += 1) {
            for (int x = startX; x < endX; x += 1) {
                // Get material directly from the world
                MaterialType material = world.get(x, y);
                if (material == MaterialType::Empty) continue;
                
                // Determine color based on material type using MATERIAL_PROPERTIES
                float r = 0.0f, g = 0.0f, b = 0.0f;
                
                // Get material color with position-based variations
                getMaterialColor(material, r, g, b, x, y);
                
                // Calculate screen position with zoom
                float screenX = (x - cameraX) * pixelSize;
                float screenY = (y - cameraY) * pixelSize;
                
                // Draw rectangles with the zoomed size - now 1x1 world pixels
                vulkanBackend->drawRectangle(
                    screenX, 
                    screenY,
                    pixelSize, pixelSize,  // Scale with pixel size (1x1 pixels)
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

void Renderer::getMaterialColor(MaterialType material, float& r, float& g, float& b, int x, int y) {
    // Get the raw pixel data directly from the world for the given material
    // We generate position-based variations for consistent texturing
    
    // Get base color
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    
    // Skip processing for empty material
    if (material == MaterialType::Empty) {
        r = g = b = 0.0f;
        return;
    }
    
    // Create position-based hash for consistent texture patterns
    int hash = ((x * 13) + (y * 7)) ^ ((x * 23) + (y * 17));
    
    // Initial variation values - moderate defaults for all materials
    int rVar = (hash % 21) - 10;  
    int gVar = ((hash >> 4) % 21) - 10;
    int bVar = ((hash >> 16) % 31) - 15;
    
    // Material-specific modifications for texture patterns
    switch (material) {
        case MaterialType::Stone:
            // Stone has gray variations with strong texture
            rVar = gVar = bVar = (hash % 41) - 20;
            // Add dark speckles
            if ((hash % 5) == 0) {
                rVar -= 25;
                gVar -= 25;
                bVar -= 25;
            }
            break;
            
        case MaterialType::Grass:
            // Grass has strong green variations with patches
            gVar = (hash % 31) - 15; // More green variation
            // Add occasional darker patches
            if ((hash % 7) == 0) {
                gVar -= 30;
                rVar -= 15;
            }
            break;
            
        case MaterialType::TopSoil:
        case MaterialType::Dirt:
            // Soil has rich brown variations with texture
            rVar = (hash % 25) - 12;
            gVar = ((hash >> 4) % 21) - 10;
            bVar = ((hash >> 8) % 17) - 8;
            // Add darker patches
            if ((hash % 4) == 0) {
                rVar -= 15;
                gVar -= 12;
            }
            break;
            
        case MaterialType::Gravel:
            // Gravel has strong texture with varied gray tones
            rVar = gVar = bVar = (hash % 37) - 18;
            // Add mixed size pebble effect
            if ((hash % 5) == 0) {
                rVar -= 25;
                gVar -= 25;
                bVar -= 25;
            } else if ((hash % 11) == 0) {
                rVar += 15;
                gVar += 15;
                bVar += 15;
            }
            break;
            
        case MaterialType::DenseRock:
            // Dense rock has dark blue-gray coloration with crystalline texture
            rVar = (hash % 18) - 9;
            gVar = (hash % 18) - 9;
            bVar = (hash % 22) - 9; // Slight blue tint
            // Add occasional mineral veins or crystalline structures
            if ((hash % 9) == 0) {
                bVar += 15; // Blueish highlights
            }
            break;
            
        case MaterialType::Sand:
            // Sand has yellow-brown variations with visible texture
            rVar = (hash % 30) - 10;
            gVar = (hash % 25) - 12;
            bVar = (hash % 15) - 8;
            // Add occasional darker grains
            if ((hash % 6) == 0) {
                rVar -= 15;
                gVar -= 15;
            }
            break;
            
        case MaterialType::Water:
            // Water has blue variations with some subtle waves
            bVar = (hash % 25) - 10;
            // Slight green tint variations for depth perception
            gVar = (hash % 15) - 7;
            // Very minimal red variation
            rVar = (hash % 6) - 3;
            break;
            
        case MaterialType::Lava:
            // Lava has hot red-orange variations with bright spots
            rVar = (hash % 25) - 5; // More red, less reduction
            gVar = (hash % 25) - 15; // More variation in orange
            bVar = (hash % 6) - 3; // Minor blue variation
            // Add occasional bright yellow-white spots
            if ((hash % 10) == 0) {
                rVar += 20;
                gVar += 15;
            }
            break;
            
        case MaterialType::Snow:
            // Snow has very subtle blue-white variations
            rVar = gVar = bVar = (hash % 10) - 5;
            break;
            
        case MaterialType::Bedrock:
            // Bedrock has dark gray variations with some texture
            rVar = gVar = bVar = (hash % 15) - 7;
            // Add some occasional darker spots for texture
            if ((hash % 8) == 0) {
                rVar -= 10;
                gVar -= 10;
                bVar -= 10;
            }
            break;
            
        case MaterialType::Sandstone:
            // Sandstone has beige-tan variations
            rVar = (hash % 16) - 8;
            gVar = (hash % 14) - 7;
            bVar = (hash % 8) - 4;
            break;
            
        case MaterialType::Fire:
            // Fire has flickering yellow-orange-red variations
            rVar = (hash % 20) - 5; // Strong red
            gVar = (hash % 30) - 15; // Varied green for yellow/orange
            bVar = (hash % 10) - 8; // Minimal blue
            // Random bright spots
            if ((hash % 3) == 0) {
                rVar += 15;
                gVar += 10;
            }
            break;
            
        case MaterialType::Oil:
            // Oil has dark brown-black variations with slight shine
            rVar = (hash % 12) - 8;
            gVar = (hash % 10) - 7;
            bVar = (hash % 8) - 6;
            // Occasional slight shine
            if ((hash % 12) == 0) {
                rVar += 8;
                gVar += 8;
                bVar += 8;
            }
            break;
            
        case MaterialType::GrassStalks:
            // Grass stalks have varied green shades
            gVar = (hash % 22) - 8; // Strong green variation
            rVar = (hash % 10) - 5; // Some red variation for yellowish tints
            bVar = (hash % 8) - 4; // Minor blue variation
            break;
            
        case MaterialType::FlammableGas:
            // Flammable gas has subtle greenish variations with transparency
            gVar = (hash % 15) - 5;
            rVar = (hash % 8) - 4;
            bVar = (hash % 8) - 4;
            break;
            
        // Add ore materials
        case MaterialType::IronOre:
            // Iron ore has gray-blue tints with metallic specks
            rVar = gVar = (hash % 25) - 12;
            bVar = (hash % 30) - 12;
            // Add metallic highlights
            if ((hash % 4) == 0) {
                rVar += 20;
                gVar += 20;
                bVar += 25;
            }
            break;
            
        case MaterialType::CopperOre:
            // Copper has orange-brown coloration with patchy texture
            rVar = (hash % 35) - 10;  // Strong orange-red variation
            gVar = (hash % 25) - 15;  // Less green variation
            bVar = (hash % 15) - 10;  // Minimal blue
            break;
            
        case MaterialType::GoldOre:
            // Gold has shiny yellow with highlight sparkles
            rVar = (hash % 30) - 10;  // Strong yellow-red
            gVar = (hash % 30) - 15;  // Yellow-green
            bVar = (hash % 10) - 8;   // Minimal blue
            // Add shiny spots
            if ((hash % 4) == 0) {
                rVar += 30;
                gVar += 20;
            }
            break;
            
        case MaterialType::CoalOre:
            // Coal has dark with occasional shiny bits
            rVar = gVar = bVar = (hash % 12) - 9;  // Generally dark
            // Add occasional shiny anthracite highlights
            if ((hash % 7) == 0) {
                rVar += 15;
                gVar += 15;
                bVar += 15;
            }
            break;
            
        case MaterialType::DiamondOre:
            // Diamond has blue-white sparkles in dark matrix
            rVar = (hash % 20) - 10;
            gVar = (hash % 25) - 10;
            bVar = (hash % 35) - 10;  // More blue variation
            // Add bright sparkles
            if ((hash % 3) == 0) {
                rVar += 30;
                gVar += 40;
                bVar += 50;
            }
            break;
            
        default:
            // For any other materials, use moderate general variations
            rVar = ((hash % 21) - 10);
            gVar = ((hash >> 4) % 21) - 10;
            bVar = ((hash >> 8) % 21) - 10;
            break;
    }
    
    // Apply color variation with proper clamping
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