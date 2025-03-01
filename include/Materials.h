#pragma once

#include <cstdint>
#include <array>

namespace PixelPhys {

// Material type identifiers
enum class MaterialType : uint8_t {
    Empty = 0,       // Air/empty space
    Sand,            // Granular material that falls and piles
    Water,           // Liquid that flows and spreads
    Stone,           // Solid static material
    Fire,            // Actively burning material
    Oil,             // Flammable liquid
    GrassStalks,     // Tall grass stalks that don't collide with player
    Dirt,            // Soil beneath grass
    FlammableGas,    // Gas that can ignite
    Grass,           // Top layer grass
    Lava,            // Hot liquid that causes fire
    Snow,            // Top layer for snow biome
    Bedrock,         // Indestructible bottom layer
    Sandstone,       // Solid material beneath sand
    COUNT            // Keep this last to track count of materials
};

// Properties of each material
struct MaterialProperties {
    // Physical properties
    bool isSolid;         // Doesn't flow or fall (static unless disturbed)
    bool isLiquid;        // Flows and spreads horizontally
    bool isPowder;        // Falls and piles up (like sand)
    bool isGas;           // Rises upward
    bool isFlammable;     // Can catch fire
    bool isPassable;      // Player can pass through this material (non-colliding)
    
    // Base colors (RGB, each 0-255)
    uint8_t r, g, b;
    
    // Color variation for more natural look
    uint8_t varR, varG, varB;  // Variation values for RGB
    
    // Adding transparency for compatibility
    uint8_t transparency = 255;  // 255 = fully opaque
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    //                         Physical Properties                    Base Color        Color Variation
    //                         solid  liquid powder gas  flammable passable r    g    b    varR varG varB
    /* Empty */               {false, false, false, false, false, true,     0,   0,   0,   0,   0,   0},
    /* Sand */                {false, false, true,  false, false, false,   225, 215, 125, 10,  10,  15},
    /* Water */               {false, true,  false, false, false, false,   32,  128, 235, 13,  12,  20},
    /* Stone */               {true,  false, false, false, false, false,   150, 150, 150, 20,  20,  20},
    /* Fire */                {false, false, false, true,  false, true,    255, 127, 32,  30,  40,  30},
    /* Oil */                 {false, true,  false, false, true,  false,   140, 120, 60,  20,  20,  10},
    /* GrassStalks */         {false, false, false, false, true,  true,    70,  200, 55,  10,  20,  10},
    /* Dirt */                {true,  false, false, false, false, false,   110, 80,  40,  15,  10,  5},
    /* FlammableGas */        {false, false, false, true,  true,  true,    50,  180, 50,  20,  40,  20},
    /* Grass */               {true,  false, false, false, true,  false,   60,  180, 60,  15,  20,  10},
    /* Lava */                {false, true,  false, false, false, false,   255, 80,  0,   30,  20,  10},
    /* Snow */                {true,  false, false, false, false, false,   245, 245, 255, 5,   5,   5},
    /* Bedrock */             {true,  false, false, false, false, false,   50,  50,  55,  10,  10,  10},
    /* Sandstone */           {true,  false, false, false, false, false,   200, 180, 120, 15,  15,  10}
}};

} // namespace PixelPhys