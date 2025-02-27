#pragma once

#include "Materials.h"
#include <vector>
#include <memory>
#include <random>
#include <cstdint>

namespace PixelPhys {

// A chunk is a fixed-size part of the world
// Using a chunk-based approach allows easier multithreading and memory management
class Chunk {
public:
    // Fixed size for each chunk
    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 128;
    
    Chunk();
    ~Chunk() = default;
    
    // Get material at the given position within this chunk
    MaterialType get(int x, int y) const;
    
    // Set material at the given position within this chunk
    void set(int x, int y, MaterialType material);
    
    // Update physics for this chunk (will be called every frame)
    // Needs access to surrounding chunks for proper boundary physics
    void update(Chunk* chunkBelow, Chunk* chunkLeft, Chunk* chunkRight);
    
    // Check if this chunk needs updating (has active materials)
    bool isDirty() const { return m_isDirty; }
    
    // Mark this chunk as needing update
    void setDirty(bool dirty) { m_isDirty = dirty; }
    
    // Get raw pixel data for rendering
    uint8_t* getPixelData() { return m_pixelData.data(); }
    const uint8_t* getPixelData() const { return m_pixelData.data(); }
    
private:
    // Grid of materials in the chunk
    std::vector<MaterialType> m_grid;
    
    // For rendering: RGBA pixel data (r,g,b,a for each cell)
    std::vector<uint8_t> m_pixelData;
    
    // Flag to indicate if this chunk needs updating
    bool m_isDirty;
    
    // Update rendering pixel data based on materials
    void updatePixelData();
};

// The World manages a collection of chunks that make up the entire simulation
class World {
public:
    World(int width, int height);
    ~World() = default;
    
    // Get world dimensions in pixels
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Get chunk dimensions and counts for optimization
    int getChunkWidth() const { return Chunk::WIDTH; }
    int getChunkHeight() const { return Chunk::HEIGHT; }
    int getChunksX() const { return m_chunksX; }
    int getChunksY() const { return m_chunksY; }
    
    // Get/Set material at world coordinates
    MaterialType get(int x, int y) const;
    void set(int x, int y, MaterialType material);
    
    // Update the entire world's physics
    void update();
    
    // Generate the initial world with terrain, etc.
    void generate(unsigned int seed);
    
    // Get the raw pixel data for rendering
    uint8_t* getPixelData() { return m_pixelData.data(); }
    const uint8_t* getPixelData() const { return m_pixelData.data(); }
    
private:
    // World dimensions in pixels
    int m_width;
    int m_height;
    
    // Dimensions in chunks
    int m_chunksX;
    int m_chunksY;
    
    // Grid of chunks
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    
    // For rendering: RGBA pixel data for the entire world
    std::vector<uint8_t> m_pixelData;
    
    // Random number generator
    std::mt19937 m_rng;
    
    // Helper functions
    Chunk* getChunkAt(int x, int y);
    const Chunk* getChunkAt(int x, int y) const;
    
    // Convert between world and chunk coordinates
    void worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const;
    
    // Update the combined pixel data from all chunks for rendering
    void updatePixelData();
    
    // World generation helper functions
    void generateTerrain();
    void generateCaves();
    void generateWaterPools();
    void generateMaterialDeposits();
};

} // namespace PixelPhys