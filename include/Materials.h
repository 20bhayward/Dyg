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
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    //                         Physical Properties                    Base Color        Color Variation
    //                         solid  liquid powder gas  flammable passable r    g    b    varR varG varB
    /* Empty */               {false, false, false, false, false, true,     0,   0,   0,   0,   0,   0},
    /* Sand */                {false, false, true,  false, false, false,   225, 215, 125, 10,  10,  15},
    /* Water */               {false, true,  false, false, false, false,   32,  128, 235, 13,  12,  20},
    /* Stone */               {true,  false, false, false, false, false,   120, 120, 125, 15,  15,  15},
    /* Fire */                {false, false, false, true,  false, true,    255, 127, 32,  30,  40,  30},
    /* Oil */                 {false, true,  false, false, true,  false,   140, 120, 60,  20,  20,  10},
    /* GrassStalks */         {false, false, false, false, true,  true,    70,  200, 55,  10,  20,  10},
    /* Dirt */                {true,  false, false, false, false, false,   110, 80,  40,  15,  10,  5},
    /* FlammableGas */        {false, false, false, true,  true,  true,    50,  180, 50,  20,  40,  20},
    /* Grass */               {true,  false, false, false, true,  false,   60,  180, 60,  15,  20,  10},
    /* Lava */                {false, true,  false, false, false, false,   255, 80,  0,   30,  20,  10},
    /* Snow */                {true,  false, false, false, false, false,   245, 245, 255, 5,   5,   5},
    /* Bedrock */             {true,  false, false, false, false, false,   50,  50,  55,  10,  10,  10},
    /* Sandstone */           {true,  false, false, false, false, false,   200, 180, 120, 15,  15,  10},
    /* Gravel */              {false, false, true,  false, false, false,   130, 130, 130, 25,  25,  25},
    /* TopSoil */             {true,  false, false, false, false, false,   80,  60,  40,  12,  10,  8},
    /* DenseRock */           {true,  false, false, false, false, false,   90,  90,  100, 15,  15,  15},
    
    // Ore materials - all are solid blocks with distinctive colors and variations
    /* IronOre */             {true,  false, false, false, false, false,   120, 120, 130, 25,  20,  20},
    /* CopperOre */           {true,  false, false, false, false, false,   180, 110, 70,  30,  15,  10},
    /* GoldOre */             {true,  false, false, false, false, false,   220, 190, 50,  25,  25,  15},
    /* CoalOre */             {true,  false, false, false, true,  false,   50,  50,  50,  10,  10,  10},
    /* DiamondOre */          {true,  false, false, false, false, false,   140, 230, 240, 25,  35,  35},
    /* SilverOre */           {true,  false, false, false, false, false,   200, 200, 210, 20,  20,  25},
    /* EmeraldOre */          {true,  false, false, false, false, false,   40,  200, 90,  15,  30,  20},
    /* SapphireOre */         {true,  false, false, false, false, false,   30,  90,  210, 15,  25,  40},
    /* RubyOre */             {true,  false, false, false, false, false,   200, 30,  60,  40,  15,  20},
    /* SulfurOre */           {true,  false, false, false, true,  false,   230, 220, 40,  35,  35,  15},
    /* QuartzOre */           {true,  false, false, false, false, false,   235, 235, 235, 20,  20,  25},
    /* UraniumOre */          {true,  false, false, false, false, false,   80,  170, 80,  30,  40,  20}
}};

} // namespace PixelPhys