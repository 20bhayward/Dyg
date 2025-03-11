#include "../include/World.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>

namespace PixelPhys {

ChunkManager::ChunkManager(int chunkSize) : m_chunkSize(chunkSize) {
    // Initialize chunk storage path
    m_chunkStoragePath = "world_data/chunks";
    
    // Create the base directory structure if it doesn't exist
    std::filesystem::create_directories(m_chunkStoragePath);
}

ChunkManager::~ChunkManager() {
    // Save all modified chunks before destroying the manager
    saveAllModifiedChunks();
}

Chunk* ChunkManager::getChunk(int chunkX, int chunkY, bool loadIfNeeded) {
    ChunkCoord coord{chunkX, chunkY};
    
    auto it = m_loadedChunks.find(coord);
    if (it != m_loadedChunks.end()) {
        return it->second.get();
    }
    
    if (!loadIfNeeded) {
        return nullptr;
    }
    
    // Check if the chunk is in the cache
    auto cacheIt = m_chunkCache.find(coord);
    if (cacheIt != m_chunkCache.end()) {
        // Move from cache to active chunks
        auto* chunkPtr = cacheIt->second.chunk.get();
        m_loadedChunks[coord] = std::move(cacheIt->second.chunk);
        m_chunkCache.erase(cacheIt);
        return chunkPtr;
    }
    
    // Check if the chunk exists on disk
    if (chunkExistsOnDisk(coord)) {
        // Load from disk
        auto loadedChunk = loadChunk(coord);
        if (loadedChunk) {
            auto* chunkPtr = loadedChunk.get();
            m_loadedChunks[coord] = std::move(loadedChunk);
            return chunkPtr;
        }
    }
    
    // Create a new chunk since it's not on disk or couldn't be loaded
    auto newChunk = createNewChunk(coord);
    auto* chunkPtr = newChunk.get();
    m_loadedChunks[coord] = std::move(newChunk);
    return chunkPtr;
}

void ChunkManager::updateActiveChunks(int centerX, int centerY) {
    // Convert center position to chunk coordinates
    int centerChunkX, centerChunkY, localX, localY;
    worldToChunkCoords(centerX, centerY, centerChunkX, centerChunkY, localX, localY);
    
    // Determine which chunks should be active (in a square around center)
    std::vector<ChunkCoord> desiredChunks;
    
    // For a more balanced distribution of chunks around the player,
    // we'll use an approach that ensures the player is in the center
    
    // The player's chunk is always included
    desiredChunks.push_back({centerChunkX, centerChunkY});
    
    // We want to ensure the 8 chunks immediately surrounding the player are loaded
    for (int y = centerChunkY - 1; y <= centerChunkY + 1; y++) {
        for (int x = centerChunkX - 1; x <= centerChunkX + 1; x++) {
            // Skip the center chunk (already added)
            if (x == centerChunkX && y == centerChunkY) continue;
            desiredChunks.push_back({x, y});
        }
    }
    
    // Then add chunks in the next ring outward (partial ring to reach 12 total)
    // These are the chunks that are at distance 2 from the center
    // We'll add them in spiral order to ensure a balanced distribution
    std::vector<ChunkCoord> outerRing = {
        {centerChunkX - 2, centerChunkY - 1}, // Left edge middle
        {centerChunkX - 2, centerChunkY}, 
        {centerChunkX - 2, centerChunkY + 1},
        {centerChunkX - 1, centerChunkY - 2}, // Top edge left
        {centerChunkX, centerChunkY - 2},     // Top edge center
        {centerChunkX + 1, centerChunkY - 2}, // Top edge right
        {centerChunkX + 2, centerChunkY - 1}, // Right edge top
        {centerChunkX + 2, centerChunkY},
        {centerChunkX + 2, centerChunkY + 1},
        {centerChunkX - 1, centerChunkY + 2}, // Bottom edge left
        {centerChunkX, centerChunkY + 2},     // Bottom edge center
        {centerChunkX + 1, centerChunkY + 2}  // Bottom edge right
    };
    
    // Add enough from the outer ring to reach MAX_LOADED_CHUNKS
    for (const auto& coord : outerRing) {
        desiredChunks.push_back(coord);
        if (desiredChunks.size() >= MAX_LOADED_CHUNKS) break;
    }
    
    // No need to sort, our pattern ensures the closest chunks are loaded
    // and we've already limited to exactly MAX_LOADED_CHUNKS
    
    // Find chunks that are no longer in the active set and need to be unloaded
    std::vector<ChunkCoord> chunksToUnload;
    for (const auto& pair : m_loadedChunks) {
        const ChunkCoord& coord = pair.first;
        // If the chunk is not in the desired list, unload it
        if (std::find(desiredChunks.begin(), desiredChunks.end(), coord) == desiredChunks.end()) {
            chunksToUnload.push_back(coord);
        }
    }
    
    // Save and move chunks to cache instead of unloading immediately
    for (const ChunkCoord& coord : chunksToUnload) {
        // Save if modified
        if (m_loadedChunks[coord]->isModified()) {
            saveChunk(coord);
        }
        
        // Move to cache instead of erasing
        m_chunkCache[coord] = {std::move(m_loadedChunks[coord]), m_currentFrame};
        m_loadedChunks.erase(coord);
    }
    
    // Increment frame counter for cache aging
    m_currentFrame++;
    
    // Load new chunks that aren't already loaded
    for (const ChunkCoord& coord : desiredChunks) {
        if (m_loadedChunks.count(coord) == 0) {
            getChunk(coord.x, coord.y);  // This will load or create the chunk
        }
    }
    
    // Update active chunks list
    m_activeChunks = desiredChunks;
    
    // Only output active chunks when they change
    static std::vector<ChunkCoord> lastActiveChunks;
    if (lastActiveChunks != m_activeChunks) {
        lastActiveChunks = m_activeChunks;
        // Output only occasionally to reduce console spam
        static int debugCounter = 0;
        if (debugCounter++ % 60 == 0) {
            // std::cout << "Active chunks: ";
            for (const auto& coord : m_activeChunks) {
                // std::cout << "(" << coord.x << "," << coord.y << ") ";
            }
            // std::cout << std::endl;
        }
    }
}

void ChunkManager::update() {
    // Track which chunks need saving
    for (const auto& pair : m_loadedChunks) {
        if (pair.second->isModified()) {
            m_dirtyChunks.insert(pair.first);
        }
    }
    
    // Clean up old chunks from cache every 300 frames (about 5 seconds at 60 FPS)
    if (m_currentFrame % 300 == 0) {
        std::vector<ChunkCoord> chunksToRemove;
        
        for (const auto& cachePair : m_chunkCache) {
            if (m_currentFrame - cachePair.second.frameUnloaded > CACHE_TTL) {
                chunksToRemove.push_back(cachePair.first);
            }
        }
        
        for (const auto& coord : chunksToRemove) {
            m_chunkCache.erase(coord);
        }
    }
}

void ChunkManager::saveAllModifiedChunks() {
    for (const auto& coord : m_dirtyChunks) {
        saveChunk(coord);
    }
    m_dirtyChunks.clear();
}

bool ChunkManager::isChunkVisible(int chunkX, int chunkY, int cameraX, int cameraY, 
                                 int screenWidth, int screenHeight) const {
    // Calculate chunk boundaries in world coordinates
    int chunkWorldX = chunkX * m_chunkSize;
    int chunkWorldY = chunkY * m_chunkSize;
    
    // Define camera view boundaries
    int cameraRight = cameraX + screenWidth;
    int cameraBottom = cameraY + screenHeight;
    
    // Check if chunk is within camera view (with some margin)
    return (chunkWorldX + m_chunkSize >= cameraX && chunkWorldX < cameraRight &&
            chunkWorldY + m_chunkSize >= cameraY && chunkWorldY < cameraBottom);
}

bool ChunkManager::isChunkLoaded(const ChunkCoord& coord) const {
    return m_loadedChunks.find(coord) != m_loadedChunks.end();
}

bool ChunkManager::chunkExistsOnDisk(const ChunkCoord& coord) const {
    std::string filePath = getChunkFilePath(coord);
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

std::string ChunkManager::getChunkFilePath(const ChunkCoord& coord) const {
    // Create multi-level directory structure to avoid too many files in one folder
    // Format: chunks/x/y.chunk where x and y are chunk coordinates
    std::ostringstream oss;
    oss << m_chunkStoragePath << "/";
    oss << coord.x << "/";
    return oss.str() + std::to_string(coord.y) + ".chunk";
}

bool ChunkManager::saveChunk(const ChunkCoord& coord) {
    auto it = m_loadedChunks.find(coord);
    if (it == m_loadedChunks.end()) {
        return false; // Chunk not loaded
    }
    
    const Chunk* chunk = it->second.get();
    
    // Skip if not modified
    if (!chunk->isModified()) {
        return true;
    }
    
    // Create directory structure if needed
    std::string filePath = getChunkFilePath(coord);
    std::string dirPath = filePath.substr(0, filePath.find_last_of("/"));
    std::filesystem::create_directories(dirPath);
    
    // Open file for writing
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return false;
    }
    
    // Serialize chunk data
    bool success = chunk->serialize(file);
    
    // Remove from dirty list if success
    if (success) {
        m_dirtyChunks.erase(coord);
    }
    
    return success;
}

std::unique_ptr<Chunk> ChunkManager::loadChunk(const ChunkCoord& coord) {
    std::string filePath = getChunkFilePath(coord);
    
    // Open file for reading
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open chunk file: " << filePath << std::endl;
        return nullptr; // File not found or can't be opened
    }
    
    // Create new chunk with correct position
    int posX = coord.x * m_chunkSize;
    int posY = coord.y * m_chunkSize;
    auto chunk = std::make_unique<Chunk>(posX, posY);
    
    // Deserialize chunk data
    if (!chunk->deserialize(file)) {
        std::cerr << "Failed to deserialize chunk from file: " << filePath << std::endl;
        return nullptr; // Failed to deserialize
    }
    
    // Reduce logging to improve performance
    static int loadCounter = 0;
    if (loadCounter++ % 10 == 0) {
        // std::cout << "Loaded chunk from " << filePath << std::endl;
    }
    return chunk;
}

void ChunkManager::worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, 
                                     int& localX, int& localY) const {
    // Handle negative coordinates properly
    if (worldX < 0) {
        chunkX = (worldX + 1) / m_chunkSize - 1;
    } else {
        chunkX = worldX / m_chunkSize;
    }
    
    if (worldY < 0) {
        chunkY = (worldY + 1) / m_chunkSize - 1;
    } else {
        chunkY = worldY / m_chunkSize;
    }
    
    // Calculate local coordinates within the chunk
    localX = worldX - (chunkX * m_chunkSize);
    localY = worldY - (chunkY * m_chunkSize);
}

float ChunkManager::calculateChunkDistance(const ChunkCoord& coord, int centerX, int centerY) const {
    // Calculate world coordinates of chunk center
    int chunkCenterX = (coord.x * m_chunkSize) + (m_chunkSize / 2);
    int chunkCenterY = (coord.y * m_chunkSize) + (m_chunkSize / 2);
    
    // Calculate Euclidean distance
    float dx = static_cast<float>(chunkCenterX - centerX);
    float dy = static_cast<float>(chunkCenterY - centerY);
    return std::sqrt(dx*dx + dy*dy);
}

std::unique_ptr<Chunk> ChunkManager::createNewChunk(const ChunkCoord& coord) {
    // Create a chunk at the world position
    int posX = coord.x * Chunk::WIDTH;
    int posY = coord.y * Chunk::HEIGHT;
    // Reduce logging to improve performance
    static int createCounter = 0;
    if (createCounter++ % 10 == 0) {
        // std::cout << "Creating new chunk at position (" << posX << "," << posY << ")" << std::endl;
    }
    return std::make_unique<Chunk>(posX, posY);
}

} // namespace PixelPhys