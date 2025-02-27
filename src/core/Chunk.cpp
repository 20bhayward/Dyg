#include "core/Chunk.h"
#include <algorithm>
#include <iostream>

namespace dyg {
namespace core {

Chunk::Chunk(const util::Vector3& position, int chunkSize, int chunkHeight)
    : position(position)
    , size(chunkSize)
    , height(chunkHeight)
    , generated(false)
    , dirty(false) {
    
    // Initialize the voxel data with all air
    voxelData.resize(size * size * height, 0); // 0 is the index for Air in the palette
}

VoxelType Chunk::getVoxel(int x, int y, int z) const {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    if (!isInBounds(x, y, z)) {
        return VoxelType::Air;
    }
    
    size_t index = coordsToIndex(x, y, z);
    return palette.getType(voxelData[index]);
}

void Chunk::setVoxel(int x, int y, int z, VoxelType type) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    if (!isInBounds(x, y, z)) {
        return;
    }
    
    size_t index = coordsToIndex(x, y, z);
    voxelData[index] = palette.addType(type);
    dirty = true;
}

const util::Vector3& Chunk::getPosition() const {
    return position;
}

bool Chunk::isGenerated() const {
    return generated;
}

void Chunk::setGenerated(bool gen) {
    generated = gen;
}

bool Chunk::isDirty() const {
    return dirty;
}

void Chunk::setDirty(bool d) {
    dirty = d;
}

std::vector<uint8_t> Chunk::serialize() const {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    std::vector<uint8_t> data;
    
    // Reserve space for header and data
    data.reserve(sizeof(position) + sizeof(size) + sizeof(height) + 
                 sizeof(uint8_t) * palette.size() + voxelData.size());
    
    // Write position
    data.push_back(static_cast<uint8_t>(position.x & 0xFF));
    data.push_back(static_cast<uint8_t>((position.x >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.x >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.x >> 24) & 0xFF));
    
    data.push_back(static_cast<uint8_t>(position.y & 0xFF));
    data.push_back(static_cast<uint8_t>((position.y >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.y >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.y >> 24) & 0xFF));
    
    data.push_back(static_cast<uint8_t>(position.z & 0xFF));
    data.push_back(static_cast<uint8_t>((position.z >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.z >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((position.z >> 24) & 0xFF));
    
    // Write size
    data.push_back(static_cast<uint8_t>(size & 0xFF));
    data.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
    
    // Write height
    data.push_back(static_cast<uint8_t>(height & 0xFF));
    data.push_back(static_cast<uint8_t>((height >> 8) & 0xFF));
    
    // Write palette size
    data.push_back(static_cast<uint8_t>(palette.size()));
    
    // Write palette
    for (size_t i = 0; i < palette.size(); ++i) {
        data.push_back(static_cast<uint8_t>(palette.getType(i)));
    }
    
    // Write voxel data
    data.insert(data.end(), voxelData.begin(), voxelData.end());
    
    return data;
}

bool Chunk::deserialize(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    if (data.size() < 17) { // Minimum size for header without palette
        std::cerr << "Invalid chunk data: too small" << std::endl;
        return false;
    }
    
    size_t offset = 0;
    
    // Read position
    position.x = data[offset++];
    position.x |= static_cast<int>(data[offset++]) << 8;
    position.x |= static_cast<int>(data[offset++]) << 16;
    position.x |= static_cast<int>(data[offset++]) << 24;
    
    position.y = data[offset++];
    position.y |= static_cast<int>(data[offset++]) << 8;
    position.y |= static_cast<int>(data[offset++]) << 16;
    position.y |= static_cast<int>(data[offset++]) << 24;
    
    position.z = data[offset++];
    position.z |= static_cast<int>(data[offset++]) << 8;
    position.z |= static_cast<int>(data[offset++]) << 16;
    position.z |= static_cast<int>(data[offset++]) << 24;
    
    // Read size
    size = data[offset++];
    size |= static_cast<int>(data[offset++]) << 8;
    
    // Read height
    height = data[offset++];
    height |= static_cast<int>(data[offset++]) << 8;
    
    // Read palette size
    uint8_t paletteSize = data[offset++];
    
    if (offset + paletteSize > data.size()) {
        std::cerr << "Invalid chunk data: palette size exceeds data size" << std::endl;
        return false;
    }
    
    // Read palette
    palette.reset();
    for (uint8_t i = 0; i < paletteSize; ++i) {
        VoxelType type = static_cast<VoxelType>(data[offset++]);
        palette.addType(type);
    }
    
    // Calculate voxel data size
    size_t voxelDataSize = size * size * height;
    
    if (offset + voxelDataSize > data.size()) {
        std::cerr << "Invalid chunk data: voxel data size exceeds data size" << std::endl;
        return false;
    }
    
    // Read voxel data
    voxelData.resize(voxelDataSize);
    std::copy(data.begin() + offset, data.begin() + offset + voxelDataSize, voxelData.begin());
    
    generated = true;
    dirty = false;
    
    return true;
}

int Chunk::getSize() const {
    return size;
}

int Chunk::getHeight() const {
    return height;
}

void Chunk::clear() {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    // Reset the palette
    palette.reset();
    
    // Fill the voxel data with air
    std::fill(voxelData.begin(), voxelData.end(), 0);
    
    dirty = true;
}

} // namespace core
} // namespace dyg