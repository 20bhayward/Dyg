#pragma once

#include "core/VoxelData.h"
#include "util/Vector3.h"
#include <vector>
#include <memory>
#include <mutex>

namespace dyg {
namespace core {

/**
 * A chunk is a fixed-size 3D section of the world
 */
class Chunk {
public:
    /**
     * Construct a new chunk at the specified position
     * @param position Position of the chunk in chunk coordinates
     * @param chunkSize Size of the chunk in the X and Z dimensions
     * @param chunkHeight Height of the chunk in the Y dimension
     */
    Chunk(const util::Vector3& position, int chunkSize, int chunkHeight);
    
    /**
     * Get the voxel type at the specified position
     * @param x X coordinate within the chunk
     * @param y Y coordinate within the chunk
     * @param z Z coordinate within the chunk
     * @return Type of the voxel
     */
    VoxelType getVoxel(int x, int y, int z) const;
    
    /**
     * Set the voxel type at the specified position
     * @param x X coordinate within the chunk
     * @param y Y coordinate within the chunk
     * @param z Z coordinate within the chunk
     * @param type Type of the voxel
     */
    void setVoxel(int x, int y, int z, VoxelType type);
    
    /**
     * Get the position of the chunk in chunk coordinates
     * @return Position of the chunk
     */
    const util::Vector3& getPosition() const;
    
    /**
     * Check if the chunk has been generated
     * @return True if the chunk has been generated
     */
    bool isGenerated() const;
    
    /**
     * Mark the chunk as generated
     */
    void setGenerated(bool generated);
    
    /**
     * Check if the chunk has been modified since generation
     * @return True if the chunk has been modified
     */
    bool isDirty() const;
    
    /**
     * Mark the chunk as dirty or clean
     */
    void setDirty(bool dirty);
    
    /**
     * Serialize the chunk data to a byte array
     * @return Serialized chunk data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * Deserialize chunk data from a byte array
     * @param data Serialized chunk data
     * @return True if successful
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * Get the size of the chunk
     * @return Size of the chunk (width/length)
     */
    int getSize() const;
    
    /**
     * Get the height of the chunk
     * @return Height of the chunk
     */
    int getHeight() const;
    
    /**
     * Clear all voxels in the chunk (set to Air)
     */
    void clear();

private:
    // Position of the chunk in chunk coordinates
    util::Vector3 position;
    
    // Size of the chunk in the X and Z dimensions
    int size;
    
    // Height of the chunk in the Y dimension
    int height;
    
    // Data storage for voxels
    std::vector<uint8_t> voxelData;
    
    // Palette for voxel type compression
    VoxelPalette palette;
    
    // Whether the chunk has been generated
    bool generated;
    
    // Whether the chunk has been modified since generation
    bool dirty;
    
    // Mutex for thread safety when modifying the chunk
    mutable std::mutex chunkMutex;
    
    // Convert 3D coordinates to array index
    inline size_t coordsToIndex(int x, int y, int z) const {
        return y * size * size + z * size + x;
    }
    
    // Check if coordinates are valid
    inline bool isInBounds(int x, int y, int z) const {
        return x >= 0 && x < size && y >= 0 && y < height && z >= 0 && z < size;
    }
};

// Shared pointer type for Chunks
using ChunkPtr = std::shared_ptr<Chunk>;

} // namespace core
} // namespace dyg