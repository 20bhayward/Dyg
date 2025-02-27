#pragma once

#include "generation/BiomeGenerator.h"
#include "core/Chunk.h"
#include "util/Config.h"
#include "util/Vector3.h"
#include <memory>
#include <vector>
#include <random>
#include <string>
#include <unordered_map>

namespace dyg {
namespace generation {

/**
 * Different structure types that can be generated
 */
enum class StructureType : uint8_t {
    Tree,
    Rock,
    Flower,
    Cactus,
    // Add more as needed
    Count
};

/**
 * Structure template for generating structures
 */
class StructureTemplate {
public:
    StructureTemplate(StructureType type, int sizeX, int sizeY, int sizeZ);
    
    StructureType getType() const;
    int getSizeX() const;
    int getSizeY() const;
    int getSizeZ() const;
    core::VoxelType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, core::VoxelType type);
    
private:
    StructureType type;
    int sizeX;
    int sizeY;
    int sizeZ;
    std::vector<core::VoxelType> blocks;
    
    size_t coordsToIndex(int x, int y, int z) const;
    bool isInBounds(int x, int y, int z) const;
};

/**
 * Generates structures and decorations based on biomes
 */
class StructureGenerator {
public:
    /**
     * Constructor
     * @param config Configuration for structure generation
     * @param biomeGenerator Reference to the biome generator
     */
    StructureGenerator(const util::Config& config, const BiomeGenerator& biomeGenerator);
    
    /**
     * Generate structures for a chunk
     * @param chunk Chunk to generate structures for
     * @return True if successful
     */
    bool generateStructures(core::ChunkPtr chunk);
    
    /**
     * Generate decorations (small features) for a chunk
     * @param chunk Chunk to generate decorations for
     * @return True if successful
     */
    bool generateDecorations(core::ChunkPtr chunk);

private:
    // Configuration
    const util::Config& config;
    
    // Reference to the biome generator
    const BiomeGenerator& biomeGenerator;
    
    // Random number generation
    std::mt19937 rng;
    
    // Structure templates
    std::unordered_map<StructureType, std::vector<StructureTemplate>> templates;
    
    // Initialize structure templates
    void initializeTemplates();
    
    // Generate a structure at a specific position
    bool placeStructure(core::ChunkPtr chunk, const StructureTemplate& structure, int x, int y, int z);
    
    // Check if a structure can be placed at a specific position
    bool canPlaceStructure(const core::ChunkPtr& chunk, const StructureTemplate& structure, int x, int y, int z) const;
    
    // Determine structure placement positions for a chunk
    std::vector<util::Vector3> determineStructurePositions(
        const core::ChunkPtr& chunk, 
        BiomeType biomeType, 
        StructureType structureType, 
        float density) const;
    
    // Get valid structure types for a specific biome
    std::vector<StructureType> getValidStructuresForBiome(BiomeType biomeType) const;
};

} // namespace generation
} // namespace dyg