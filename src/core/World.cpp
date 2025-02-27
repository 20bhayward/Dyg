#include "core/World.h"
#include "util/FileIO.h"
#include <iostream>
#include <filesystem>

namespace dyg {
namespace core {

World::World(const util::Config& config)
    : config(config)
    , playerPosition(0, 0, 0) {
    
    // Create chunk manager
    chunkManager = std::make_unique<ChunkManager>(*this, config);
    
    std::cout << "World initialized with seed: " << config.seed << std::endl;
    std::cout << "View distance: " << config.viewDistance << " chunks" << std::endl;
    std::cout << "Using " << config.numThreads << " threads for world generation" << std::endl;
}

const util::Config& World::getConfig() const {
    return config;
}

uint32_t World::getSeed() const {
    return config.seed;
}

void World::updateChunks(const util::Vector3& playerPos, util::ThreadPool& threadPool) {
    // Update player position
    playerPosition = playerPos;
    
    // Let the chunk manager handle chunk loading/unloading
    chunkManager->updateChunks(playerPos, threadPool);
}

int World::integrateCompletedChunks() {
    return chunkManager->processCompletedChunks();
}

ChunkPtr World::getChunk(const util::Vector3& chunkPos) const {
    return chunkManager->getChunk(chunkPos);
}

VoxelType World::getVoxel(const util::Vector3& worldPos) const {
    // Convert world position to chunk position
    util::Vector3 chunkPos = chunkManager->worldToChunkPos(worldPos);
    
    // Get the chunk
    ChunkPtr chunk = chunkManager->getChunk(chunkPos);
    if (!chunk) {
        return VoxelType::Air;
    }
    
    // Convert world position to local position within the chunk
    util::Vector3 localPos = chunkManager->worldToLocalPos(worldPos);
    
    // Get the voxel from the chunk
    return chunk->getVoxel(localPos.x, localPos.y, localPos.z);
}

bool World::setVoxel(const util::Vector3& worldPos, VoxelType type) {
    // Convert world position to chunk position
    util::Vector3 chunkPos = chunkManager->worldToChunkPos(worldPos);
    
    // Get the chunk
    ChunkPtr chunk = chunkManager->getChunk(chunkPos);
    if (!chunk) {
        return false;
    }
    
    // Convert world position to local position within the chunk
    util::Vector3 localPos = chunkManager->worldToLocalPos(worldPos);
    
    // Set the voxel in the chunk
    chunk->setVoxel(localPos.x, localPos.y, localPos.z, type);
    
    return true;
}

bool World::save() {
    // Create the save directory if it doesn't exist
    if (!util::FileIO::createDirectory(config.saveDirectory)) {
        std::cerr << "Failed to create save directory: " << config.saveDirectory << std::endl;
        return false;
    }
    
    // Save all chunks
    int savedChunks = chunkManager->saveChunks();
    std::cout << "Saved " << savedChunks << " chunks" << std::endl;
    
    // Save world metadata
    std::string metadataPath = config.saveDirectory + "/world.dat";
    std::vector<uint8_t> metadata;
    
    // Seed
    metadata.push_back(static_cast<uint8_t>(config.seed & 0xFF));
    metadata.push_back(static_cast<uint8_t>((config.seed >> 8) & 0xFF));
    metadata.push_back(static_cast<uint8_t>((config.seed >> 16) & 0xFF));
    metadata.push_back(static_cast<uint8_t>((config.seed >> 24) & 0xFF));
    
    // Chunk size
    metadata.push_back(static_cast<uint8_t>(config.chunkSize & 0xFF));
    metadata.push_back(static_cast<uint8_t>((config.chunkSize >> 8) & 0xFF));
    
    // World height
    metadata.push_back(static_cast<uint8_t>(config.worldHeight & 0xFF));
    metadata.push_back(static_cast<uint8_t>((config.worldHeight >> 8) & 0xFF));
    
    // Save metadata
    if (!util::FileIO::saveToFile(metadataPath, metadata, config.useCompression)) {
        std::cerr << "Failed to save world metadata" << std::endl;
        return false;
    }
    
    return true;
}

bool World::load(const std::string& worldName) {
    std::string worldPath = config.saveDirectory + "/" + worldName;
    
    // Check if the world directory exists
    if (!util::FileIO::fileExists(worldPath)) {
        std::cerr << "World does not exist: " << worldPath << std::endl;
        return false;
    }
    
    // Load world metadata
    std::string metadataPath = worldPath + "/world.dat";
    std::vector<uint8_t> metadata = util::FileIO::loadFromFile(metadataPath, config.useCompression);
    
    if (metadata.size() < 8) {
        std::cerr << "Invalid world metadata" << std::endl;
        return false;
    }
    
    // Extract metadata
    uint32_t seed = metadata[0] | (metadata[1] << 8) | (metadata[2] << 16) | (metadata[3] << 24);
    int chunkSize = metadata[4] | (metadata[5] << 8);
    int worldHeight = metadata[6] | (metadata[7] << 8);
    
    // Update config
    config.seed = seed;
    config.chunkSize = chunkSize;
    config.worldHeight = worldHeight;
    
    std::cout << "Loaded world with seed: " << config.seed << std::endl;
    
    // Recreate chunk manager with loaded config
    chunkManager = std::make_unique<ChunkManager>(*this, config);
    
    return true;
}

std::vector<ChunkPtr> World::getActiveChunks() const {
    return chunkManager->getActiveChunks();
}

} // namespace core
} // namespace dyg