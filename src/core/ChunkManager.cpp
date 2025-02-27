#include "core/ChunkManager.h"
#include "core/World.h"
#include "generation/TerrainGenerator.h"
#include "generation/CaveGenerator.h"
#include "generation/BiomeGenerator.h"
#include "generation/StructureGenerator.h"
#include "util/FileIO.h"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <cmath>

namespace dyg {
namespace core {

ChunkManager::ChunkManager(World& world, const util::Config& config)
    : world(world)
    , config(config) {
}

void ChunkManager::updateChunks(const util::Vector3& playerPos, util::ThreadPool& threadPool) {
    // Convert player position to chunk coordinates
    util::Vector3 playerChunkPos = worldToChunkPos(playerPos);
    
    // Get the pattern of chunks to load around the player
    std::vector<util::Vector3> loadPattern = calculateSpiralPattern(config.viewDistance);
    
    // Lock the chunks map to prevent concurrent modifications
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    // Request chunks in the loading pattern
    for (const auto& offset : loadPattern) {
        util::Vector3 chunkPos = playerChunkPos + offset;
        
        // If the chunk is not already loaded or pending, request it
        if (chunks.find(chunkPos) == chunks.end() && 
            pendingChunks.find(chunkPos) == pendingChunks.end()) {
            requestChunk(chunkPos, threadPool);
        }
    }
    
    // Add chunks to unload queue if they're too far from the player
    for (const auto& [pos, chunk] : chunks) {
        int dx = std::abs(pos.x - playerChunkPos.x);
        int dz = std::abs(pos.z - playerChunkPos.z);
        
        if (dx > config.viewDistance + 1 || dz > config.viewDistance + 1) {
            unloadQueue.push(pos);
        }
    }
    
    // Process some unload queue entries
    const int maxUnloadsPerFrame = 5;
    for (int i = 0; i < maxUnloadsPerFrame && !unloadQueue.empty(); ++i) {
        util::Vector3 pos = unloadQueue.front();
        unloadQueue.pop();
        
        unloadChunk(pos);
    }
}

ChunkPtr ChunkManager::getChunk(const util::Vector3& chunkPos) const {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    auto it = chunks.find(chunkPos);
    if (it != chunks.end()) {
        return it->second;
    }
    
    return nullptr;
}

void ChunkManager::requestChunk(const util::Vector3& chunkPos, util::ThreadPool& threadPool) {
    // Start a task to generate or load the chunk
    std::future<ChunkPtr> future = threadPool.enqueue([this, chunkPos]() {
        // Check if the chunk exists on disk
        std::string filePath = getChunkFilePath(chunkPos);
        if (util::FileIO::fileExists(filePath)) {
            return loadChunk(chunkPos);
        } else {
            return generateChunk(chunkPos);
        }
    });
    
    // Add the pending chunk to the map
    pendingChunks[chunkPos] = std::move(future);
}

int ChunkManager::processCompletedChunks() {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    int processed = 0;
    std::vector<util::Vector3> completedPositions;
    
    // Check each pending chunk
    for (auto& [pos, future] : pendingChunks) {
        // Check if the task is complete without blocking
        if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                // Get the chunk
                ChunkPtr chunk = future.get();
                
                if (chunk) {
                    // Add the chunk to the map
                    chunks[pos] = chunk;
                    processed++;
                }
                
                // Mark this position as completed
                completedPositions.push_back(pos);
            } catch (const std::exception& e) {
                std::cerr << "Error processing chunk at position ("
                          << pos.x << ", " << pos.y << ", " << pos.z
                          << "): " << e.what() << std::endl;
                
                // Mark as completed to remove from pending list
                completedPositions.push_back(pos);
            }
        }
    }
    
    // Remove completed positions from the pending map
    for (const auto& pos : completedPositions) {
        pendingChunks.erase(pos);
    }
    
    return processed;
}

int ChunkManager::saveChunks() {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    int saved = 0;
    
    // Save all dirty chunks
    for (auto& [pos, chunk] : chunks) {
        if (chunk->isDirty()) {
            if (saveChunk(chunk)) {
                chunk->setDirty(false);
                saved++;
            }
        }
    }
    
    return saved;
}

std::vector<ChunkPtr> ChunkManager::getActiveChunks() const {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    std::vector<ChunkPtr> activeChunks;
    activeChunks.reserve(chunks.size());
    
    for (const auto& [pos, chunk] : chunks) {
        activeChunks.push_back(chunk);
    }
    
    return activeChunks;
}

util::Vector3 ChunkManager::worldToChunkPos(const util::Vector3& worldPos) const {
    int chunkSize = config.chunkSize;
    
    // Calculate chunk coordinates by dividing world coordinates by chunk size
    int chunkX = static_cast<int>(std::floor(static_cast<float>(worldPos.x) / chunkSize));
    int chunkY = static_cast<int>(std::floor(static_cast<float>(worldPos.y) / config.worldHeight));
    int chunkZ = static_cast<int>(std::floor(static_cast<float>(worldPos.z) / chunkSize));
    
    return util::Vector3(chunkX, chunkY, chunkZ);
}

util::Vector3 ChunkManager::worldToLocalPos(const util::Vector3& worldPos) const {
    int chunkSize = config.chunkSize;
    
    // Calculate local coordinates within a chunk
    int localX = worldPos.x - static_cast<int>(std::floor(static_cast<float>(worldPos.x) / chunkSize)) * chunkSize;
    int localY = worldPos.y - static_cast<int>(std::floor(static_cast<float>(worldPos.y) / config.worldHeight)) * config.worldHeight;
    int localZ = worldPos.z - static_cast<int>(std::floor(static_cast<float>(worldPos.z) / chunkSize)) * chunkSize;
    
    // Handle negative coordinates
    if (localX < 0) localX += chunkSize;
    if (localY < 0) localY += config.worldHeight;
    if (localZ < 0) localZ += chunkSize;
    
    return util::Vector3(localX, localY, localZ);
}

util::Vector3 ChunkManager::chunkToWorldPos(const util::Vector3& chunkPos, const util::Vector3& localPos) const {
    int chunkSize = config.chunkSize;
    
    // Calculate world coordinates from chunk and local coordinates
    int worldX = chunkPos.x * chunkSize + localPos.x;
    int worldY = chunkPos.y * config.worldHeight + localPos.y;
    int worldZ = chunkPos.z * chunkSize + localPos.z;
    
    return util::Vector3(worldX, worldY, worldZ);
}

ChunkPtr ChunkManager::generateChunk(const util::Vector3& chunkPos) {
    // Create a new chunk
    ChunkPtr chunk = std::make_shared<Chunk>(chunkPos, config.chunkSize, config.worldHeight);
    
    // Generate terrain
    generation::TerrainGenerator terrainGen(config);
    if (!terrainGen.generateTerrain(chunk)) {
        std::cerr << "Failed to generate terrain for chunk at ("
                  << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z
                  << ")" << std::endl;
        return nullptr;
    }
    
    // Generate caves and ores
    generation::CaveGenerator caveGen(config);
    if (!caveGen.generateCaves(chunk)) {
        std::cerr << "Failed to generate caves for chunk at ("
                  << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z
                  << ")" << std::endl;
    }
    
    if (!caveGen.generateOres(chunk)) {
        std::cerr << "Failed to generate ores for chunk at ("
                  << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z
                  << ")" << std::endl;
    }
    
    // In a real implementation, we would add biomes and structures here
    // For now, we'll skip them to keep the example simpler
    
    // Mark the chunk as generated and dirty
    chunk->setGenerated(true);
    chunk->setDirty(true);
    
    return chunk;
}

ChunkPtr ChunkManager::loadChunk(const util::Vector3& chunkPos) {
    std::string filePath = getChunkFilePath(chunkPos);
    
    // Load data from disk
    std::vector<uint8_t> data = util::FileIO::loadFromFile(filePath, config.useCompression);
    if (data.empty()) {
        std::cerr << "Failed to load chunk from: " << filePath << std::endl;
        return nullptr;
    }
    
    // Create a new chunk
    ChunkPtr chunk = std::make_shared<Chunk>(chunkPos, config.chunkSize, config.worldHeight);
    
    // Deserialize the chunk
    if (!chunk->deserialize(data)) {
        std::cerr << "Failed to deserialize chunk from: " << filePath << std::endl;
        return nullptr;
    }
    
    return chunk;
}

bool ChunkManager::saveChunk(const ChunkPtr& chunk) {
    if (!chunk) {
        return false;
    }
    
    const util::Vector3& chunkPos = chunk->getPosition();
    std::string filePath = getChunkFilePath(chunkPos);
    
    // Serialize the chunk
    std::vector<uint8_t> data = chunk->serialize();
    if (data.empty()) {
        std::cerr << "Failed to serialize chunk at ("
                  << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z
                  << ")" << std::endl;
        return false;
    }
    
    // Save data to disk
    return util::FileIO::saveToFile(filePath, data, config.useCompression);
}

void ChunkManager::unloadChunk(const util::Vector3& chunkPos) {
    auto it = chunks.find(chunkPos);
    if (it != chunks.end()) {
        ChunkPtr chunk = it->second;
        
        // Save the chunk if it's dirty
        if (chunk && chunk->isDirty()) {
            saveChunk(chunk);
        }
        
        // Remove the chunk from the map
        chunks.erase(it);
    }
}

std::string ChunkManager::getChunkFilePath(const util::Vector3& chunkPos) const {
    std::stringstream ss;
    ss << config.saveDirectory << "/chunks/";
    ss << "c." << chunkPos.x << "." << chunkPos.y << "." << chunkPos.z << ".dat";
    return ss.str();
}

std::vector<util::Vector3> ChunkManager::calculateSpiralPattern(int viewDistance) const {
    std::vector<util::Vector3> pattern;
    
    // Estimate the number of chunks in the pattern
    int estimatedSize = (2 * viewDistance + 1) * (2 * viewDistance + 1);
    pattern.reserve(estimatedSize);
    
    // Add the center chunk
    pattern.push_back(util::Vector3(0, 0, 0));
    
    // Generate a spiral pattern
    for (int layer = 1; layer <= viewDistance; ++layer) {
        // Top edge
        for (int x = -layer; x <= layer; ++x) {
            pattern.push_back(util::Vector3(x, 0, -layer));
        }
        
        // Right edge
        for (int z = -layer + 1; z <= layer; ++z) {
            pattern.push_back(util::Vector3(layer, 0, z));
        }
        
        // Bottom edge
        for (int x = layer - 1; x >= -layer; --x) {
            pattern.push_back(util::Vector3(x, 0, layer));
        }
        
        // Left edge
        for (int z = layer - 1; z >= -layer + 1; --z) {
            pattern.push_back(util::Vector3(-layer, 0, z));
        }
    }
    
    return pattern;
}

} // namespace core
} // namespace dyg