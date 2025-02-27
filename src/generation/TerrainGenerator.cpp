#include "generation/TerrainGenerator.h"
#include <algorithm>
#include <iostream>

namespace dyg {
namespace generation {

TerrainGenerator::TerrainGenerator(const util::Config& config)
    : config(config) {
    noiseGenerator = std::make_unique<NoiseGenerator>(config.seed);
}

bool TerrainGenerator::generateTerrain(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "TerrainGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    // Generate heightmap for the chunk
    std::vector<float> heightMap = generateHeightMap(chunk);
    
    // Apply the heightmap to the chunk
    applyHeightMap(chunk, heightMap);
    
    // Mark the chunk as generated (partially - still needs caves, ores, etc.)
    chunk->setGenerated(true);
    chunk->setDirty(true);
    
    return true;
}

std::vector<float> TerrainGenerator::generateHeightMap(const core::ChunkPtr& chunk) const {
    // Get the chunk's position
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    
    // Generate heightmap using noise generator
    return noiseGenerator->generateHeightMap(
        chunkPos.x, chunkPos.z,
        chunkSize,
        config.baseNoiseScale,
        config.detailNoiseScale
    );
}

void TerrainGenerator::applyHeightMap(const core::ChunkPtr& chunk, const std::vector<float>& heightMap) {
    int chunkSize = chunk->getSize();
    int worldHeight = config.worldHeight;
    
    // Apply the heightmap to the chunk
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            // Get the height value at this position
            float normalizedHeight = heightMap[z * chunkSize + x];
            
            // Convert to actual block height
            int height = mapHeightValue(normalizedHeight, worldHeight);
            
            // Fill columns with appropriate blocks
            for (int y = 0; y < chunk->getHeight(); ++y) {
                if (y < height - 4) {
                    // Deep underground - stone
                    chunk->setVoxel(x, y, z, core::VoxelType::Stone);
                } else if (y < height - 1) {
                    // Underground - dirt
                    chunk->setVoxel(x, y, z, core::VoxelType::Dirt);
                } else if (y < height) {
                    // Surface - grass (will be modified by biome generator)
                    chunk->setVoxel(x, y, z, core::VoxelType::Grass);
                } else if (y < height + 1 && height < worldHeight / 3) {
                    // Water for low areas (oceans, lakes)
                    chunk->setVoxel(x, y, z, core::VoxelType::Water);
                } else {
                    // Air above the surface
                    chunk->setVoxel(x, y, z, core::VoxelType::Air);
                }
            }
        }
    }
}

int TerrainGenerator::mapHeightValue(float normalizedHeight, int worldHeight) const {
    // Base height calculation - maps [0,1] to a reasonable height range
    // Adjust these parameters for different terrain styles
    
    // Mountains
    if (normalizedHeight > 0.8f) {
        // Exponential increase for mountain peaks
        float mountainFactor = (normalizedHeight - 0.8f) / 0.2f;
        return static_cast<int>(worldHeight * (0.6f + 0.3f * mountainFactor * mountainFactor));
    }
    // Hills
    else if (normalizedHeight > 0.6f) {
        float hillFactor = (normalizedHeight - 0.6f) / 0.2f;
        return static_cast<int>(worldHeight * (0.45f + 0.15f * hillFactor));
    }
    // Plains
    else if (normalizedHeight > 0.3f) {
        float plainFactor = (normalizedHeight - 0.3f) / 0.3f;
        return static_cast<int>(worldHeight * (0.4f + 0.05f * plainFactor));
    }
    // Shallow water
    else if (normalizedHeight > 0.2f) {
        float shallowFactor = (normalizedHeight - 0.2f) / 0.1f;
        return static_cast<int>(worldHeight * (0.35f + 0.05f * shallowFactor));
    }
    // Ocean
    else {
        float oceanFactor = normalizedHeight / 0.2f;
        return static_cast<int>(worldHeight * (0.25f + 0.1f * oceanFactor));
    }
}

} // namespace generation
} // namespace dyg