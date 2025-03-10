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
    Gravel,          // Loose rock material, between stone and dirt
    TopSoil,         // Rich soil layer between grass and dirt
    DenseRock,       // Hard rock layer found deep underground or in mountains
    
    // Ore materials
    IronOre,         // Common ore found in most biomes
    CopperOre,       // Common ore found in most biomes
    GoldOre,         // Valuable ore found in deeper layers
    CoalOre,         // Common fuel ore found in most biomes
    DiamondOre,      // Rare and valuable ore found in deepest layers
    SilverOre,       // Semi-rare metal ore
    EmeraldOre,      // Rare gem found primarily in jungle biomes
    SapphireOre,     // Rare gem found primarily in snow biomes
    RubyOre,         // Rare gem found primarily in mountain biomes
    SulfurOre,       // Uncommon ore found in desert biomes
    QuartzOre,       // Common crystalline material in various biomes
    UraniumOre,      // Rare radioactive material found in deepest layers
    
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
    
    // Powder & liquid specific properties - MUST BE uint8_t to avoid narrowing conversion errors
    uint8_t inertialResistance = 50;  // How resistant a powder is to being set into motion (0-100)
    uint8_t dispersalRate = 3;        // How far liquids will search for empty spaces (liquids only)
};

#define MAT_PROPS(m) MATERIAL_PROPERTIES[static_cast<std::size_t>(m)]

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    //                         Physical Properties                    Base Color        Color Variation     Material-specific
    //                         solid  liquid powder gas  flam pass.   r    g    b    varR varG varB        inert. disp.
    /* Empty */               {false, false, false, false, false, true,     0,   0,   0,   0,   0,   0,    50,    0},
    /* Sand */                {false, false, true,  false, false, false,   225, 215, 125, 10,  10,  15,    30,    0},
    /* Water */               {false, true,  false, false, false, false,   32,  128, 235, 13,  12,  20,    0,     5},
    /* Stone */               {true,  false, false, false, false, false,   120, 120, 125, 15,  15,  15,    90,    0},
    /* Fire */                {false, false, false, true,  false, true,    255, 127, 32,  30,  40,  30,    0,     0},
    /* Oil */                 {false, true,  false, false, true,  false,   140, 120, 60,  20,  20,  10,    0,     4},
    /* GrassStalks */         {false, false, false, false, true,  true,    70,  200, 55,  10,  20,  10,    50,    0},
    /* Dirt */                {true,  false, false, false, false, false,   110, 80,  40,  15,  10,  5,     70,    0},
    /* FlammableGas */        {false, false, false, true,  true,  true,    50,  180, 50,  20,  40,  20,    0,     0},
    /* Grass */               {true,  false, false, false, true,  false,   60,  180, 60,  15,  20,  10,    70,    0},
    /* Lava */                {false, true,  false, false, false, false,   255, 80,  0,   30,  20,  10,    0,     2},
    /* Snow */                {true,  false, false, false, false, false,   245, 245, 255, 5,   5,   5,     20,    0},
    /* Bedrock */             {true,  false, false, false, false, false,   50,  50,  55,  10,  10,  10,    100,   0},
    /* Sandstone */           {true,  false, false, false, false, false,   200, 180, 120, 15,  15,  10,    90,    0},
    /* Gravel */              {false, false, true,  false, false, false,   130, 130, 130, 25,  25,  25,    50,    0},
    /* TopSoil */             {true,  false, false, false, false, false,   80,  60,  40,  12,  10,  8,     70,    0},
    /* DenseRock */           {true,  false, false, false, false, false,   90,  90,  100, 15,  15,  15,    100,   0},
    
    // Ore materials - all are solid blocks with distinctive colors and variations
    /* IronOre */             {true,  false, false, false, false, false,   120, 120, 130, 25,  20,  20,    100,   0},
    /* CopperOre */           {true,  false, false, false, false, false,   180, 110, 70,  30,  15,  10,    100,   0},
    /* GoldOre */             {true,  false, false, false, false, false,   220, 190, 50,  25,  25,  15,    100,   0},
    /* CoalOre */             {true,  false, false, false, true,  false,   50,  50,  50,  10,  10,  10,    80,    0},
    /* DiamondOre */          {true,  false, false, false, false, false,   140, 230, 240, 25,  35,  35,    100,   0},
    /* SilverOre */           {true,  false, false, false, false, false,   200, 200, 210, 20,  20,  25,    100,   0},
    /* EmeraldOre */          {true,  false, false, false, false, false,   40,  200, 90,  15,  30,  20,    100,   0},
    /* SapphireOre */         {true,  false, false, false, false, false,   30,  90,  210, 15,  25,  40,    100,   0},
    /* RubyOre */             {true,  false, false, false, false, false,   200, 30,  60,  40,  15,  20,    100,   0},
    /* SulfurOre */           {true,  false, false, false, true,  false,   230, 220, 40,  35,  35,  15,    90,    0},
    /* QuartzOre */           {true,  false, false, false, false, false,   235, 235, 235, 20,  20,  25,    100,   0},
    /* UraniumOre */          {true,  false, false, false, false, false,   80,  170, 80,  30,  40,  20,    100,   0}
}};

} // namespace PixelPhys