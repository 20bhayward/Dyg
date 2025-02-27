#pragma once

#include "generation/NoiseGenerator.h"
#include "core/Chunk.h"
#include "util/Config.h"
#include <memory>

namespace dyg {
namespace generation {

/**
 * Generates terrain for chunks
 */
class TerrainGenerator {
public:
    /**
     * Constructor
     * @param config Configuration for terrain generation
     */
    explicit TerrainGenerator(const util::Config& config);
    
    /**
     * Generate terrain for a chunk
     * @param chunk Chunk to generate terrain for
     * @return True if successful
     */
    bool generateTerrain(core::ChunkPtr chunk);

private:
    // Configuration
    const util::Config& config;
    
    // Noise generator
    std::unique_ptr<NoiseGenerator> noiseGenerator;
    
    // Generate a heightmap for a chunk
    std::vector<float> generateHeightMap(const core::ChunkPtr& chunk) const;
    
    // Apply the heightmap to the chunk
    void applyHeightMap(const core::ChunkPtr& chunk, const std::vector<float>& heightMap);
    
    // Map a normalized height value to an actual height in blocks
    int mapHeightValue(float normalizedHeight, int worldHeight) const;
};

} // namespace generation
} // namespace dyg