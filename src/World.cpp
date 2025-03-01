#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>

namespace PixelPhys {

// Chunk implementation

Chunk::Chunk(int posX, int posY) : m_posX(posX), m_posY(posY), m_isDirty(true) {
    // Initialize chunk with empty cells
    m_grid.resize(WIDTH * HEIGHT, MaterialType::Empty);
    
    // Initialize pixel data for rendering (RGBA for each cell)
    m_pixelData.resize(WIDTH * HEIGHT * 4, 0);
}

MaterialType Chunk::get(int x, int y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return MaterialType::Empty;
    }
    // Position-based material access for pixel-perfect alignment
    int idx = y * WIDTH + x;
    
    // Make absolutely sure we're in bounds
    if (idx < 0 || idx >= static_cast<int>(m_grid.size())) {
        return MaterialType::Empty;
    }
    
    return m_grid[idx];
}

void Chunk::set(int x, int y, MaterialType material) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }
    
    // Position-based material access for pixel-perfect alignment
    int idx = y * WIDTH + x;
    
    // Make absolutely sure we're in bounds
    if (idx < 0 || idx >= static_cast<int>(m_grid.size())) {
        return;
    }
    
    MaterialType oldMaterial = m_grid[idx];
    if (oldMaterial != material) {
        // Always replace the existing material rather than placing on top
        m_grid[idx] = material;
        m_isDirty = true;
        
        // Update pixel data for this cell with pixel-perfect alignment
        int pixelIdx = idx * 4;
        
        // Ensure we're in bounds for the pixel data too
        if (pixelIdx < 0 || pixelIdx + 3 >= static_cast<int>(m_pixelData.size())) {
            return;
        }
        
        const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
        
        // Use deterministic position-based variation for consistent textures
        // This ensures aligned materials have consistent textures rather than random
        int posHash = ((x * 13) + (y * 7)) % 32;
        int variation = (posHash % 7) - 3;
        
        m_pixelData[pixelIdx] = std::max(0, std::min(255, static_cast<int>(props.r) + variation));     // R
        m_pixelData[pixelIdx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + variation));   // G
        m_pixelData[pixelIdx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + variation));   // B
        m_pixelData[pixelIdx+3] = props.transparency;  // A (using default 255 for fully opaque)
    }
}

void Chunk::update(Chunk* chunkBelow, Chunk* chunkLeft, Chunk* chunkRight) {
    // Simplified version - just update pixel data
    updatePixelData();
}

void Chunk::updatePixelData() {
    // Update pixel data for all cells in the chunk
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType material = m_grid[idx];
            int pixelIdx = idx * 4;
            
            if (material == MaterialType::Empty) {
                // Empty cells are transparent
                m_pixelData[pixelIdx] = 0;
                m_pixelData[pixelIdx+1] = 0;
                m_pixelData[pixelIdx+2] = 0;
                m_pixelData[pixelIdx+3] = 0;
            } else {
                const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                
                // Create a position-based variation for more natural look
                int posHash = ((x * 13) + (y * 7)) % 32;
                int variation = (posHash % 7) - 3;
                
                // Apply the variation to the base color
                int r = props.r + (variation + props.varR) % 20 - 10;
                int g = props.g + (variation + props.varG) % 20 - 10;
                int b = props.b + (variation + props.varB) % 20 - 10;
                
                // Clamp values to valid range
                m_pixelData[pixelIdx] = std::max(0, std::min(255, r));
                m_pixelData[pixelIdx+1] = std::max(0, std::min(255, g));
                m_pixelData[pixelIdx+2] = std::max(0, std::min(255, b));
                m_pixelData[pixelIdx+3] = props.transparency;
            }
        }
    }
    
    // Mark chunk as clean after updating
    m_isDirty = false;
}

// World implementation

World::World(int width, int height) : m_width(width), m_height(height) {
    // Compute chunk dimensions
    m_chunksX = (width + Chunk::WIDTH - 1) / Chunk::WIDTH;
    m_chunksY = (height + Chunk::HEIGHT - 1) / Chunk::HEIGHT;
    
    // Create all chunks
    m_chunks.resize(m_chunksX * m_chunksY);
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            m_chunks[y * m_chunksX + x] = std::make_unique<Chunk>(x * Chunk::WIDTH, y * Chunk::HEIGHT);
        }
    }
    
    // Initialize pixel data for rendering
    m_pixelData.resize(width * height * 4, 0);
    
    // Initialize RNG
    std::random_device rd;
    m_rng = std::mt19937(rd());
    
    std::cout << "Created world with dimensions: " << width << "x" << height << std::endl;
    std::cout << "Chunk grid size: " << m_chunksX << "x" << m_chunksY << std::endl;
}

MaterialType World::get(int x, int y) const {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return MaterialType::Empty;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    const Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return MaterialType::Empty;
    }
    
    // Get material at local coordinates
    return chunk->get(localX, localY);
}

void World::set(int x, int y, MaterialType material) {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return;
    }
    
    // Set material at local coordinates
    chunk->set(localX, localY, material);
    
    // Update pixel data
    int idx = y * m_width + x;
    int pixelIdx = idx * 4;
    
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    
    // Create a position-based variation for more natural look
    int posHash = ((x * 13) + (y * 7)) % 32;
    int variation = (posHash % 7) - 3;
    
    m_pixelData[pixelIdx] = std::max(0, std::min(255, static_cast<int>(props.r) + variation));     // R
    m_pixelData[pixelIdx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + variation));   // G
    m_pixelData[pixelIdx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + variation));   // B
    m_pixelData[pixelIdx+3] = props.transparency;  // A
}

void World::update() {
    // Update all chunks
    for (auto& chunk : m_chunks) {
        if (chunk && chunk->isDirty()) {
            // Find neighbor chunks
            int chunkIdx = &chunk - &m_chunks[0];
            int y = chunkIdx / m_chunksX;
            int x = chunkIdx % m_chunksX;
            
            // Get neighbor chunks
            Chunk* chunkBelow = (y < m_chunksY - 1) ? getChunkAt(x, y + 1) : nullptr;
            Chunk* chunkLeft = (x > 0) ? getChunkAt(x - 1, y) : nullptr;
            Chunk* chunkRight = (x < m_chunksX - 1) ? getChunkAt(x + 1, y) : nullptr;
            
            // Update this chunk
            chunk->update(chunkBelow, chunkLeft, chunkRight);
        }
    }
    
    // Update the combined pixel data from all chunks
    updatePixelData();
}

void World::update(int startX, int startY, int endX, int endY) {
    // Simplified - just update the entire world
    update();
}

void World::levelLiquids() {
    // Not needed for simplified version
}

void World::levelLiquids(int startX, int startY, int endX, int endY) {
    // Not needed for simplified version
}

void World::generate(unsigned int seed) {
    // Seed the RNG
    m_rng.seed(seed);
    
    // Clear the world first with empty space
    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            set(x, y, MaterialType::Empty);
        }
    }
    
    // Create ground layer about 60% of the way down
    int groundLevel = m_height * 3 / 5;
    
    // Create terrain with gentle hills
    for (int x = 0; x < m_width; ++x) {
        // Base height with small variations using sine
        int baseHeight = groundLevel;
        int smallHills = static_cast<int>(sin(x * 0.03f) * 20); 
        int mediumHills = static_cast<int>(sin(x * 0.01f) * 40);
        int largeHills = static_cast<int>(sin(x * 0.005f) * 60);
        
        // Combine different scales of hills
        int terrainHeight = baseHeight + smallHills + mediumHills + largeHills;
        
        // Add dirt and stone layers
        for (int y = terrainHeight; y < m_height; ++y) {
            // First 30 pixels below surface is dirt
            if (y < terrainHeight + 30) {
                set(x, y, MaterialType::Dirt);
            } else {
                // Everything deeper is stone
                set(x, y, MaterialType::Stone);
            }
        }
        
        // Add grass on top of terrain
        if (terrainHeight > 0 && terrainHeight < m_height) {
            set(x, terrainHeight - 1, MaterialType::GrassStalks);
        }
    }
    
    // Create multiple lakes and water features
    // Large lake in the center
    int mainLakeWidth = m_width / 3;
    int mainLakeX = (m_width - mainLakeWidth) / 2;
    int mainLakeY = groundLevel - 50; // Just above the ground level
    int mainLakeDepth = 120;
    
    // Add the main lake water
    for (int x = mainLakeX; x < mainLakeX + mainLakeWidth; ++x) {
        // Curved lake bottom with flat parts
        int depth;
        float xRel = (float)(x - mainLakeX) / mainLakeWidth;
        if (xRel < 0.2f || xRel > 0.8f) {
            // Edges are shallower
            depth = mainLakeDepth / 2;
        } else {
            // Center is deeper
            depth = mainLakeDepth;
        }
        
        for (int y = mainLakeY; y < mainLakeY + depth; ++y) {
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
            
            // Only fill where there's no terrain
            if (get(x, y) == MaterialType::Empty || 
                get(x, y) == MaterialType::Dirt ||
                get(x, y) == MaterialType::GrassStalks) {
                set(x, y, MaterialType::Water);
            }
        }
    }
    
    // Add a smaller pond to the left
    int leftPondX = mainLakeX - mainLakeWidth / 2;
    int leftPondY = groundLevel - 30;
    int leftPondWidth = mainLakeWidth / 3;
    int leftPondDepth = 80;
    
    for (int x = leftPondX; x < leftPondX + leftPondWidth; ++x) {
        for (int y = leftPondY; y < leftPondY + leftPondDepth; ++y) {
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
            
            // Only fill empty spaces
            if (get(x, y) == MaterialType::Empty || 
                get(x, y) == MaterialType::Dirt ||
                get(x, y) == MaterialType::GrassStalks) {
                set(x, y, MaterialType::Water);
            }
        }
    }
    
    // Add a smaller pond to the right
    int rightPondX = mainLakeX + mainLakeWidth + mainLakeWidth / 6;
    int rightPondY = groundLevel - 40;
    int rightPondWidth = mainLakeWidth / 2;
    int rightPondDepth = 100;
    
    for (int x = rightPondX; x < rightPondX + rightPondWidth; ++x) {
        for (int y = rightPondY; y < rightPondY + rightPondDepth; ++y) {
            if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
            
            // Only fill empty spaces
            if (get(x, y) == MaterialType::Empty || 
                get(x, y) == MaterialType::Dirt ||
                get(x, y) == MaterialType::GrassStalks) {
                set(x, y, MaterialType::Water);
            }
        }
    }
    
    // Add sand around all water bodies
    // Process the whole world to add sand near water
    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            // Only turn dirt or grass into sand, and only near water
            if (get(x, y) == MaterialType::Dirt || get(x, y) == MaterialType::GrassStalks) {
                // Check if there's water nearby
                bool nearWater = false;
                
                // Check in a 5x5 area for water
                for (int nx = -2; nx <= 2 && !nearWater; ++nx) {
                    for (int ny = -2; ny <= 2 && !nearWater; ++ny) {
                        int checkX = x + nx;
                        int checkY = y + ny;
                        
                        if (checkX >= 0 && checkX < m_width && checkY >= 0 && checkY < m_height) {
                            if (get(checkX, checkY) == MaterialType::Water) {
                                nearWater = true;
                            }
                        }
                    }
                }
                
                // Replace with sand if near water
                if (nearWater) {
                    set(x, y, MaterialType::Sand);
                }
            }
        }
    }
    
    // Add fires around the terrain (equally spaced for better visibility)
    int numFires = 15;
    int fireSpacing = m_width / numFires;
    
    for (int i = 0; i < numFires; ++i) {
        // Place fires more evenly throughout the world
        int fireX = i * fireSpacing + fireSpacing/2;
        
        // Find the surface at this x-coordinate
        int fireY = -1;
        for (int y = 0; y < m_height; ++y) {
            if (get(fireX, y) != MaterialType::Empty) {
                fireY = y - 1; // Just above the surface
                break;
            }
        }
        
        // If we found a surface, place a campfire
        if (fireY > 0) {
            // Create pyramid-shaped campfire
            int fireSize = 8; // Fixed size for consistency
            
            // Create the base of logs (optional)
            for (int x = fireX - fireSize/2; x <= fireX + fireSize/2; ++x) {
                if (x < 0 || x >= m_width) continue;
                // Set the log at surface level
                set(x, fireY + 1, MaterialType::Oil); // Using oil as "wood"
            }
            
            // Create the fire pyramid
            for (int level = 0; level < fireSize; ++level) {
                int width = fireSize - level;
                
                for (int x = fireX - width/2; x <= fireX + width/2; ++x) {
                    int y = fireY - level;
                    if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
                    
                    // Place fire only where there's empty space
                    if (get(x, y) == MaterialType::Empty) {
                        set(x, y, MaterialType::Fire);
                    }
                }
            }
        }
    }
    
    // Add oil deposits underground (more structured)
    int numOilDeposits = 5;
    int oilSpacing = m_width / numOilDeposits;
    
    for (int i = 0; i < numOilDeposits; ++i) {
        // Place oil deposits evenly throughout the world
        int oilX = i * oilSpacing + oilSpacing/3; // Offset to not align with fires
        int oilY = groundLevel + 100; // Deep underground
        
        // Create oval-shaped oil deposits
        int oilWidth = 200; // Larger, more visible deposits
        int oilHeight = 100;
        
        // Create a nice elliptical shape
        for (int x = oilX - oilWidth/2; x <= oilX + oilWidth/2; ++x) {
            for (int y = oilY - oilHeight/2; y <= oilY + oilHeight/2; ++y) {
                if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
                
                // Create an elliptical shape with the equation (x/a)^2 + (y/b)^2 <= 1
                float xRel = (float)(x - oilX) / (oilWidth/2);
                float yRel = (float)(y - oilY) / (oilHeight/2);
                
                if (xRel*xRel + yRel*yRel <= 1.0f) {
                    // Only replace stone with oil
                    if (get(x, y) == MaterialType::Stone) {
                        set(x, y, MaterialType::Oil);
                    }
                }
            }
        }
    }
    
    // Add flammable gas in caves and pockets - more structured
    int numGasCaves = 10;
    for (int i = 0; i < numGasCaves; ++i) {
        // Distribute gas clouds more evenly
        int gasX = m_width * (i + 0.5f) / numGasCaves;
        int gasY = std::uniform_int_distribution<int>(50, groundLevel - 100)(m_rng);
        
        // Create more interesting gas cave shapes
        int mainSize = 80; // Larger, more visible gas pockets
        
        // Create the main gas cloud
        for (int x = gasX - mainSize; x <= gasX + mainSize; ++x) {
            for (int y = gasY - mainSize/2; y <= gasY + mainSize/2; ++y) {
                if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;
                
                // Use noise to create more organic shapes
                // Distance from center + some perlin-like noise
                float dx = x - gasX;
                float dy = y - gasY;
                float distanceSquared = (dx*dx/(mainSize*mainSize) + dy*dy/((mainSize/2)*(mainSize/2)));
                
                // Add some noise based on position
                float noise = sin(x * 0.05f) * sin(y * 0.05f) * 0.3f;
                
                // Fill if within the noisy boundary
                if (distanceSquared + noise < 1.0f && get(x, y) == MaterialType::Empty) {
                    set(x, y, MaterialType::FlammableGas);
                }
            }
        }
        
        // Add some tendrils/extensions to the gas cloud for visual interest
        int numTendrils = 3;
        for (int t = 0; t < numTendrils; ++t) {
            // Pick a random angle
            float angle = std::uniform_real_distribution<float>(0, 6.28f)(m_rng);
            int tendrilLength = std::uniform_int_distribution<int>(40, 100)(m_rng);
            
            // Create the tendril
            for (int step = 0; step < tendrilLength; ++step) {
                int tx = gasX + static_cast<int>(cos(angle) * step);
                int ty = gasY + static_cast<int>(sin(angle) * step);
                
                // Add some wiggle
                tx += sin(step * 0.2f) * 5;
                ty += cos(step * 0.3f) * 3;
                
                if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height) continue;
                
                // Create a blob at this position
                int blobSize = 5 - step / 20; // Gets smaller toward the end
                if (blobSize < 1) blobSize = 1;
                
                for (int bx = tx - blobSize; bx <= tx + blobSize; ++bx) {
                    for (int by = ty - blobSize; by <= ty + blobSize; ++by) {
                        if (bx < 0 || bx >= m_width || by < 0 || by >= m_height) continue;
                        
                        if (get(bx, by) == MaterialType::Empty) {
                            set(bx, by, MaterialType::FlammableGas);
                        }
                    }
                }
            }
        }
    }
    
    // Update pixel data
    updatePixelData();
}

Chunk* World::getChunkAt(int x, int y) {
    if (x < 0 || x >= m_chunksX || y < 0 || y >= m_chunksY) {
        return nullptr;
    }
    
    int idx = y * m_chunksX + x;
    return m_chunks[idx].get();
}

const Chunk* World::getChunkAt(int x, int y) const {
    if (x < 0 || x >= m_chunksX || y < 0 || y >= m_chunksY) {
        return nullptr;
    }
    
    int idx = y * m_chunksX + x;
    return m_chunks[idx].get();
}

void World::worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const {
    chunkX = worldX / Chunk::WIDTH;
    chunkY = worldY / Chunk::HEIGHT;
    localX = worldX % Chunk::WIDTH;
    localY = worldY % Chunk::HEIGHT;
}

void World::updatePixelData() {
    // Update combined pixel data from all chunks
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (!chunk) continue;
            
            // Copy chunk's pixel data to the world's pixel data
            int chunkBaseX = x * Chunk::WIDTH;
            int chunkBaseY = y * Chunk::HEIGHT;
            
            const uint8_t* chunkPixelData = chunk->getPixelData();
            
            for (int cy = 0; cy < Chunk::HEIGHT; ++cy) {
                int worldY = chunkBaseY + cy;
                if (worldY >= m_height) continue;
                
                for (int cx = 0; cx < Chunk::WIDTH; ++cx) {
                    int worldX = chunkBaseX + cx;
                    if (worldX >= m_width) continue;
                    
                    int chunkIdx = (cy * Chunk::WIDTH + cx) * 4;
                    int worldIdx = (worldY * m_width + worldX) * 4;
                    
                    if (worldIdx + 3 < static_cast<int>(m_pixelData.size()) && 
                        chunkIdx + 3 < static_cast<int>(chunk->getPixelData() - chunkPixelData) * 4) {
                        m_pixelData[worldIdx] = chunkPixelData[chunkIdx];        // R
                        m_pixelData[worldIdx+1] = chunkPixelData[chunkIdx+1];    // G
                        m_pixelData[worldIdx+2] = chunkPixelData[chunkIdx+2];    // B
                        m_pixelData[worldIdx+3] = chunkPixelData[chunkIdx+3];    // A
                    }
                }
            }
        }
    }
}

} // namespace PixelPhys
