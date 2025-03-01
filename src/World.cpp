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
