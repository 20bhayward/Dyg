#pragma once

#include "generation/NoiseGenerator.h"
#include "core/Chunk.h"
#include "util/Config.h"
#include <memory>

namespace dyg {
namespace generation {

/**
 * Generates caves and subterranean features
 */
class CaveGenerator {
public:
    /**
     * Constructor
     * @param config Configuration for cave generation
     */
    explicit CaveGenerator(const util::Config& config);
    
    /**
     * Generate caves for a chunk
     * @param chunk Chunk to generate caves for
     * @return True if successful
     */
    bool generateCaves(core::ChunkPtr chunk);
    
    /**
     * Generate ore deposits for a chunk
     * @param chunk Chunk to generate ores for
     * @return True if successful
     */
    bool generateOres(core::ChunkPtr chunk);

private:
    // Configuration
    const util::Config& config;
    
    // Noise generator
    std::unique_ptr<NoiseGenerator> noiseGenerator;
    
    // Generate a 3D noise field for cave carving
    std::vector<float> generateCaveNoiseField(const core::ChunkPtr& chunk) const;
    
    // Apply cellular automata to refine cave shapes
    void applyCellularAutomata(const core::ChunkPtr& chunk, std::vector<bool>& caveMap, int iterations);
    
    // Apply the cave map to the chunk
    void applyCaveMap(const core::ChunkPtr& chunk, const std::vector<bool>& caveMap);
    
    // Check if a voxel is a valid ore placement location
    bool isValidOreLocation(const core::ChunkPtr& chunk, int x, int y, int z) const;
    
    // Generate a single ore vein
    void generateOreVein(const core::ChunkPtr& chunk, int x, int y, int z, core::VoxelType oreType, int size);
    
    // Convert 3D coordinates to array index
    inline size_t coordsToIndex(int x, int y, int z, int sizeX, int sizeY, int sizeZ) const {
        return (y * sizeX * sizeZ) + (z * sizeX) + x;
    }
    
    // Random number generation
    std::mt19937 rng;
};

} // namespace generation
} // namespace dyg