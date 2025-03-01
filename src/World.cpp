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
        
        // Create a position-based variation for more natural look
        // Enhanced variation using multiple hash functions for more texture
        int posHash1 = ((x * 13) + (y * 7)) % 32;
        int posHash2 = ((x * 23) + (y * 17)) % 64;
        int posHash3 = ((x * 5) + (y * 31)) % 16;
        
        // Different variation for each color channel - INCREASED VARIATION
        int rVariation = (posHash1 % 35) - 17;  // Much stronger variation
        int gVariation = (posHash2 % 31) - 15;  // Much stronger variation
        int bVariation = (posHash3 % 27) - 13;  // Much stronger variation
        
        // Apply material-specific variation patterns
        // Various materials have their own unique texture patterns
        switch (material) {
            case MaterialType::Stone:
                // Stone has gray variations with strong texture
                rVariation = gVariation = bVariation = (posHash1 % 45) - 22;
                // Add dark speckles
                if (posHash2 % 5 == 0) {
                    rVariation -= 25;
                    gVariation -= 25;
                    bVariation -= 25;
                }
                break;
            case MaterialType::Grass:
                // Grass has strong green variations with patches
                gVariation = (posHash1 % 50) - 15; // Strong green variation
                rVariation = (posHash2 % 25) - 15; // Variation for yellowish tints
                // Add occasional darker patches
                if (posHash1 % 3 == 0) {
                    gVariation -= 20;
                    rVariation -= 10;
                }
                break;
            case MaterialType::Sand:
                // Sand has strong yellow-brown variations with visible texture
                rVariation = (posHash1 % 30) - 10;
                gVariation = (posHash1 % 25) - 12;
                bVariation = (posHash3 % 15) - 10;
                // Add occasional darker grains
                if (posHash2 % 4 == 0) {
                    rVariation -= 15;
                    gVariation -= 15;
                }
                break;
            case MaterialType::Dirt:
                // Dirt has rich brown variations with texture
                rVariation = (posHash1 % 40) - 15;
                gVariation = (posHash2 % 30) - 15;
                bVariation = (posHash3 % 20) - 12;
                // Add occasional darker and lighter patches
                if (posHash2 % 5 == 0) {
                    rVariation -= 20;
                    gVariation -= 20;
                    bVariation -= 10;
                } else if (posHash2 % 7 == 0) {
                    rVariation += 15;
                    gVariation += 10;
                }
                break;
            case MaterialType::Snow:
                // Snow has very subtle blue-white variations
                rVariation = gVariation = bVariation = (posHash1 % 7) - 3;
                break;
            case MaterialType::Sandstone:
                // Sandstone has beige-tan variations
                rVariation = (posHash1 % 16) - 8;
                gVariation = (posHash2 % 14) - 7;
                bVariation = (posHash3 % 8) - 4;
                break;
            case MaterialType::Bedrock:
                // Bedrock has dark gray variations with some texture
                rVariation = gVariation = bVariation = (posHash1 % 20) - 8;
                // Add some occasional darker spots for texture
                if (posHash2 % 8 == 0) {
                    rVariation -= 10;
                    gVariation -= 10;
                    bVariation -= 10;
                }
                break;
            case MaterialType::Water:
                // Water has blue variations with some subtle waves
                bVariation = (posHash1 % 18) - 9;
                // Slight green tint variations for depth perception
                gVariation = (posHash2 % 10) - 5;
                // Very minimal red variation
                rVariation = (posHash3 % 4) - 2;
                break;
            case MaterialType::Lava:
                // Lava has hot red-orange variations with bright spots
                rVariation = (posHash1 % 30) - 5; // More red, less reduction
                gVariation = (posHash2 % 25) - 15; // More variation in orange
                bVariation = (posHash3 % 6) - 3; // Minor blue variation
                // Add occasional bright yellow-white spots
                if (posHash2 % 10 == 0) {
                    rVariation += 20;
                    gVariation += 15;
                }
                break;
            case MaterialType::GrassStalks:
                // Grass stalks have varied green shades
                gVariation = (posHash1 % 22) - 8; // Strong green variation
                rVariation = (posHash2 % 10) - 5; // Some red variation for yellowish/brownish tints
                bVariation = (posHash3 % 8) - 4; // Minor blue variation
                break;
            case MaterialType::Fire:
                // Fire has flickering yellow-orange-red variations
                rVariation = (posHash1 % 20) - 5; // Strong red
                gVariation = (posHash2 % 30) - 15; // Varied green for yellow/orange
                bVariation = (posHash3 % 10) - 8; // Minimal blue
                // Random bright spots
                if (posHash2 % 5 == 0) {
                    rVariation += 15;
                    gVariation += 10;
                }
                break;
            case MaterialType::Oil:
                // Oil has dark brown-black variations with slight shine
                rVariation = (posHash1 % 12) - 8;
                gVariation = (posHash2 % 10) - 7;
                bVariation = (posHash3 % 8) - 6;
                // Occasional slight shine
                if (posHash2 % 12 == 0) {
                    rVariation += 8;
                    gVariation += 8;
                    bVariation += 8;
                }
                break;
            case MaterialType::FlammableGas:
                // Flammable gas has subtle greenish variations with transparency
                gVariation = (posHash1 % 15) - 5;
                rVariation = (posHash2 % 8) - 4;
                bVariation = (posHash3 % 8) - 4;
                break;
            default:
                // Default variation - still apply some texture for any other materials
                rVariation = (posHash1 % 12) - 6;
                gVariation = (posHash2 % 12) - 6;
                bVariation = (posHash3 % 12) - 6;
                break;
        }
        
        // Apply the enhanced variation to the base color
        int r = props.r + rVariation + props.varR;
        int g = props.g + gVariation + props.varG;
        int b = props.b + bVariation + props.varB;
        
        // Clamp values to valid range
        m_pixelData[pixelIdx] = std::max(0, std::min(255, r));
        m_pixelData[pixelIdx+1] = std::max(0, std::min(255, g));
        m_pixelData[pixelIdx+2] = std::max(0, std::min(255, b));
        m_pixelData[pixelIdx+3] = props.transparency;
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

// Simple Perlin noise implementation
float perlinNoise2D(float x, float y, unsigned int seed) {
    // Hash function for simple pseudo-random gradients
    auto hash = [seed](int x, int y) -> int {
        int h = seed + x * 374761393 + y * 668265263;
        h = (h ^ (h >> 13)) * 1274126177;
        return h ^ (h >> 16);
    };

    // Get gradient at grid point
    auto gradient = [&hash](int ix, int iy, float x, float y) -> float {
        int h = hash(ix, iy);
        // Convert hash to one of 8 unit vectors
        float angle = static_cast<float>(h % 8) * 0.7853981634f; // pi/4
        float gx = std::cos(angle);
        float gy = std::sin(angle);
        // Dot product of distance and gradient
        return gx * (x - static_cast<float>(ix)) + gy * (y - static_cast<float>(iy));
    };

    // Get grid cell coordinates
    int x0 = std::floor(x);
    int y0 = std::floor(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Get distances from grid point
    float dx0 = x - static_cast<float>(x0);
    float dy0 = y - static_cast<float>(y0);
    float dx1 = x - static_cast<float>(x1);
    float dy1 = y - static_cast<float>(y1);

    // Calculate gradients at grid points
    float g00 = gradient(x0, y0, x, y);
    float g10 = gradient(x1, y0, x, y);
    float g01 = gradient(x0, y1, x, y);
    float g11 = gradient(x1, y1, x, y);

    // Cubic interpolation (smootherstep)
    auto interpolate = [](float a, float b, float t) -> float {
        return a + t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f) * (b - a);
    };

    // Interpolate along x
    float x0y0x1y0 = interpolate(g00, g10, dx0);
    float x0y1x1y1 = interpolate(g01, g11, dx0);

    // Interpolate along y
    return interpolate(x0y0x1y0, x0y1x1y1, dy0);
}

// Function to get fractal noise (multiple octaves)
float fractalNoise(float x, float y, int octaves, float persistence, float scale, unsigned int seed) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += perlinNoise2D(x * scale * frequency, y * scale * frequency, seed + i) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    // Normalize
    return total / maxValue;
}

// Define biome types
enum class BiomeType {
    GRASSLAND,
    DESERT,
    MOUNTAIN,
    SNOW,
    JUNGLE
};

void World::generate(unsigned int seed) {
    // Seed the RNG
    m_rng.seed(seed);
    
    std::cout << "Generating world with seed: " << seed << std::endl;
    
    // World generation constants
    const int WORLD_WIDTH = m_width;
    const int WORLD_HEIGHT = m_height;
    int octaves = 4;
    float persistence = 0.5f;
    
    // Biome noise
    float biomeNoiseScale = 0.0005f;
    
    // Clear the world first with empty space
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            set(x, y, MaterialType::Empty);
        }
    }
    
    // Step 1: Generate heightmap using Perlin noise
    std::vector<int> heightMap(WORLD_WIDTH);
    
    // Define biome regions for height generation (same as for biomes)
    const int desertEndX = WORLD_WIDTH / 3;
    const int grasslandEndX = 2 * WORLD_WIDTH / 3;
    const int transitionWidth = WORLD_WIDTH / 100; // Narrow transition zone for sharper biome borders
    
    // Different noise parameters for each biome type
    const float desertNoiseScale = 0.006f;    // Higher frequency, more hills
    const float grasslandNoiseScale = 0.003f; // Lower frequency, gentler hills
    const float mountainNoiseScale = 0.0035f; // Reduced frequency for smoother mountains
    
    // Height scale factors for each biome
    const float desertHeightScale = 0.35f;    // Moderate hills
    const float grasslandHeightScale = 0.25f; // Flatter
    const float mountainHeightScale = 0.7f;   // Taller peaks, but not extreme
    
    // Base height level for each biome (proportion of world height)
    const float desertBaseHeight = 0.3f;      // Medium base height
    const float grasslandBaseHeight = 0.25f;  // Lower base height
    const float mountainBaseHeight = 0.4f;    // Higher base level
    
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        // Determine which biome region this x belongs to
        float currentNoiseScale, heightScale, baseHeight;
        
        if (x < desertEndX - transitionWidth) {
            // Pure desert region
            currentNoiseScale = desertNoiseScale;
            heightScale = desertHeightScale;
            baseHeight = desertBaseHeight;
        } 
        else if (x < desertEndX + transitionWidth) {
            // Desert-grassland transition - interpolate parameters with smooth easing function
            float t = (float)(x - (desertEndX - transitionWidth)) / (2.0f * transitionWidth);
            
            // Apply smooth easing function (cubic) to make transition more natural
            // This is a smooth S-curve transition: t = t^2 * (3 - 2*t)
            t = t * t * (3.0f - 2.0f * t);
            
            currentNoiseScale = desertNoiseScale * (2.0f - t) + grasslandNoiseScale * t;
            heightScale = desertHeightScale * (2.0f - t) + grasslandHeightScale * t;
            baseHeight = desertBaseHeight * (1.0f - t) + grasslandBaseHeight * t;
        }
        else if (x < grasslandEndX - transitionWidth) {
            // Pure grassland region
            currentNoiseScale = grasslandNoiseScale;
            heightScale = grasslandHeightScale;
            baseHeight = grasslandBaseHeight;
        }
        else if (x < grasslandEndX + transitionWidth) {
            // Grassland-mountain transition - interpolate parameters
            float t = (float)(x - (grasslandEndX - transitionWidth)) / (2.0f * transitionWidth);
            
            // Apply smooth easing function (cubic) to make transition more natural
            // This is a smooth S-curve transition: t = t^2 * (3 - 2*t)
            t = t * t * (3.0f - 2.0f * t);
            
            currentNoiseScale = grasslandNoiseScale * (1.0f - t) + mountainNoiseScale * t;
            heightScale = grasslandHeightScale * (1.0f - t) + mountainHeightScale * t;
            baseHeight = grasslandBaseHeight * (1.0f - t) + mountainBaseHeight * t;
        }
        else {
            // Pure mountain region
            currentNoiseScale = mountainNoiseScale;
            heightScale = mountainHeightScale;
            baseHeight = mountainBaseHeight;
        }
        
        // Generate fractal noise with biome-specific parameters
        // Use more octaves for mountains to get smoother large shapes with finer details
        int biomeOctaves = octaves;
        if (currentNoiseScale == mountainNoiseScale) {
            biomeOctaves = 6; // More octaves for mountains
        }
        
        float height = fractalNoise(static_cast<float>(x), 0.0f, biomeOctaves, persistence, currentNoiseScale, seed);
        
        // Apply biome-specific scaling
        height = (height * 0.5f + 0.5f) * (WORLD_HEIGHT * heightScale);
        
        // Add biome-specific base level
        heightMap[x] = static_cast<int>(height) + static_cast<int>(WORLD_HEIGHT * baseHeight);
    }
    
    // Step 2: Generate biome map - based primarily on X position for the three main biomes
    std::vector<BiomeType> biomeMap(WORLD_WIDTH);
    
    // Reuse the same region constants from step 1 for biome assignment
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        float biomeNoise = perlinNoise2D(x * biomeNoiseScale, 42.0f, seed + 123);
        
        // Base biome by X position
        BiomeType baseBiome;
        
        if (x < desertEndX - transitionWidth) {
            // Pure desert region
            baseBiome = BiomeType::DESERT;
        } 
        else if (x < desertEndX + transitionWidth) {
            // Desert-to-grassland transition zone
            // Calculate smooth transition factor
            float t = (float)(x - (desertEndX - transitionWidth)) / (2.0f * transitionWidth);
            t = t * t * (3.0f - 2.0f * t); // Smooth cubic easing
            
            // Use blend of noise and position to determine the biome
            // As we get closer to grassland region, the probability of grassland increases
            float threshold = -0.5f + t; // Threshold shifts from -0.5 to 0.5 across transition
            baseBiome = (biomeNoise < threshold) ? BiomeType::DESERT : BiomeType::GRASSLAND;
        }
        else if (x < grasslandEndX - transitionWidth) {
            // Pure grassland region
            baseBiome = BiomeType::GRASSLAND;
        }
        else if (x < grasslandEndX + transitionWidth) {
            // Grassland-to-mountain transition zone
            // Calculate smooth transition factor
            float t = (float)(x - (grasslandEndX - transitionWidth)) / (2.0f * transitionWidth);
            t = t * t * (3.0f - 2.0f * t); // Smooth cubic easing
            
            // Use blend of noise and position to determine the biome
            // As we get closer to mountain region, the probability of mountains increases
            float threshold = -0.5f + t; // Threshold shifts from -0.5 to 0.5 across transition
            baseBiome = (biomeNoise < threshold) ? BiomeType::GRASSLAND : BiomeType::MOUNTAIN;
        }
        else {
            // Pure mountain region
            baseBiome = BiomeType::MOUNTAIN;
        }
        
        // Mountain peaks can appear anywhere if height is very high
        if (heightMap[x] > 0.9f * WORLD_HEIGHT) {
            biomeMap[x] = BiomeType::MOUNTAIN;
        }
        // Snow caps appear on high locations in mountain biome
        else if (baseBiome == BiomeType::MOUNTAIN && heightMap[x] > 0.75f * WORLD_HEIGHT) {
            biomeMap[x] = BiomeType::SNOW;
        }
        // Jungle can appear in grassland areas with proper noise value
        else if (baseBiome == BiomeType::GRASSLAND && biomeNoise > 0.4f) {
            biomeMap[x] = BiomeType::JUNGLE;
        }
        else {
            biomeMap[x] = baseBiome;
        }
    }
    
    // Step 3: Fill terrain with blocks based on height and biome
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        int terrainHeight = heightMap[x];
        BiomeType biome = biomeMap[x];
        
        // Invert the terrain height to make 0 the bottom of the world and WORLD_HEIGHT the top
        // This ensures the ground is at the bottom and the sky is at the top
        terrainHeight = WORLD_HEIGHT - terrainHeight;
        
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            if (y < terrainHeight) {
                // Below ground is empty (air)
                continue; // Already filled with Empty
            } else if (y == terrainHeight) {
                // Surface block at the ground level
                switch (biome) {
                    case BiomeType::DESERT:
                        set(x, y, MaterialType::Sand);
                        break;
                    case BiomeType::SNOW:
                        set(x, y, MaterialType::Snow);
                        break;
                    case BiomeType::MOUNTAIN:
                        set(x, y, MaterialType::Stone);
                        break;
                    case BiomeType::JUNGLE:
                    case BiomeType::GRASSLAND:
                        set(x, y, MaterialType::Grass);
                        break;
                }
            } else {
                // Above the surface (underground)
                int depth = y - terrainHeight; // How deep we are from the surface
                
                if (depth < 40) {
                    // Just below surface
                    if (biome == BiomeType::DESERT) {
                        set(x, y, MaterialType::Sand);
                    } else if (biome == BiomeType::SNOW) {
                        set(x, y, MaterialType::Snow);
                    } else {
                        set(x, y, MaterialType::Dirt);
                    }
                } else if (y > 0.9 * WORLD_HEIGHT) {
                    // Top 10% of world (was bottom, now top due to inversion)
                    set(x, y, MaterialType::Bedrock);
                } else {
                    // Default underground material
                    if (biome == BiomeType::DESERT) {
                        set(x, y, MaterialType::Sandstone);
                    } else {
                        set(x, y, MaterialType::Stone);
                    }
                }
            }
        }
    }
    
    std::cout << "Basic terrain generation complete." << std::endl;
    
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
