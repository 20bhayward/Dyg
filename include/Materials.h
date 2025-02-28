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
    Grass,       // Top layer of grass for grasslands biome (non-colliding)
    GrassStalks,  // Tall grass stalks that don't collide with player
    Dirt,        // Soil beneath grass
    TopSoil,     // Thin layer between grass and dirt
    Gravel,      // Loose stone that can fall
    Smoke,       // Rises upward from fire
    Steam,       // Produced when water meets hot materials
    Mud,         // Wet dirt, formed when water meets dirt
    Coal,        // Black flammable material that burns for a long time
    Moss,        // Grows on rocks, light green
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
    bool isPassable;      // Player can pass through this material (non-colliding)
    bool isEmissive;      // Whether the material emits light
    bool isRefractive;    // Whether the material refracts light (like water)
    uint8_t density;      // Determines which material sinks/floats (higher = denser)
    uint8_t transparency; // 0-255 value for alpha transparency (255 = opaque)
    uint8_t durability;   // How resistant the material is to destruction (0-255)
    uint8_t emissiveStrength; // 0-255 value for how much light this material emits
    
    // Base colors (RGB, each 0-255)
    uint8_t r, g, b;
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    // Empty                     solid liquid powder gas flammable grass passable emissive refractive density transp durability emitStr  r    g    b
    /* Empty */                 {false, false, false, false, false, false, true,    false, false,     0,   0,     0,         0,    0,   0,   0},
    /* Sand */                  {false, false, true,  false, false, false, false,   false, false,   200, 255,    20,         0,  225, 215, 125},
    /* Water */                 {false, true,  false, false, false, false, false,   false, true,    100, 180,     0,         0,   32, 128, 235},
    /* Stone */                 {true,  false, false, false, false, false, false,   false, false,   255, 255,   100,         0,  150, 150, 150},
    /* Wood */                  {true,  false, false, false, true,  false, false,   false, false,   180, 255,    40,         0,  150, 100,  50},
    /* Fire */                  {false, false, false, true,  false, false, true,    true,  false,    10, 190,     0,       220,  255, 127,  32},
    /* Oil */                   {false, true,  false, false, true,  false, false,   false, true,     90, 200,     0,         0,  140, 120,  60},
    /* Grass */                 {true,  false, false, false, true,  true,  false,   false, false,   150, 255,    20,         0,   85, 180,  65},
    /* GrassStalks */           {false, false, false, false, true,  true,  true,    false, false,    50, 235,     5,         0,   70, 200,  55},
    /* Dirt */                  {true,  false, false, false, false, false, false,   false, false,   180, 255,    30,         0,  110,  80,  40},
    /* TopSoil */               {true,  false, false, false, false, false, false,   false, false,   160, 255,    25,         0,   95,  70,  45},
    /* Gravel */                {false, false, true,  false, false, false, false,   false, false,   210, 255,    35,         0,  130, 130, 130},
    /* Smoke */                 {false, false, false, true,  false, false, true,    false, false,     5, 150,     0,         0,   80,  80,  80},
    /* Steam */                 {false, false, false, true,  false, false, true,    false, false,     5, 180,     0,         0,  200, 200, 200},
    /* Mud */                   {true,  false, false, false, false, false, false,   false, false,   190, 255,    15,         0,   90,  65,  35},
    /* Coal */                  {true,  false, false, false, true,  false, false,   true,  false,   220, 255,    45,        20,   40,  40,  40},
    /* Moss */                  {true,  false, false, false, true,  true,  true,    true,  false,    70, 255,    15,        30,   60, 160,  70}
}};

} // namespace PixelPhys