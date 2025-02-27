#include "physics/PhysicsSimulator.h"
#include "core/VoxelData.h"
#include <iostream>

namespace dyg {
namespace physics {

PhysicsSimulator::PhysicsSimulator(const util::Config& config)
    : config(config) {
}

int PhysicsSimulator::update(const std::vector<core::ChunkPtr>& chunks, util::ThreadPool& threadPool) {
    if (chunks.empty()) {
        return 0;
    }
    
    // Keep track of which chunks have active physics
    {
        std::lock_guard<std::mutex> lock(activeMutex);
        activeChunks = chunks;
    }
    
    // Process physics in parallel for each chunk
    std::vector<std::future<int>> results;
    results.reserve(chunks.size());
    
    for (const auto& chunk : chunks) {
        results.push_back(
            threadPool.enqueue([this, chunk]() {
                return updateChunk(chunk);
            })
        );
    }
    
    // Wait for all tasks to complete and count the total updates
    int totalUpdates = 0;
    for (auto& result : results) {
        totalUpdates += result.get();
    }
    
    return totalUpdates;
}

int PhysicsSimulator::updateChunk(const core::ChunkPtr& chunk) {
    if (!chunk || !chunk->isGenerated()) {
        return 0;
    }
    
    int updates = 0;
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Bottom-up scanning for gravity-affected voxels
    // This ensures that voxels can fall through multiple blocks in one update
    for (int y = 1; y < chunkHeight; ++y) {
        for (int z = 0; z < chunkSize; ++z) {
            for (int x = 0; x < chunkSize; ++x) {
                core::VoxelType voxelType = chunk->getVoxel(x, y, z);
                
                // Skip air voxels
                if (voxelType == core::VoxelType::Air) {
                    continue;
                }
                
                bool updated = false;
                
                // Process gravity for affected blocks
                if (isAffectedByGravity(voxelType)) {
                    updated = simulateGravity(chunk, x, y, z);
                }
                // Process fluid flow
                else if (isFluid(voxelType)) {
                    updated = simulateFluid(chunk, x, y, z);
                }
                
                if (updated) {
                    updates++;
                }
            }
        }
    }
    
    if (updates > 0) {
        chunk->setDirty(true);
    }
    
    return updates;
}

bool PhysicsSimulator::isAffectedByGravity(core::VoxelType voxelType) const {
    const auto& properties = core::VoxelData::getProperties(voxelType);
    return properties.isGranular;
}

bool PhysicsSimulator::isFluid(core::VoxelType voxelType) const {
    const auto& properties = core::VoxelData::getProperties(voxelType);
    return properties.isFluid;
}

bool PhysicsSimulator::simulateGravity(const core::ChunkPtr& chunk, int x, int y, int z) {
    // Check if there's empty space below
    core::VoxelType below = getVoxel(chunk, x, y - 1, z);
    
    if (below == core::VoxelType::Air) {
        // Move the voxel down
        core::VoxelType current = chunk->getVoxel(x, y, z);
        setVoxel(chunk, x, y - 1, z, current);
        setVoxel(chunk, x, y, z, core::VoxelType::Air);
        return true;
    }
    
    // If blocked but fluid below, move down and displace fluid to the side
    if (isFluid(below)) {
        // Find an adjacent fluid block to displace to
        for (int dx = -1; dx <= 1; dx += 2) {
            if (getVoxel(chunk, x + dx, y - 1, z) == core::VoxelType::Air) {
                // Displace fluid
                setVoxel(chunk, x + dx, y - 1, z, below);
                // Move voxel down
                core::VoxelType current = chunk->getVoxel(x, y, z);
                setVoxel(chunk, x, y - 1, z, current);
                setVoxel(chunk, x, y, z, core::VoxelType::Air);
                return true;
            }
        }
        
        for (int dz = -1; dz <= 1; dz += 2) {
            if (getVoxel(chunk, x, y - 1, z + dz) == core::VoxelType::Air) {
                // Displace fluid
                setVoxel(chunk, x, y - 1, z + dz, below);
                // Move voxel down
                core::VoxelType current = chunk->getVoxel(x, y, z);
                setVoxel(chunk, x, y - 1, z, current);
                setVoxel(chunk, x, y, z, core::VoxelType::Air);
                return true;
            }
        }
    }
    
    return false;
}

bool PhysicsSimulator::simulateFluid(const core::ChunkPtr& chunk, int x, int y, int z) {
    core::VoxelType fluidType = chunk->getVoxel(x, y, z);
    
    // Check if there's empty space below
    if (getVoxel(chunk, x, y - 1, z) == core::VoxelType::Air) {
        // Flow downward
        setVoxel(chunk, x, y - 1, z, fluidType);
        setVoxel(chunk, x, y, z, core::VoxelType::Air);
        return true;
    }
    
    // Check horizontal flow (only if not flowing down)
    bool flowed = false;
    for (int dx = -1; dx <= 1; dx += 2) {
        if (getVoxel(chunk, x + dx, y, z) == core::VoxelType::Air && 
            getVoxel(chunk, x + dx, y - 1, z) == core::VoxelType::Air) {
            // Flow horizontally and downward
            setVoxel(chunk, x + dx, y - 1, z, fluidType);
            flowed = true;
        }
    }
    
    for (int dz = -1; dz <= 1; dz += 2) {
        if (getVoxel(chunk, x, y, z + dz) == core::VoxelType::Air && 
            getVoxel(chunk, x, y - 1, z + dz) == core::VoxelType::Air) {
            // Flow horizontally and downward
            setVoxel(chunk, x, y - 1, z + dz, fluidType);
            flowed = true;
        }
    }
    
    // If the fluid flowed, remove it from the current position
    if (flowed) {
        setVoxel(chunk, x, y, z, core::VoxelType::Air);
    }
    
    return flowed;
}

core::VoxelType PhysicsSimulator::getVoxel(const core::ChunkPtr& chunk, int x, int y, int z) const {
    // Get a voxel from the chunk or a neighboring chunk
    const int chunkSize = chunk->getSize();
    
    // If coordinates are within the chunk, get the voxel directly
    if (x >= 0 && x < chunkSize && y >= 0 && y < chunk->getHeight() && z >= 0 && z < chunkSize) {
        return chunk->getVoxel(x, y, z);
    }
    
    // Otherwise, we'd need to find the neighboring chunk
    // For simplicity in this basic implementation, just return Air
    // A full implementation would calculate the neighboring chunk position and fetch it
    return core::VoxelType::Air;
}

bool PhysicsSimulator::setVoxel(const core::ChunkPtr& chunk, int x, int y, int z, core::VoxelType type) {
    // Set a voxel in the chunk or a neighboring chunk
    const int chunkSize = chunk->getSize();
    
    // If coordinates are within the chunk, set the voxel directly
    if (x >= 0 && x < chunkSize && y >= 0 && y < chunk->getHeight() && z >= 0 && z < chunkSize) {
        chunk->setVoxel(x, y, z, type);
        return true;
    }
    
    // Otherwise, we'd need to find the neighboring chunk
    // For simplicity in this basic implementation, just return false
    // A full implementation would calculate the neighboring chunk position and modify it
    return false;
}

} // namespace physics
} // namespace dyg