#include "core/VoxelData.h"
#include <algorithm>

namespace dyg {
namespace core {

// Initialize static members
std::array<VoxelProperties, static_cast<size_t>(VoxelType::Count)> VoxelData::properties;
bool VoxelData::propertiesInitialized = false;

VoxelData::VoxelData() {
    if (!propertiesInitialized) {
        initializeProperties();
    }
}

const VoxelProperties& VoxelData::getProperties(VoxelType type) {
    if (!propertiesInitialized) {
        initializeProperties();
    }
    
    return properties[static_cast<size_t>(type)];
}

void VoxelData::initializeProperties() {
    // Initialize properties for each voxel type
    
    // Air
    properties[static_cast<size_t>(VoxelType::Air)] = {
        false,  // isSolid
        false,  // isFluid
        false,  // isGranular
        0.0f,   // density
        0,      // friction
        0,      // luminosity
        0x00000000  // color (transparent)
    };
    
    // Stone
    properties[static_cast<size_t>(VoxelType::Stone)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        2.5f,   // density
        128,    // friction
        0,      // luminosity
        0xFF888888  // color (gray)
    };
    
    // Dirt
    properties[static_cast<size_t>(VoxelType::Dirt)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        1.5f,   // density
        100,    // friction
        0,      // luminosity
        0xFF8B4513  // color (brown)
    };
    
    // Grass
    properties[static_cast<size_t>(VoxelType::Grass)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        1.5f,   // density
        100,    // friction
        0,      // luminosity
        0xFF00AA00  // color (green)
    };
    
    // Sand
    properties[static_cast<size_t>(VoxelType::Sand)] = {
        true,   // isSolid
        false,  // isFluid
        true,   // isGranular
        1.6f,   // density
        120,    // friction
        0,      // luminosity
        0xFFEEDD44  // color (sand color)
    };
    
    // Water
    properties[static_cast<size_t>(VoxelType::Water)] = {
        false,  // isSolid
        true,   // isFluid
        false,  // isGranular
        1.0f,   // density
        20,     // friction
        0,      // luminosity
        0x8800AAFF  // color (transparent blue)
    };
    
    // Wood
    properties[static_cast<size_t>(VoxelType::Wood)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        0.8f,   // density
        80,     // friction
        0,      // luminosity
        0xFF8B5A2B  // color (wood brown)
    };
    
    // Leaves
    properties[static_cast<size_t>(VoxelType::Leaves)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        0.2f,   // density
        40,     // friction
        0,      // luminosity
        0xAA00CC00  // color (semi-transparent green)
    };
    
    // Coal
    properties[static_cast<size_t>(VoxelType::Coal)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        1.5f,   // density
        90,     // friction
        0,      // luminosity
        0xFF222222  // color (dark gray)
    };
    
    // Iron
    properties[static_cast<size_t>(VoxelType::Iron)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        7.8f,   // density
        150,    // friction
        0,      // luminosity
        0xFFCCCCCC  // color (light gray)
    };
    
    // Gold
    properties[static_cast<size_t>(VoxelType::Gold)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        19.3f,  // density
        120,    // friction
        0,      // luminosity
        0xFFFFD700  // color (gold)
    };
    
    // Diamond
    properties[static_cast<size_t>(VoxelType::Diamond)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        3.5f,   // density
        200,    // friction
        0,      // luminosity
        0xFF00FFFF  // color (cyan)
    };
    
    // Lava
    properties[static_cast<size_t>(VoxelType::Lava)] = {
        false,  // isSolid
        true,   // isFluid
        false,  // isGranular
        3.1f,   // density
        50,     // friction
        15,     // luminosity (emits light)
        0xFFFF4400  // color (orange-red)
    };
    
    // Snow
    properties[static_cast<size_t>(VoxelType::Snow)] = {
        true,   // isSolid
        false,  // isFluid
        true,   // isGranular
        0.1f,   // density
        40,     // friction
        0,      // luminosity
        0xFFFFFFFF  // color (white)
    };
    
    // Ice
    properties[static_cast<size_t>(VoxelType::Ice)] = {
        true,   // isSolid
        false,  // isFluid
        false,  // isGranular
        0.92f,  // density
        10,     // friction (slippery)
        0,      // luminosity
        0xDDAAEEFF  // color (translucent blue)
    };
    
    propertiesInitialized = true;
}

// VoxelPalette implementation
VoxelPalette::VoxelPalette() {
    // Initialize with Air as the first type
    palette.push_back(VoxelType::Air);
}

uint8_t VoxelPalette::addType(VoxelType type) {
    // Check if type already exists in palette
    for (uint8_t i = 0; i < palette.size(); ++i) {
        if (palette[i] == type) {
            return i;
        }
    }
    
    // Add new type if there's room
    if (palette.size() < MAX_PALETTE_SIZE) {
        palette.push_back(type);
        return static_cast<uint8_t>(palette.size() - 1);
    }
    
    // Palette is full, return the index of the most similar type
    // For now, just return Air as a fallback
    return 0;
}

VoxelType VoxelPalette::getType(uint8_t index) const {
    if (index < palette.size()) {
        return palette[index];
    }
    return VoxelType::Air; // Default to air if index is invalid
}

bool VoxelPalette::isFull() const {
    return palette.size() >= MAX_PALETTE_SIZE;
}

size_t VoxelPalette::size() const {
    return palette.size();
}

void VoxelPalette::reset() {
    palette.clear();
    palette.push_back(VoxelType::Air);
}

} // namespace core
} // namespace dyg