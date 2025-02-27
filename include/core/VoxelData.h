#pragma once

#include <cstdint>
#include <vector>
#include <array>

namespace dyg {
namespace core {

// Types of voxels that can exist in the world
enum class VoxelType : uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Sand,
    Water,
    Wood,
    Leaves,
    Coal,
    Iron,
    Gold,
    Diamond,
    Lava,
    Snow,
    Ice,
    // Add more types as needed
    Count  // Always keep this as the last element
};

// Physical properties of voxels
struct VoxelProperties {
    bool isSolid;          // Whether the voxel blocks movement
    bool isFluid;          // Whether the voxel flows
    bool isGranular;       // Whether the voxel falls (like sand)
    float density;         // Density for fluid simulation
    uint8_t friction;      // Friction coefficient
    uint8_t luminosity;    // How much light it emits
    uint32_t color;        // RGBA color for rendering
};

// Voxel data with properties
class VoxelData {
public:
    VoxelData();
    
    // Get properties for a given voxel type
    static const VoxelProperties& getProperties(VoxelType type);
    
    // Initialize voxel properties
    static void initializeProperties();

private:
    // Array of properties for each voxel type
    static std::array<VoxelProperties, static_cast<size_t>(VoxelType::Count)> properties;
    static bool propertiesInitialized;
};

// Palette-based compression for chunks
class VoxelPalette {
public:
    VoxelPalette();
    
    // Add a type to the palette and get its index
    uint8_t addType(VoxelType type);
    
    // Get the type for a given index
    VoxelType getType(uint8_t index) const;
    
    // Check if the palette is full
    bool isFull() const;
    
    // Get the number of types in the palette
    size_t size() const;
    
    // Reset the palette
    void reset();

private:
    static constexpr size_t MAX_PALETTE_SIZE = 256; // For 8-bit indices
    std::vector<VoxelType> palette;
};

} // namespace core
} // namespace dyg