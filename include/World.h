#pragma once

#include "Materials.h"
#include <vector>
#include <memory>
#include <random>
#include <cstdint>
#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm> // for std::find, std::sort

namespace PixelPhys {

// Define biome types for ore generation and world generation
enum class BiomeType {
    GRASSLAND,
    DESERT,
    MOUNTAIN,
    SNOW,
    JUNGLE
};

// Structure to represent chunk coordinates for the streaming system
struct ChunkCoord {
    int x;
    int y;
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y;
    }
};

// Hash function for ChunkCoord to use in unordered_map/set
struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& coord) const {
        return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.y) << 1);
    }
};

// Forward declarations
class Chunk;
class ChunkManager;

// A chunk is a fixed-size part of the world
// Using a chunk-based approach allows easier multithreading and memory management
class Chunk {
public:
    // Fixed size for each chunk - for better aligned ore generation
    static constexpr int WIDTH = 512;   // Slightly larger chunks for more coherent ore patterns
    static constexpr int HEIGHT = 512;
    
    Chunk(int posX = 0, int posY = 0);
    ~Chunk() = default;
    
    // Store chunk position in world coordinates for pixel-perfect alignment
    int m_posX;
    int m_posY;
    
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
    
    // Check if this chunk needs updating next frame
    bool shouldUpdateNextFrame() const { return m_shouldUpdateNextFrame; }
    
    // Mark this chunk for update next frame
    void setShouldUpdateNextFrame(bool update) { m_shouldUpdateNextFrame = update; }
    
    // Get inactivity counter
    int getInactivityCounter() const { return m_inactivityCounter; }
    
    // Set free falling state for a specific cell
    void setFreeFalling(int idx, bool falling) { 
        if (idx >= 0 && idx < static_cast<int>(m_isFreeFalling.size())) {
            m_isFreeFalling[idx] = falling; 
        }
    }
    
    // Get raw pixel data for rendering
    uint8_t* getPixelData() { return m_pixelData.data(); }
    
    // Update rendering pixel data based on materials
    void updatePixelData();
    
    // Serialization methods for streaming system (will be implemented later)
    bool serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);
    
    // Check if the chunk has been modified since last save
    bool isModified() const { return m_isModified; }
    
    // Set modified flag
    void setModified(bool modified) { m_isModified = modified; }
    
private:
    // Grid of materials in the chunk
    std::vector<MaterialType> m_grid;
    
    // For rendering: RGBA pixel data (r,g,b,a for each cell)
    std::vector<uint8_t> m_pixelData;
    
    // Flag to track if this chunk has been modified since last save
    bool m_isModified = false;
    
    // Flag to indicate if this chunk needs updating this frame
    bool m_isDirty;
    
    // Flag to indicate if this chunk should be updated next frame
    bool m_shouldUpdateNextFrame;
    
    // Counter to track how many frames a chunk has been inactive
    int m_inactivityCounter;
    
    // Handle interactions between different materials (fire spreading, etc.)
    void handleMaterialInteractions(const std::vector<MaterialType>& oldGrid, bool& anyMaterialMoved);
    
    // Helper to count water pixels below current position (for depth-based effects)
    int countWaterBelow(int x, int y) const;
    
    // For random number generation in material interactions
    std::mt19937 m_rng{std::random_device{}()};
    
    // Helper for determining if a material can fall into another material
    bool canDisplace(MaterialType above, MaterialType below) const;
    
    // Helpers for liquid dynamics
    bool isNotIsolatedLiquid(const std::vector<MaterialType>& grid, int x, int y);
    
    // Track if an element is currently in motion (for sand inertia)
    std::vector<bool> m_isFreeFalling;
};

// Chunk streaming system
class ChunkManager {
public:
    ChunkManager(int chunkSize = 512);
    ~ChunkManager();
    
    // Core chunk operations
    Chunk* getChunk(int chunkX, int chunkY, bool loadIfNeeded = true);
    void updateActiveChunks(int centerX, int centerY);
    void update();
    
    // Save all modified chunks
    void saveAllModifiedChunks();
    
    // Visibility checker
    bool isChunkVisible(int chunkX, int chunkY, int cameraX, int cameraY, int screenWidth, int screenHeight) const;
    
    // Get active chunks for rendering
    const std::vector<ChunkCoord>& getActiveChunks() const { return m_activeChunks; }
    
    // Chunk file operations
    bool saveChunk(const ChunkCoord& coord);
    std::unique_ptr<Chunk> loadChunk(const ChunkCoord& coord);
    bool isChunkLoaded(const ChunkCoord& coord) const;
    
    // Check if chunk exists on disk
    bool chunkExistsOnDisk(const ChunkCoord& coord) const;
    
    // Get path for chunk file
    std::string getChunkFilePath(const ChunkCoord& coord) const;
    
    // Convert world to chunk coordinates
    void worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const;
    
private:
    // Map of loaded chunks
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> m_loadedChunks;
    
    // Set of modified chunks that need saving
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_dirtyChunks;
    
    // Recently unloaded chunks for caching (to avoid excessive file I/O)
    struct CachedChunk {
        std::unique_ptr<Chunk> chunk;
        int frameUnloaded;
    };
    std::unordered_map<ChunkCoord, CachedChunk, ChunkCoordHash> m_chunkCache;
    int m_currentFrame = 0;
    const int CACHE_TTL = 600; // Cache chunks for 10 seconds (assuming 60 FPS)
    
    // Currently active chunk coordinates
    std::vector<ChunkCoord> m_activeChunks;
    
    // Maximum number of chunks to keep loaded - matches Noita's approach in GDC talk
    const int MAX_LOADED_CHUNKS = 12; // Exactly 12 chunks as specified in the streaming design
    
    // Size of chunks in world units
    const int m_chunkSize;
    
    // Base folder for chunk storage
    std::string m_chunkStoragePath;
    
    // Calculate distance between chunk and player
    float calculateChunkDistance(const ChunkCoord& coord, int centerX, int centerY) const;
    
    // Create new chunk
    std::unique_ptr<Chunk> createNewChunk(const ChunkCoord& coord);
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
    
    // Get material type for rendering with shader-based effects
    MaterialType getMaterialAt(int x, int y) const { return get(x, y); }
    
    // Update the entire world's physics
    void update();
    
    // Update only a specific region of the world (optimization for large worlds)
    void update(int startX, int startY, int endX, int endY);
    
    // Special processing to level out liquids (fix floating particles)
    void levelLiquids();
    
    // Level liquids in specific region (optimization for large worlds)
    void levelLiquids(int startX, int startY, int endX, int endY);
    
    // Generate the initial world with terrain, etc.
    void generate(unsigned int seed);
    
    // Update active chunks based on camera position
    void updatePlayerPosition(int playerX, int playerY) {
        m_chunkManager.updateActiveChunks(playerX, playerY);
    }
    
    // Get the list of active chunks for rendering
    const std::vector<ChunkCoord>& getActiveChunks() const {
        return m_chunkManager.getActiveChunks();
    }
    
    // Save all modified chunks to disk
    void save() {
        m_chunkManager.saveAllModifiedChunks();
    }
    
    // Get a chunk at specific chunk coordinates
    Chunk* getChunkByCoords(int chunkX, int chunkY) {
        return m_chunkManager.getChunk(chunkX, chunkY);
    }
    
    // Check if a chunk is visible from the camera
    bool isChunkVisible(int chunkX, int chunkY, int cameraX, int cameraY, int screenWidth, int screenHeight) const {
        return m_chunkManager.isChunkVisible(chunkX, chunkY, cameraX, cameraY, screenWidth, screenHeight);
    }
    
    // Get the raw pixel data for rendering
    uint8_t* getPixelData() { 
        if (m_pixelData.empty()) {
            // std::cout << "WARNING: World::getPixelData - pixel data array is empty!" << std::endl;
            return nullptr;
        }
        return m_pixelData.data();
    }
    const uint8_t* getPixelData() const { 
        if (m_pixelData.empty()) {
            // std::cout << "WARNING: World::getPixelData const - pixel data array is empty!" << std::endl;
            return nullptr;
        }
        return m_pixelData.data(); 
    }
    
private:
    // World dimensions in pixels
    int m_width;
    int m_height;
    
    // Dimensions in chunks
    int m_chunksX;
    int m_chunksY;
    
    // Chunk manager for streaming system
    ChunkManager m_chunkManager;
    
    // Legacy vector of chunks (will be phased out)
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    
    // Optimization: Track dirty chunks for more efficient updates
    const int PROCESSING_CHUNK_SIZE = 64; // Pixels per processing chunk (smaller than storage chunks)
    std::vector<std::pair<int, int>> m_dirtyChunks; // Processing chunks that need updates
    
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
    void updateWorldPixelData();
    
    // World generation helper functions
    void generateTerrain();
    // Ore generation helper functions
    void generateOreVein(int startX, int startY, MaterialType oreType, int maxSize, float density, int maxRadius, BiomeType biome = BiomeType::GRASSLAND);
    void generateOreVeinBranch(int startX, int startY, MaterialType oreType, int maxSize, float density, int maxRadius, float startAngle, BiomeType biome = BiomeType::GRASSLAND);
    void placeOreCluster(int centerX, int centerY, MaterialType oreType, int radius, float density);
    bool isValidPosition(int x, int y) const;
    bool isValidPositionForOre(int x, int y) const;
};

} // namespace PixelPhys