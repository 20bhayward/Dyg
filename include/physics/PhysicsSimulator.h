#pragma once

#include "core/Chunk.h"
#include "util/Config.h"
#include "util/ThreadPool.h"
#include <vector>
#include <memory>
#include <mutex>

namespace dyg {
namespace physics {

/**
 * Simulates physics for voxels (falling sand, fluid simulation, etc.)
 */
class PhysicsSimulator {
public:
    /**
     * Constructor
     * @param config Configuration for physics simulation
     */
    explicit PhysicsSimulator(const util::Config& config);
    
    /**
     * Update physics for a collection of chunks
     * @param chunks Vector of chunks to update
     * @param threadPool Thread pool for parallel updates
     * @return Number of voxels updated
     */
    int update(const std::vector<core::ChunkPtr>& chunks, util::ThreadPool& threadPool);
    
    /**
     * Check if a voxel is affected by gravity
     * @param voxelType Type of the voxel
     * @return True if the voxel is affected by gravity
     */
    bool isAffectedByGravity(core::VoxelType voxelType) const;
    
    /**
     * Check if a voxel is a fluid
     * @param voxelType Type of the voxel
     * @return True if the voxel is a fluid
     */
    bool isFluid(core::VoxelType voxelType) const;

private:
    // Configuration
    const util::Config& config;
    
    // Track which chunks have active physics
    std::vector<core::ChunkPtr> activeChunks;
    
    // Mutex for thread safety
    std::mutex activeMutex;
    
    // Update physics for a single chunk
    int updateChunk(const core::ChunkPtr& chunk);
    
    // Simulate gravity for affected voxels
    bool simulateGravity(const core::ChunkPtr& chunk, int x, int y, int z);
    
    // Simulate fluid flow
    bool simulateFluid(const core::ChunkPtr& chunk, int x, int y, int z);
    
    // Get a voxel from a chunk or a neighboring chunk
    core::VoxelType getVoxel(const core::ChunkPtr& chunk, int x, int y, int z) const;
    
    // Set a voxel in a chunk or a neighboring chunk
    bool setVoxel(const core::ChunkPtr& chunk, int x, int y, int z, core::VoxelType type);
};

} // namespace physics
} // namespace dyg