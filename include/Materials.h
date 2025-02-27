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
    Grass,       // Top layer of grass for grasslands biome
    Dirt,        // Soil beneath grass
    TopSoil,     // Thin layer between grass and dirt
    Gravel,      // Loose stone that can fall
    Smoke,       // Rises upward from fire
    Steam,       // Produced when water meets hot materials
    COUNT        // Keep this last to track count of materials
};

// Properties of each material
struct MaterialProperties {
    bool isSolid;         // Doesn't flow or fall (static unless disturbed)
    bool isLiquid;        // Flows and spreads horizontally
    bool isPowder;        // Falls and piles up (like sand)
    bool isGas;           // Rises upward
    bool isFlammable;     // Can catch fire
    bool isGrass;         // Special material for grass rendering
    uint8_t density;      // Determines which material sinks/floats (higher = denser)
    uint8_t transparency; // 0-255 value for alpha transparency (255 = opaque)
    
    // Base colors (RGB, each 0-255)
    uint8_t r, g, b;
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    // Empty
    {false, false, false, false, false, false, 0, 0, 0, 0, 0},
    // Sand
    {false, false, true, false, false, false, 200, 255, 225, 215, 125},
    // Water - increased transparency for better liquid appearance
    {false, true, false, false, false, false, 100, 180, 32, 128, 235},
    // Stone
    {true, false, false, false, false, false, 255, 255, 150, 150, 150},
    // Wood
    {true, false, false, false, true, false, 180, 255, 150, 100, 50},
    // Fire
    {false, false, false, true, false, false, 10, 190, 255, 127, 32},
    // Oil
    {false, true, false, false, true, false, 90, 200, 140, 120, 60},
    // Grass
    {true, false, false, false, true, true, 150, 255, 85, 180, 65},
    // Dirt
    {true, false, false, false, false, false, 180, 255, 110, 80, 40},
    // TopSoil
    {true, false, false, false, false, false, 160, 255, 95, 70, 45},
    // Gravel
    {false, false, true, false, false, false, 210, 255, 130, 130, 130},
    // Smoke
    {false, false, false, true, false, false, 5, 150, 80, 80, 80},
    // Steam
    {false, false, false, true, false, false, 5, 180, 200, 200, 200}
}};

} // namespace PixelPhys