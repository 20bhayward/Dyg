#pragma once

#include "core/ChunkManager.h"
#include "util/Config.h"
#include "util/ThreadPool.h"
#include "util/Vector3.h"
#include <memory>
#include <string>

namespace dyg {
namespace core {

/**
 * The world is the main container for all chunks and manages world generation
 */
class World {
public:
    /**
     * Constructor
     * @param config Configuration for the world
     */
    explicit World(const util::Config& config);
    
    /**
     * Get the configuration
     * @return Reference to the configuration
     */
    const util::Config& getConfig() const;
    
    /**
     * Get the seed for world generation
     * @return Seed value
     */
    uint32_t getSeed() const;
    
    /**
     * Update the chunks based on player position
     * @param playerPos Position of the player in world coordinates
     * @param threadPool Thread pool for chunk generation
     */
    void updateChunks(const util::Vector3& playerPos, util::ThreadPool& threadPool);
    
    /**
     * Process completed chunk generation tasks
     * @return Number of chunks processed
     */
    int integrateCompletedChunks();
    
    /**
     * Get a chunk at the specified position
     * @param chunkPos Position of the chunk in chunk coordinates
     * @return Shared pointer to the chunk, or nullptr if not loaded
     */
    ChunkPtr getChunk(const util::Vector3& chunkPos) const;
    
    /**
     * Get the voxel type at the specified world position
     * @param worldPos Position in world coordinates
     * @return Type of the voxel
     */
    VoxelType getVoxel(const util::Vector3& worldPos) const;
    
    /**
     * Set the voxel type at the specified world position
     * @param worldPos Position in world coordinates
     * @param type Type of the voxel
     * @return True if the voxel was set successfully
     */
    bool setVoxel(const util::Vector3& worldPos, VoxelType type);
    
    /**
     * Save the world to disk
     * @return True if the world was saved successfully
     */
    bool save();
    
    /**
     * Load the world from disk
     * @param worldName Name of the world to load
     * @return True if the world was loaded successfully
     */
    bool load(const std::string& worldName);
    
    /**
     * Get active chunks
     * @return Vector of active chunks
     */
    std::vector<ChunkPtr> getActiveChunks() const;

private:
    // Configuration
    util::Config config;
    
    // Chunk manager
    std::unique_ptr<ChunkManager> chunkManager;
    
    // Current player position
    util::Vector3 playerPosition;
};

} // namespace core
} // namespace dyg