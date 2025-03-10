#include "../include/World.h"
#include <cmath>

namespace PixelPhys {

ChunkManager::ChunkManager(int chunkSize) : m_chunkSize(chunkSize) {
    // Initialize with no active chunks
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
    
    // Create a new chunk since it's not loaded yet
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
    
    // Calculate view distance (chunks visible from player)
    // For 12 chunks in a pattern, we need approximately 3x4 grid
    const int viewDistance = 2; // 5x5 grid = 25 chunks, but we'll keep only 12
    
    // Collect coordinates of chunks that should be loaded
    for (int y = centerChunkY - viewDistance; y <= centerChunkY + viewDistance; y++) {
        for (int x = centerChunkX - viewDistance; x <= centerChunkX + viewDistance; x++) {
            desiredChunks.push_back({x, y});
        }
    }
    
    // Sort chunks by distance to center (closest first)
    std::sort(desiredChunks.begin(), desiredChunks.end(),
        [this, centerX, centerY](const ChunkCoord& a, const ChunkCoord& b) {
            return calculateChunkDistance(a, centerX, centerY) < 
                   calculateChunkDistance(b, centerX, centerY);
        });
    
    // Keep only closest MAX_LOADED_CHUNKS
    if (desiredChunks.size() > MAX_LOADED_CHUNKS) {
        desiredChunks.resize(MAX_LOADED_CHUNKS);
    }
    
    // For initial implementation, we keep all chunks loaded but mark which ones are active
    m_activeChunks = desiredChunks;
    
    // Output active chunks for debugging
    std::cout << "Active chunks: ";
    for (const auto& coord : m_activeChunks) {
        std::cout << "(" << coord.x << "," << coord.y << ") ";
    }
    std::cout << std::endl;
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
    std::cout << "Creating chunk at position (" << posX << "," << posY << ")" << std::endl;
    return std::make_unique<Chunk>(posX, posY);
}

} // namespace PixelPhys