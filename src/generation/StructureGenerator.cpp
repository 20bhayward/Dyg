#include "generation/StructureGenerator.h"
#include <iostream>
#include <random>

namespace dyg {
namespace generation {

StructureTemplate::StructureTemplate(StructureType type, int sizeX, int sizeY, int sizeZ)
    : type(type)
    , sizeX(sizeX)
    , sizeY(sizeY)
    , sizeZ(sizeZ) {
    
    // Initialize the blocks array with air
    blocks.resize(sizeX * sizeY * sizeZ, core::VoxelType::Air);
}

StructureType StructureTemplate::getType() const {
    return type;
}

int StructureTemplate::getSizeX() const {
    return sizeX;
}

int StructureTemplate::getSizeY() const {
    return sizeY;
}

int StructureTemplate::getSizeZ() const {
    return sizeZ;
}

core::VoxelType StructureTemplate::getBlock(int x, int y, int z) const {
    if (!isInBounds(x, y, z)) {
        return core::VoxelType::Air;
    }
    
    return blocks[coordsToIndex(x, y, z)];
}

void StructureTemplate::setBlock(int x, int y, int z, core::VoxelType type) {
    if (!isInBounds(x, y, z)) {
        return;
    }
    
    blocks[coordsToIndex(x, y, z)] = type;
}

size_t StructureTemplate::coordsToIndex(int x, int y, int z) const {
    return y * sizeX * sizeZ + z * sizeX + x;
}

bool StructureTemplate::isInBounds(int x, int y, int z) const {
    return x >= 0 && x < sizeX && y >= 0 && y < sizeY && z >= 0 && z < sizeZ;
}

// StructureGenerator implementation
StructureGenerator::StructureGenerator(const util::Config& config, const BiomeGenerator& biomeGenerator)
    : config(config)
    , biomeGenerator(biomeGenerator)
    , rng(config.seed + 3) { // Use a different seed than other generators
    
    // Initialize structure templates
    initializeTemplates();
}

bool StructureGenerator::generateStructures(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "StructureGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    
    // Calculate world coordinates of the chunk's corner
    int worldX = chunkPos.x * chunkSize;
    int worldZ = chunkPos.z * chunkSize;
    
    // Generate structures for each biome in the chunk
    for (int z = 0; z < chunkSize; z += 4) { // Check every 4 blocks to reduce density
        for (int x = 0; x < chunkSize; x += 4) {
            // Determine biome at this position
            BiomeType biome = biomeGenerator.getBiomeAt(worldX + x, worldZ + z);
            
            // Get valid structure types for this biome
            const auto validStructures = getValidStructuresForBiome(biome);
            
            // Skip if no valid structures
            if (validStructures.empty()) {
                continue;
            }
            
            // Random chance to place a structure
            std::uniform_real_distribution<float> placementDist(0.0f, 1.0f);
            if (placementDist(rng) > 0.1f) { // 10% chance to place a structure
                continue;
            }
            
            // Select a random structure type
            std::uniform_int_distribution<size_t> structTypeDist(0, validStructures.size() - 1);
            StructureType structureType = validStructures[structTypeDist(rng)];
            
            // Get the templates for this structure type
            const auto& templates = this->templates[structureType];
            if (templates.empty()) {
                continue;
            }
            
            // Select a random template
            std::uniform_int_distribution<size_t> templateDist(0, templates.size() - 1);
            const auto& structure = templates[templateDist(rng)];
            
            // Find a place to put the structure
            int structX = x;
            int structZ = z;
            
            // Find the surface height
            int structY = -1;
            for (int y = chunk->getHeight() - 1; y >= 0; --y) {
                core::VoxelType voxel = chunk->getVoxel(structX, y, structZ);
                if (voxel != core::VoxelType::Air && voxel != core::VoxelType::Water) {
                    structY = y + 1; // Place on top of the surface
                    break;
                }
            }
            
            // Skip if no valid placement found
            if (structY == -1) {
                continue;
            }
            
            // Check if we can place the structure here
            if (canPlaceStructure(chunk, structure, structX, structY, structZ)) {
                // Place the structure
                placeStructure(chunk, structure, structX, structY, structZ);
            }
        }
    }
    
    return true;
}

bool StructureGenerator::generateDecorations(core::ChunkPtr chunk) {
    if (!chunk) {
        std::cerr << "StructureGenerator: Null chunk provided" << std::endl;
        return false;
    }
    
    const auto& chunkPos = chunk->getPosition();
    int chunkSize = chunk->getSize();
    
    // Calculate world coordinates of the chunk's corner
    int worldX = chunkPos.x * chunkSize;
    int worldZ = chunkPos.z * chunkSize;
    
    // Generate decorations (flowers, small rocks, etc.)
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            // Determine biome at this position
            BiomeType biome = biomeGenerator.getBiomeAt(worldX + x, worldZ + z);
            
            // Skip ocean and mountain biomes
            if (biome == BiomeType::Ocean || biome == BiomeType::Mountains) {
                continue;
            }
            
            // Random chance to place a decoration
            std::uniform_real_distribution<float> placementDist(0.0f, 1.0f);
            if (placementDist(rng) > 0.05f) { // 5% chance to place a decoration
                continue;
            }
            
            // Find the surface height
            int decorY = -1;
            for (int y = chunk->getHeight() - 1; y >= 0; --y) {
                core::VoxelType voxel = chunk->getVoxel(x, y, z);
                if (voxel != core::VoxelType::Air && voxel != core::VoxelType::Water) {
                    decorY = y + 1; // Place on top of the surface
                    break;
                }
            }
            
            // Skip if no valid placement found
            if (decorY == -1 || decorY >= chunk->getHeight()) {
                continue;
            }
            
            // Place decoration based on biome
            core::VoxelType decorationType;
            bool placeDecoration = true;
            
            switch (biome) {
                case BiomeType::Plains:
                case BiomeType::Forest:
                    // Flowers or grass
                    decorationType = core::VoxelType::Grass;
                    break;
                
                case BiomeType::Desert:
                    // Small cactus or dead bush
                    decorationType = placementDist(rng) < 0.5f ? core::VoxelType::Wood : core::VoxelType::Dirt;
                    break;
                
                case BiomeType::Tundra:
                case BiomeType::Taiga:
                    // Snow-covered rocks
                    decorationType = core::VoxelType::Snow;
                    break;
                
                case BiomeType::Swamp:
                    // Mud or dead plants
                    decorationType = core::VoxelType::Dirt;
                    break;
                
                default:
                    placeDecoration = false;
                    break;
            }
            
            // Place the decoration
            if (placeDecoration) {
                chunk->setVoxel(x, decorY, z, decorationType);
            }
        }
    }
    
    return true;
}

void StructureGenerator::initializeTemplates() {
    // Create a small tree template
    auto smallTree = StructureTemplate(StructureType::Tree, 3, 5, 3);
    
    // Trunk
    smallTree.setBlock(1, 0, 1, core::VoxelType::Wood);
    smallTree.setBlock(1, 1, 1, core::VoxelType::Wood);
    smallTree.setBlock(1, 2, 1, core::VoxelType::Wood);
    
    // Leaves
    for (int y = 3; y <= 4; ++y) {
        for (int z = 0; z < 3; ++z) {
            for (int x = 0; x < 3; ++x) {
                if (x == 1 && z == 1 && y == 3) {
                    smallTree.setBlock(x, y, z, core::VoxelType::Wood); // Extend trunk
                } else {
                    smallTree.setBlock(x, y, z, core::VoxelType::Leaves);
                }
            }
        }
    }
    
    templates[StructureType::Tree].push_back(smallTree);
    
    // Create a large tree template
    auto largeTree = StructureTemplate(StructureType::Tree, 5, 8, 5);
    
    // Trunk
    for (int y = 0; y < 5; ++y) {
        largeTree.setBlock(2, y, 2, core::VoxelType::Wood);
    }
    
    // Leaves (bottom layer, wider)
    for (int y = 4; y <= 7; ++y) {
        int radius = (y < 6) ? 2 : 1;
        for (int z = 2 - radius; z <= 2 + radius; ++z) {
            for (int x = 2 - radius; x <= 2 + radius; ++x) {
                if (x == 2 && z == 2 && y <= 5) {
                    largeTree.setBlock(x, y, z, core::VoxelType::Wood); // Extend trunk
                } else {
                    largeTree.setBlock(x, y, z, core::VoxelType::Leaves);
                }
            }
        }
    }
    
    templates[StructureType::Tree].push_back(largeTree);
    
    // Create a rock template
    auto smallRock = StructureTemplate(StructureType::Rock, 3, 2, 3);
    
    // Rock shape
    smallRock.setBlock(0, 0, 0, core::VoxelType::Stone);
    smallRock.setBlock(1, 0, 0, core::VoxelType::Stone);
    smallRock.setBlock(2, 0, 0, core::VoxelType::Stone);
    smallRock.setBlock(0, 0, 1, core::VoxelType::Stone);
    smallRock.setBlock(1, 0, 1, core::VoxelType::Stone);
    smallRock.setBlock(2, 0, 1, core::VoxelType::Stone);
    smallRock.setBlock(0, 0, 2, core::VoxelType::Stone);
    smallRock.setBlock(1, 0, 2, core::VoxelType::Stone);
    smallRock.setBlock(2, 0, 2, core::VoxelType::Stone);
    
    // Upper part
    smallRock.setBlock(1, 1, 1, core::VoxelType::Stone);
    
    templates[StructureType::Rock].push_back(smallRock);
    
    // Create a flower template
    auto flower = StructureTemplate(StructureType::Flower, 1, 1, 1);
    flower.setBlock(0, 0, 0, core::VoxelType::Grass);
    templates[StructureType::Flower].push_back(flower);
    
    // Create a cactus template
    auto cactus = StructureTemplate(StructureType::Cactus, 1, 3, 1);
    cactus.setBlock(0, 0, 0, core::VoxelType::Wood);
    cactus.setBlock(0, 1, 0, core::VoxelType::Wood);
    cactus.setBlock(0, 2, 0, core::VoxelType::Wood);
    templates[StructureType::Cactus].push_back(cactus);
}

bool StructureGenerator::placeStructure(core::ChunkPtr chunk, const StructureTemplate& structure, int x, int y, int z) {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Place the structure
    for (int sy = 0; sy < structure.getSizeY(); ++sy) {
        for (int sz = 0; sz < structure.getSizeZ(); ++sz) {
            for (int sx = 0; sx < structure.getSizeX(); ++sx) {
                // Convert structure coordinates to chunk coordinates
                int cx = x + sx;
                int cy = y + sy;
                int cz = z + sz;
                
                // Skip if outside chunk bounds
                if (cx < 0 || cx >= chunkSize || cy < 0 || cy >= chunkHeight || cz < 0 || cz >= chunkSize) {
                    continue;
                }
                
                // Get the block type from the structure
                core::VoxelType blockType = structure.getBlock(sx, sy, sz);
                
                // Skip air blocks
                if (blockType == core::VoxelType::Air) {
                    continue;
                }
                
                // Place the block
                chunk->setVoxel(cx, cy, cz, blockType);
            }
        }
    }
    
    return true;
}

bool StructureGenerator::canPlaceStructure(const core::ChunkPtr& chunk, const StructureTemplate& structure, int x, int y, int z) const {
    int chunkSize = chunk->getSize();
    int chunkHeight = chunk->getHeight();
    
    // Check if the structure fits within the chunk
    if (x + structure.getSizeX() > chunkSize ||
        y + structure.getSizeY() > chunkHeight ||
        z + structure.getSizeZ() > chunkSize) {
        return false;
    }
    
    // Ensure we have solid ground under the structure
    for (int sx = 0; sx < structure.getSizeX(); ++sx) {
        for (int sz = 0; sz < structure.getSizeZ(); ++sz) {
            // Check the block below
            if (y > 0) {
                core::VoxelType blockType = chunk->getVoxel(x + sx, y - 1, z + sz);
                
                // If any block below is air or water, return false
                if (blockType == core::VoxelType::Air || blockType == core::VoxelType::Water) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

std::vector<util::Vector3> StructureGenerator::determineStructurePositions(
    const core::ChunkPtr& chunk, 
    BiomeType biomeType, 
    StructureType structureType, 
    float density) const {
    
    std::vector<util::Vector3> positions;
    
    int chunkSize = chunk->getSize();
    
    // Use different random number generator for each biome/structure combination
    std::mt19937 posDeterministicRng(
        config.seed + 
        static_cast<uint32_t>(biomeType) * 100 +
        static_cast<uint32_t>(structureType) * 10000 + 
        chunk->getPosition().x * 1000000 + 
        chunk->getPosition().z * 100000000
    );
    
    // Number of structures to place
    int numStructures = static_cast<int>(chunkSize * chunkSize * density);
    
    // Distributions for position
    std::uniform_int_distribution<int> xDist(0, chunkSize - 1);
    std::uniform_int_distribution<int> zDist(0, chunkSize - 1);
    
    // Generate positions
    for (int i = 0; i < numStructures; ++i) {
        int x = xDist(posDeterministicRng);
        int z = zDist(posDeterministicRng);
        
        // Find the surface height
        int y = -1;
        for (int testY = chunk->getHeight() - 1; testY >= 0; --testY) {
            core::VoxelType voxel = chunk->getVoxel(x, testY, z);
            if (voxel != core::VoxelType::Air && voxel != core::VoxelType::Water) {
                y = testY + 1; // Place on top of the surface
                break;
            }
        }
        
        // Skip if no valid position found
        if (y == -1) {
            continue;
        }
        
        positions.push_back(util::Vector3(x, y, z));
    }
    
    return positions;
}

std::vector<StructureType> StructureGenerator::getValidStructuresForBiome(BiomeType biomeType) const {
    std::vector<StructureType> validStructures;
    
    switch (biomeType) {
        case BiomeType::Plains:
            validStructures.push_back(StructureType::Tree);
            validStructures.push_back(StructureType::Rock);
            validStructures.push_back(StructureType::Flower);
            break;
        
        case BiomeType::Forest:
            validStructures.push_back(StructureType::Tree);
            validStructures.push_back(StructureType::Flower);
            break;
        
        case BiomeType::Desert:
            validStructures.push_back(StructureType::Rock);
            validStructures.push_back(StructureType::Cactus);
            break;
        
        case BiomeType::Mountains:
            validStructures.push_back(StructureType::Rock);
            break;
        
        case BiomeType::Taiga:
        case BiomeType::Tundra:
            validStructures.push_back(StructureType::Tree);
            validStructures.push_back(StructureType::Rock);
            break;
        
        case BiomeType::Swamp:
            validStructures.push_back(StructureType::Tree);
            break;
        
        case BiomeType::Ocean:
            // No structures for ocean
            break;
        
        default:
            break;
    }
    
    return validStructures;
}

} // namespace generation
} // namespace dyg