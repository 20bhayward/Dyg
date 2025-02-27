#pragma once

#include "core/Chunk.h"
#include "util/Vector3.h"
#include "util/ThreadPool.h"
#include "util/Config.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <queue>
#include <functional>

namespace dyg {
namespace core {

// Forward declarations
class World;

/**
 * Manages chunk loading, unloading, and generation
 */
class ChunkManager {
public:
    /**
     * Constructor
     * @param world Reference to the world
     * @param config Configuration for the chunk manager
     */
    ChunkManager(World& world, const util::Config& config);
    
    /**
     * Update the active chunks based on the player position
     * @param playerPos Position of the player in world coordinates
     * @param threadPool Thread pool for chunk generation
     */
    void updateChunks(const util::Vector3& playerPos, util::ThreadPool& threadPool);
    
    /**
     * Get a chunk at the specified position
     * @param chunkPos Position of the chunk in chunk coordinates
     * @return Shared pointer to the chunk, or nullptr if not loaded
     */
    ChunkPtr getChunk(const util::Vector3& chunkPos) const;
    
    /**
     * Request a chunk to be generated
     * @param chunkPos Position of the chunk in chunk coordinates
     * @param threadPool Thread pool for chunk generation
     */
    void requestChunk(const util::Vector3& chunkPos, util::ThreadPool& threadPool);
    
    /**
     * Process completed chunk generation tasks
     * @return Number of chunks processed
     */
    int processCompletedChunks();
    
    /**
     * Save all dirty chunks to disk
     * @return Number of chunks saved
     */
    int saveChunks();
    
    /**
     * Get active chunks
     * @return Vector of active chunks
     */
    std::vector<ChunkPtr> getActiveChunks() const;
    
    /**
     * Convert world coordinates to chunk coordinates
     * @param worldPos Position in world coordinates
     * @return Position in chunk coordinates
     */
    util::Vector3 worldToChunkPos(const util::Vector3& worldPos) const;
    
    /**
     * Convert world coordinates to local chunk coordinates
     * @param worldPos Position in world coordinates
     * @return Local position within a chunk
     */
    util::Vector3 worldToLocalPos(const util::Vector3& worldPos) const;
    
    /**
     * Convert chunk and local coordinates to world coordinates
     * @param chunkPos Position in chunk coordinates
     * @param localPos Local position within the chunk
     * @return Position in world coordinates
     */
    util::Vector3 chunkToWorldPos(const util::Vector3& chunkPos, const util::Vector3& localPos) const;

private:
    // Reference to the world
    World& world;
    
    // Configuration
    const util::Config& config;
    
    // Map of loaded chunks (chunk position -> chunk)
    std::unordered_map<util::Vector3, ChunkPtr, util::Vector3::Hash> chunks;
    
    // List of chunks to be unloaded
    std::queue<util::Vector3> unloadQueue;
    
    // List of chunks being generated
    std::unordered_map<util::Vector3, std::future<ChunkPtr>, util::Vector3::Hash> pendingChunks;
    
    // Mutex for thread safety
    mutable std::mutex chunkMutex;
    
    // Generate a new chunk
    ChunkPtr generateChunk(const util::Vector3& chunkPos);
    
    // Load a chunk from disk
    ChunkPtr loadChunk(const util::Vector3& chunkPos);
    
    // Save a chunk to disk
    bool saveChunk(const ChunkPtr& chunk);
    
    // Unload a chunk
    void unloadChunk(const util::Vector3& chunkPos);
    
    // Get the file path for a chunk
    std::string getChunkFilePath(const util::Vector3& chunkPos) const;
    
    // Calculate spiral pattern for chunk loading
    std::vector<util::Vector3> calculateSpiralPattern(int viewDistance) const;
};

} // namespace core
} // namespace dyg