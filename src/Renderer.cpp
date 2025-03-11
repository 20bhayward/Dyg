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
    
    // Match the pixel size from main_vulkan.cpp
    const float pixelSize = 2.0f;  // Each world pixel is 2.0 screen pixels - MUST MATCH PIXEL_SIZE in main_vulkan.cpp
    
    // Calculate how much world space we can display with 2x pixels
    int visibleWorldWidth = m_screenWidth / pixelSize;
    int visibleWorldHeight = m_screenHeight / pixelSize;
    
    // Define viewport bounds
    int startX = cameraX;
    int startY = cameraY;
    int endX = std::min(worldWidth, cameraX + visibleWorldWidth);
    int endY = std::min(worldHeight, cameraY + visibleWorldHeight);
    
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "FPS: " << frameCount << std::endl;
    }
              
    // First try to use chunks for rendering (much faster)
    // Center the player position exactly in the middle of the screen for proper chunk loading
    const_cast<World&>(world).updatePlayerPosition(cameraX + visibleWorldWidth/2, cameraY + visibleWorldHeight/2);
    
    // Only output active chunks occasionally to reduce spam
    static int chunkDebugCounter = 0;
    if (chunkDebugCounter++ % 60 == 0) {
        std::cout << "Renderer - Active chunks: " << world.getActiveChunks().size() << std::endl;
    }
    
    // Get the active chunks list
    const auto& activeChunks = world.getActiveChunks();
    
    // Calculate chunk size in pixels
    int chunkPixelWidth = world.getChunkWidth();
    int chunkPixelHeight = world.getChunkHeight();
    
    // Try to render the active chunks first
    bool renderedAnything = false;
    
    // Count of draw calls
    int drawCalls = 0;
    const int MAX_DRAW_CALLS = 150000; // Very high limit to ensure proper rendering quality
    
    if (activeChunks.size() > 0) {
        // Render ALL active chunks for proper streaming
        int chunksToRender = static_cast<int>(activeChunks.size());
        
        for (int i = 0; i < chunksToRender; i++) {
            const auto& chunkCoord = activeChunks[i];
            int chunkWorldX = chunkCoord.x * chunkPixelWidth;
            int chunkWorldY = chunkCoord.y * chunkPixelHeight;
            
            // Skip if not visible
            if (chunkWorldX + chunkPixelWidth < cameraX || chunkWorldX >= cameraX + m_screenWidth/pixelSize ||
                chunkWorldY + chunkPixelHeight < cameraY || chunkWorldY >= cameraY + m_screenHeight/pixelSize) {
                continue;  
            }
            
            // Get the chunk
            Chunk* chunk = const_cast<World&>(world).getChunkByCoords(chunkCoord.x, chunkCoord.y);
            if (!chunk) continue;
            
            // Draw chunk boundary
            float chunkScreenX = (chunkWorldX - cameraX) * pixelSize;
            float chunkScreenY = (chunkWorldY - cameraY) * pixelSize;
            float chunkScreenWidth = chunkPixelWidth * pixelSize;
            float chunkScreenHeight = chunkPixelHeight * pixelSize;
            
            // Don't draw chunk outlines to keep the display clean
            // Just increase counter to maintain the same flow
            drawCalls++;
            
            if (drawCalls >= MAX_DRAW_CALLS) break;
            
            // Use a step size of 1 to ensure all pixels are rendered clearly
            int step = 1; // Sample every single pixel for perfect visual quality
            
            for (int cy = 0; cy < chunkPixelHeight; cy += step) {
                for (int cx = 0; cx < chunkPixelWidth; cx += step) {
                    // Get material directly from the chunk
                    MaterialType material = chunk->get(cx, cy);
                    if (material == MaterialType::Empty) continue;
                    
                    // Calculate world coordinates
                    int wx = chunkWorldX + cx;
                    int wy = chunkWorldY + cy;
                    
                    // Determine color based on material type
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    
                    // Get material color
                    getMaterialColor(material, r, g, b, wx, wy);
                    
                    // Calculate screen position 
                    float screenX = (wx - cameraX) * pixelSize;
                    float screenY = (wy - cameraY) * pixelSize;
                    
                    // Draw pixel as rectangle with larger size for efficiency
                    vulkanBackend->drawRectangle(
                        screenX, screenY,
                        pixelSize * step, pixelSize * step,
                        r, g, b
                    );
                    
                    drawCalls++;
                    renderedAnything = true;
                    
                    if (drawCalls >= MAX_DRAW_CALLS) break;
                }
                if (drawCalls >= MAX_DRAW_CALLS) break;
            }
            
            if (drawCalls >= MAX_DRAW_CALLS) break;
        }
    } 
    
    // Draw test pattern if needed
    if (!renderedAnything) {
        // Draw a simple test pattern
        for (int y = 0; y < 20 && drawCalls < MAX_DRAW_CALLS; y++) {
            for (int x = 0; x < 20 && drawCalls < MAX_DRAW_CALLS; x++) {
                float r = (float)x / 20.0f;
                float g = (float)y / 20.0f;
                float b = 0.5f;
                
                vulkanBackend->drawRectangle(
                    x * 20.0f, y * 20.0f,
                    16.0f, 16.0f,
                    r, g, b
                );
                
                drawCalls++;
            }
        }
    }
    
    // Print debug info
    if (frameCount % 60 == 0) {
        std::cout << "Draw calls: " << drawCalls << std::endl;
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