#include "generation/BiomeGenerator.h"
#include <iostream>

namespace dyg {
namespace generation {

BiomeGenerator::BiomeGenerator(const util::Config& config)
    : config(config) {
    
    noiseGenerator = std::make_unique<NoiseGenerator>(config.seed + 2);
    
    // Initialize biome properties
    initializeBiomeProperties();
}

bool BiomeGenerator::applyBiomes(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "BiomeGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    int chunkSize = chunk->getSize();
    
    // Generate temperature and humidity maps
    std::vector<float> temperatureMap = generateTemperatureMap(chunk);
    std::vector<float> humidityMap = generateHumidityMap(chunk);
    
    // Determine biome for each x,z coordinate
    std::vector<BiomeType> biomeMap(chunkSize * chunkSize);
    
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            // Get values at this coordinate
            float temperature = temperatureMap[z * chunkSize + x];
            float humidity = humidityMap[z * chunkSize + x];
            
            // Get the height at this position
            float height = 0.0f;
            for (int y = chunk->getHeight() - 1; y >= 0; --y) {
                core::VoxelType voxel = chunk->getVoxel(x, y, z);
                if (voxel != core::VoxelType::Air && voxel != core::VoxelType::Water) {
                    height = static_cast<float>(y) / chunk->getHeight();
                    break;
                }
            }
            
            // Determine biome based on height, temperature, and humidity
            BiomeType biome = determineBiome(height, temperature, humidity);
            biomeMap[z * chunkSize + x] = biome;
        }
    }
    
    // Apply surface blocks based on biome
    applySurfaceBlocks(chunk, biomeMap);
    
    return true;
}

BiomeType BiomeGenerator::getBiomeAt(int x, int z) const {
    // Calculate biome based on global coordinates
    float temperature = noiseGenerator->perlin2D(x * 0.001f, z * 0.001f, config.temperatureScale, 4, 0.5f, 2.0f);
    float humidity = noiseGenerator->perlin2D(x * 0.001f + 500.0f, z * 0.001f + 500.0f, config.humidityScale, 4, 0.5f, 2.0f);
    
    // Normalize to [0, 1] range
    temperature = (temperature + 1.0f) * 0.5f;
    humidity = (humidity + 1.0f) * 0.5f;
    
    // Use a default height for this simplified implementation
    float height = 0.5f;
    
    return determineBiome(height, temperature, humidity);
}

const BiomeProperties& BiomeGenerator::getBiomeProperties(BiomeType biome) const {
    return biomeProperties[static_cast<size_t>(biome)];
}

void BiomeGenerator::initializeBiomeProperties() {
    // Ocean biome
    biomeProperties[static_cast<size_t>(BiomeType::Ocean)] = {
        0.0f, 0.3f,           // height range
        0.0f, 1.0f,           // temperature range
        0.3f, 1.0f,           // humidity range
        core::VoxelType::Sand, // surface block
        core::VoxelType::Sand, // subsurface block
        core::VoxelType::Sand, // underwater block
        1,                    // surface depth
        3                     // subsurface depth
    };
    
    // Plains biome
    biomeProperties[static_cast<size_t>(BiomeType::Plains)] = {
        0.3f, 0.5f,           // height range
        0.3f, 0.7f,           // temperature range
        0.3f, 0.7f,           // humidity range
        core::VoxelType::Grass, // surface block
        core::VoxelType::Dirt, // subsurface block
        core::VoxelType::Sand, // underwater block
        1,                    // surface depth
        3                     // subsurface depth
    };
    
    // Desert biome
    biomeProperties[static_cast<size_t>(BiomeType::Desert)] = {
        0.3f, 0.5f,           // height range
        0.7f, 1.0f,           // temperature range
        0.0f, 0.3f,           // humidity range
        core::VoxelType::Sand, // surface block
        core::VoxelType::Sand, // subsurface block
        core::VoxelType::Sand, // underwater block
        3,                    // surface depth
        5                     // subsurface depth
    };
    
    // Forest biome
    biomeProperties[static_cast<size_t>(BiomeType::Forest)] = {
        0.3f, 0.6f,           // height range
        0.3f, 0.7f,           // temperature range
        0.7f, 1.0f,           // humidity range
        core::VoxelType::Grass, // surface block
        core::VoxelType::Dirt, // subsurface block
        core::VoxelType::Dirt, // underwater block
        1,                    // surface depth
        4                     // subsurface depth
    };
    
    // Mountains biome
    biomeProperties[static_cast<size_t>(BiomeType::Mountains)] = {
        0.6f, 1.0f,           // height range
        0.2f, 0.7f,           // temperature range
        0.3f, 0.8f,           // humidity range
        core::VoxelType::Stone, // surface block
        core::VoxelType::Stone, // subsurface block
        core::VoxelType::Stone, // underwater block
        2,                    // surface depth
        5                     // subsurface depth
    };
    
    // Taiga biome
    biomeProperties[static_cast<size_t>(BiomeType::Taiga)] = {
        0.3f, 0.6f,           // height range
        0.0f, 0.3f,           // temperature range
        0.5f, 1.0f,           // humidity range
        core::VoxelType::Snow, // surface block
        core::VoxelType::Dirt, // subsurface block
        core::VoxelType::Dirt, // underwater block
        1,                    // surface depth
        3                     // subsurface depth
    };
    
    // Swamp biome
    biomeProperties[static_cast<size_t>(BiomeType::Swamp)] = {
        0.3f, 0.4f,           // height range
        0.5f, 0.8f,           // temperature range
        0.7f, 1.0f,           // humidity range
        core::VoxelType::Dirt, // surface block
        core::VoxelType::Dirt, // subsurface block
        core::VoxelType::Dirt, // underwater block
        2,                    // surface depth
        4                     // subsurface depth
    };
    
    // Tundra biome
    biomeProperties[static_cast<size_t>(BiomeType::Tundra)] = {
        0.3f, 0.5f,           // height range
        0.0f, 0.2f,           // temperature range
        0.0f, 0.5f,           // humidity range
        core::VoxelType::Snow, // surface block
        core::VoxelType::Dirt, // subsurface block
        core::VoxelType::Dirt, // underwater block
        1,                    // surface depth
        2                     // subsurface depth
    };
}

std::vector<float> BiomeGenerator::generateTemperatureMap(const core::ChunkPtr& chunk) const {
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    
    // World coordinates of the chunk's corner
    const float worldX = chunkPos.x * chunkSize;
    const float worldZ = chunkPos.z * chunkSize;
    
    std::vector<float> temperatureMap(chunkSize * chunkSize);
    
    // Generate temperature map
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            float wx = worldX + x;
            float wz = worldZ + z;
            
            // Generate temperature with multiple octaves
            float temperature = noiseGenerator->perlin2D(wx, wz, config.temperatureScale, 4, 0.5f, 2.0f);
            
            // Normalize to [0, 1] range
            temperature = (temperature + 1.0f) * 0.5f;
            
            // Store in the map
            temperatureMap[z * chunkSize + x] = temperature;
        }
    }
    
    return temperatureMap;
}

std::vector<float> BiomeGenerator::generateHumidityMap(const core::ChunkPtr& chunk) const {
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    
    // World coordinates of the chunk's corner
    const float worldX = chunkPos.x * chunkSize;
    const float worldZ = chunkPos.z * chunkSize;
    
    std::vector<float> humidityMap(chunkSize * chunkSize);
    
    // Generate humidity map (offset coordinates to get different pattern)
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            float wx = worldX + x + 500.0f; // Offset to get different pattern
            float wz = worldZ + z + 500.0f;
            
            // Generate humidity with multiple octaves
            float humidity = noiseGenerator->perlin2D(wx, wz, config.humidityScale, 4, 0.5f, 2.0f);
            
            // Normalize to [0, 1] range
            humidity = (humidity + 1.0f) * 0.5f;
            
            // Store in the map
            humidityMap[z * chunkSize + x] = humidity;
        }
    }
    
    return humidityMap;
}

BiomeType BiomeGenerator::determineBiome(float height, float temperature, float humidity) const {
    // Check each biome's ranges to find a match
    for (size_t i = 0; i < static_cast<size_t>(BiomeType::Count); ++i) {
        const auto& props = biomeProperties[i];
        
        if (height >= props.minHeight && height <= props.maxHeight &&
            temperature >= props.minTemperature && temperature <= props.maxTemperature &&
            humidity >= props.minHumidity && humidity <= props.maxHumidity) {
            return static_cast<BiomeType>(i);
        }
    }
    
    // Default to plains if no match
    return BiomeType::Plains;
}

void BiomeGenerator::applySurfaceBlocks(const core::ChunkPtr& chunk, const std::vector<BiomeType>& biomeMap) {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Apply surface blocks based on biome
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            // Get biome at this position
            BiomeType biome = biomeMap[z * chunkSize + x];
            const auto& props = biomeProperties[static_cast<size_t>(biome)];
            
            // Find the surface block
            int surfaceY = -1;
            for (int y = chunkHeight - 1; y >= 0; --y) {
                core::VoxelType voxel = chunk->getVoxel(x, y, z);
                if (voxel != core::VoxelType::Air && voxel != core::VoxelType::Water) {
                    surfaceY = y;
                    break;
                }
            }
            
            if (surfaceY >= 0) {
                // Check if this is underwater
                bool underwater = false;
                for (int y = surfaceY + 1; y < chunkHeight; ++y) {
                    if (chunk->getVoxel(x, y, z) == core::VoxelType::Water) {
                        underwater = true;
                        break;
                    }
                }
                
                // Apply appropriate blocks
                if (underwater) {
                    // Underwater surface
                    chunk->setVoxel(x, surfaceY, z, props.underwaterBlock);
                    
                    // Subsurface
                    for (int y = 1; y <= props.subsurfaceDepth && surfaceY - y >= 0; ++y) {
                        chunk->setVoxel(x, surfaceY - y, z, props.subSurfaceBlock);
                    }
                } else {
                    // Above water surface
                    chunk->setVoxel(x, surfaceY, z, props.surfaceBlock);
                    
                    // Subsurface
                    for (int y = 1; y <= props.subsurfaceDepth && surfaceY - y >= 0; ++y) {
                        if (y <= props.surfaceDepth) {
                            chunk->setVoxel(x, surfaceY - y, z, props.surfaceBlock);
                        } else {
                            chunk->setVoxel(x, surfaceY - y, z, props.subSurfaceBlock);
                        }
                    }
                }
            }
        }
    }
}

} // namespace generation
} // namespace dyg