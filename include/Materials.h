#pragma once

#include <cstdint>
#include <array>

namespace PixelPhys {

// Material type identifiers
enum class MaterialType : uint8_t {
    Empty = 0,   // Air/empty space
    Sand,        // Granular material that falls and piles
    Water,       // Liquid that flows and spreads
    Stone,       // Solid static material
    Wood,        // Solid burnable material
    Fire,        // Actively burning material
    Oil,         // Flammable liquid
    // Add more materials as needed
    COUNT        // Keep this last to track count of materials
};

// Properties of each material
struct MaterialProperties {
    bool isSolid;         // Doesn't flow or fall (static unless disturbed)
    bool isLiquid;        // Flows and spreads horizontally
    bool isPowder;        // Falls and piles up (like sand)
    bool isGas;           // Rises upward
    bool isFlammable;     // Can catch fire
    uint8_t density;      // Determines which material sinks/floats (higher = denser)
    
    // Colors (RGB, each 0-255)
    uint8_t r, g, b;
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    // Empty
    {false, false, false, false, false, 0, 0, 0, 0},
    // Sand
    {false, false, true, false, false, 200, 225, 215, 125},
    // Water
    {false, true, false, false, false, 100, 32, 128, 235},
    // Stone
    {true, false, false, false, false, 255, 150, 150, 150},
    // Wood
    {true, false, false, false, true, 180, 150, 100, 50},
    // Fire
    {false, false, false, true, false, 10, 255, 127, 32},
    // Oil
    {false, true, false, false, true, 90, 140, 120, 60}
}};

} // namespace PixelPhys