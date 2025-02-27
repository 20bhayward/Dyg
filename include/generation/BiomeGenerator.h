#pragma once

#include "generation/NoiseGenerator.h"
#include "core/Chunk.h"
#include "util/Config.h"
#include <memory>
#include <array>
#include <vector>

namespace dyg {
namespace generation {

/**
 * Different biome types in the world
 */
enum class BiomeType : uint8_t {
    Ocean,
    Plains,
    Desert,
    Forest,
    Mountains,
    Taiga,
    Swamp,
    Tundra,
    // Add more as needed
    Count
};

/**
 * Biome data with properties
 */
struct BiomeProperties {
    float minHeight;          // Minimum terrain height (0-1)
    float maxHeight;          // Maximum terrain height (0-1)
    float minTemperature;     // Minimum temperature (0-1)
    float maxTemperature;     // Maximum temperature (0-1)
    float minHumidity;        // Minimum humidity (0-1)
    float maxHumidity;        // Maximum humidity (0-1)
    core::VoxelType surfaceBlock;  // Block used for the top layer
    core::VoxelType subSurfaceBlock;  // Block used below the surface
    core::VoxelType underwaterBlock;  // Block used underwater
    int surfaceDepth;         // Depth of the surface layer
    int subsurfaceDepth;      // Depth of the subsurface layer (use this in code)
};

/**
 * Generates and manages biomes for the world
 */
class BiomeGenerator {
public:
    /**
     * Constructor
     * @param config Configuration for biome generation
     */
    explicit BiomeGenerator(const util::Config& config);
    
    /**
     * Apply biomes to a chunk
     * @param chunk Chunk to apply biomes to
     * @return True if successful
     */
    bool applyBiomes(core::ChunkPtr chunk);
    
    /**
     * Get the biome at a specific position
     * @param x X coordinate in world space
     * @param z Z coordinate in world space
     * @return BiomeType at the position
     */
    BiomeType getBiomeAt(int x, int z) const;
    
    /**
     * Get the properties for a specific biome
     * @param biome Biome type
     * @return BiomeProperties for the biome
     */
    const BiomeProperties& getBiomeProperties(BiomeType biome) const;

private:
    // Configuration
    const util::Config& config;
    
    // Noise generator
    std::unique_ptr<NoiseGenerator> noiseGenerator;
    
    // Array of properties for each biome type
    std::array<BiomeProperties, static_cast<size_t>(BiomeType::Count)> biomeProperties;
    
    // Initialize biome properties
    void initializeBiomeProperties();
    
    // Generate temperature and humidity maps
    std::vector<float> generateTemperatureMap(const core::ChunkPtr& chunk) const;
    std::vector<float> generateHumidityMap(const core::ChunkPtr& chunk) const;
    
    // Determine biome based on height, temperature, and humidity
    BiomeType determineBiome(float height, float temperature, float humidity) const;
    
    // Apply surface blocks based on biome and height
    void applySurfaceBlocks(const core::ChunkPtr& chunk, const std::vector<BiomeType>& biomeMap);
};

} // namespace generation
} // namespace dyg