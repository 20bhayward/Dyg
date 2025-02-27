#include "generation/CaveGenerator.h"
#include <algorithm>
#include <random>
#include <iostream>

namespace dyg {
namespace generation {

CaveGenerator::CaveGenerator(const util::Config& config)
    : config(config)
    , rng(config.seed + 1) { // Using a different seed than the terrain generator
    
    noiseGenerator = std::make_unique<NoiseGenerator>(config.seed + 1);
}

bool CaveGenerator::generateCaves(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "CaveGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    // Generate 3D noise field for cave carving
    std::vector<float> noiseField = generateCaveNoiseField(chunk);
    
    // Convert noise field to boolean cave map (true = cave, false = solid)
    const float caveThreshold = 0.4f; // Adjust to control cave density
    std::vector<bool> caveMap(noiseField.size());
    for (size_t i = 0; i < noiseField.size(); ++i) {
        caveMap[i] = noiseField[i] > caveThreshold;
    }
    
    // Apply cellular automata to refine cave shapes
    applyCellularAutomata(chunk, caveMap, config.caveIterations);
    
    // Apply the cave map to the chunk
    applyCaveMap(chunk, caveMap);
    
    return true;
}

bool CaveGenerator::generateOres(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "CaveGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Distribution for ore type selection
    std::uniform_real_distribution<float> oreDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> sizeDist(2, 6);
    
    // Calculate the number of ore veins to generate based on chunk volume and ore density
    int chunkVolume = chunkSize * chunkSize * chunkHeight;
    int numVeins = static_cast<int>(chunkVolume * config.oreDensity / 1000);
    
    // Generate ore veins
    for (int i = 0; i < numVeins; ++i) {
        // Random position within the chunk
        std::uniform_int_distribution<int> xDist(0, chunkSize - 1);
        std::uniform_int_distribution<int> zDist(0, chunkSize - 1);
        
        // Height-dependent distributions for different ore types
        int x = xDist(rng);
        int z = zDist(rng);
        
        // Select ore type based on depth
        core::VoxelType oreType;
        int y;
        float roll = oreDist(rng);
        
        // Deep ores (diamonds, gold)
        if (roll < 0.15) {
            std::uniform_int_distribution<int> yDist(1, chunkHeight / 5);
            y = yDist(rng);
            
            if (roll < 0.03) {
                oreType = core::VoxelType::Diamond; // Rarest
            } else if (roll < 0.08) {
                oreType = core::VoxelType::Gold;
            } else {
                oreType = core::VoxelType::Iron;
            }
        }
        // Mid-level ores (iron, coal)
        else if (roll < 0.5) {
            std::uniform_int_distribution<int> yDist(chunkHeight / 5, chunkHeight / 2);
            y = yDist(rng);
            
            if (roll < 0.3) {
                oreType = core::VoxelType::Iron;
            } else {
                oreType = core::VoxelType::Coal;
            }
        }
        // Upper-level ores (coal)
        else {
            std::uniform_int_distribution<int> yDist(chunkHeight / 2, 3 * chunkHeight / 4);
            y = yDist(rng);
            oreType = core::VoxelType::Coal;
        }
        
        // Place the ore vein if the position is valid
        if (isValidOreLocation(chunk, x, y, z)) {
            generateOreVein(chunk, x, y, z, oreType, sizeDist(rng));
        }
    }
    
    return true;
}

std::vector<float> CaveGenerator::generateCaveNoiseField(const core::ChunkPtr& chunk) const {
    // Get chunk position
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Generate 3D noise field
    return noiseGenerator->generate3DNoiseField(
        chunkPos.x, chunkPos.y, chunkPos.z,
        chunkSize, chunkHeight,
        0.05f // Scale for cave noise
    );
}

void CaveGenerator::applyCellularAutomata(const core::ChunkPtr& chunk, std::vector<bool>& caveMap, int iterations) {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Create a temporary map to store the next iteration
    std::vector<bool> newMap(caveMap.size());
    
    // Apply cellular automata iterations
    for (int iter = 0; iter < iterations; ++iter) {
        // Apply the CA rules to each cell
        for (int y = 1; y < chunkHeight - 1; ++y) {
            for (int z = 1; z < chunkSize - 1; ++z) {
                for (int x = 1; x < chunkSize - 1; ++x) {
                    size_t index = coordsToIndex(x, y, z, chunkSize, chunkHeight, chunkSize);
                    
                    // Count neighbors that are caves
                    int caveNeighbors = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0 && dz == 0) continue; // Skip self
                                
                                size_t nIndex = coordsToIndex(x + dx, y + dy, z + dz, chunkSize, chunkHeight, chunkSize);
                                if (caveMap[nIndex]) {
                                    caveNeighbors++;
                                }
                            }
                        }
                    }
                    
                    // Apply CA rules
                    if (caveMap[index]) {
                        // Cave cells become solid if they have too few or too many cave neighbors
                        newMap[index] = caveNeighbors >= 5 && caveNeighbors <= 18;
                    } else {
                        // Solid cells become caves if they have enough cave neighbors
                        newMap[index] = caveNeighbors >= 12;
                    }
                }
            }
        }
        
        // Update the map for the next iteration
        caveMap = newMap;
    }
}

void CaveGenerator::applyCaveMap(const core::ChunkPtr& chunk, const std::vector<bool>& caveMap) {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Apply the cave map to the chunk
    for (int y = 1; y < chunkHeight - 1; ++y) {
        for (int z = 0; z < chunkSize; ++z) {
            for (int x = 0; x < chunkSize; ++x) {
                size_t index = coordsToIndex(x, y, z, chunkSize, chunkHeight, chunkSize);
                
                // If the map indicates a cave, set the voxel to air
                if (caveMap[index]) {
                    // Only carve caves in solid blocks, not in water or other special blocks
                    core::VoxelType currentType = chunk->getVoxel(x, y, z);
                    if (currentType == core::VoxelType::Stone || 
                        currentType == core::VoxelType::Dirt) {
                        chunk->setVoxel(x, y, z, core::VoxelType::Air);
                    }
                }
            }
        }
    }
}

bool CaveGenerator::isValidOreLocation(const core::ChunkPtr& chunk, int x, int y, int z) const {
    // Check if the voxel is stone
    return chunk->getVoxel(x, y, z) == core::VoxelType::Stone;
}

void CaveGenerator::generateOreVein(const core::ChunkPtr& chunk, int x, int y, int z, core::VoxelType oreType, int size) {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Simple flood-fill algorithm for ore vein
    std::uniform_int_distribution<int> dirDist(0, 5);
    
    // Start with the center position
    chunk->setVoxel(x, y, z, oreType);
    
    // Expand in random directions
    int curX = x;
    int curY = y;
    int curZ = z;
    
    for (int i = 0; i < size; ++i) {
        // Pick a random direction
        int dir = dirDist(rng);
        
        // Move in the selected direction
        switch (dir) {
            case 0: curX++; break;
            case 1: curX--; break;
            case 2: curY++; break;
            case 3: curY--; break;
            case 4: curZ++; break;
            case 5: curZ--; break;
        }
        
        // Check if the new position is valid
        if (curX >= 0 && curX < chunkSize &&
            curY >= 0 && curY < chunkHeight &&
            curZ >= 0 && curZ < chunkSize) {
            
            if (isValidOreLocation(chunk, curX, curY, curZ)) {
                chunk->setVoxel(curX, curY, curZ, oreType);
            }
        }
    }
}

} // namespace generation
} // namespace dyg