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
    Wood,            // Solid burnable material
    Fire,            // Actively burning material
    Oil,             // Flammable liquid
    Grass,           // Top layer of grass for grasslands biome (non-colliding)
    GrassStalks,     // Tall grass stalks that don't collide with player
    Dirt,            // Soil beneath grass
    TopSoil,         // Thin layer between grass and dirt
    Gravel,          // Loose stone that can fall
    Smoke,           // Rises upward from fire
    Steam,           // Produced when water meets hot materials
    Mud,             // Wet dirt, formed when water meets dirt
    Coal,            // Black flammable material that burns for a long time
    Moss,            // Grows on rocks, light green
    Lava,            // Hot molten rock
    Ice,             // Frozen water
    ToxicSludge,     // Poisonous liquid 
    Acid,            // Corrosive liquid that dissolves materials
    Blood,           // Red liquid from creatures
    Slime,           // Sticky viscous substance
    Glass,           // Transparent solid material
    Metal,           // Strong solid material
    Snow,            // Light powder that melts into water
    Poison,          // Purple toxic liquid
    PoisonGas,       // Toxic gas
    FlammableGas,    // Gas that can ignite
    FreezingLiquid,  // Extremely cold liquid
    Honey,           // Sticky, viscous golden liquid
    VoidLiquid,      // Dark liquid that consumes everything
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
    bool isGrass;         // Special material for grass rendering
    bool isPassable;      // Player can pass through this material (non-colliding)
    uint8_t density;      // Determines which material sinks/floats (higher = denser)
    uint8_t durability;   // How resistant the material is to destruction (0-255)
    
    // Visual properties
    bool isEmissive;      // Whether the material emits light
    bool isRefractive;    // Whether the material refracts light (like water)
    bool isTranslucent;   // Material allows light to pass through
    bool isReflective;    // Material reflects light (like metal or glass)
    uint8_t transparency; // 0-255 value for alpha transparency (255 = opaque)
    uint8_t emissiveStrength; // 0-255 value for how much light this material emits
    
    // Shader effects
    bool hasWaveEffect;   // Material has wave-like animation (water, lava)
    bool hasGlowEffect;   // Material has pulsating glow effect
    bool hasParticleEffect; // Material generates particles (fire, smoke)
    bool hasShimmerEffect;  // Material has sparkling or shimmering effect
    uint8_t waveSpeed;    // Speed of wave animation (0-255)
    uint8_t waveHeight;   // Height of wave animation (0-255)
    uint8_t glowSpeed;    // Speed of glow pulsation (0-255)
    uint8_t refractiveIndex; // Light bending strength (0-255)
    
    // Base colors (RGB, each 0-255)
    uint8_t r, g, b;
    
    // Secondary color for effects (RGB, each 0-255)
    uint8_t secondaryR, secondaryG, secondaryB;
};

// Define properties for each material type
constexpr std::array<MaterialProperties, static_cast<std::size_t>(MaterialType::COUNT)> MATERIAL_PROPERTIES = {{
    //                         Physical Properties                                Visual Properties                      Shader Effects                                   Base Color     Secondary Color
    //                         solid liquid powder gas flammable grass passable density durabil  emissv refract transluc reflect transp emitStr waveEff glowEff partcEff shimmEff waveSpd waveHgt glowSpd refrIdx r    g    b    secR secG secB
    /* Empty */               {false, false, false, false, false, false, true,     0,     0,     false, false, false,  false,   0,    0,     false,  false,  false,  false,     0,     0,     0,     0,     0,   0,   0,    0,   0,   0},
    /* Sand */                {false, false, true,  false, false, false, false,  200,    20,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,   225, 215, 125, 235, 225, 140},
    /* Water */               {false, true,  false, false, false, false, false,  100,     0,     false, true,  true,   false,  180,    0,     true,   false,  false,  true,     80,    30,     0,    90,    32, 128, 235,  45, 140, 245},
    /* Stone */               {true,  false, false, false, false, false, false,  255,   100,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,   150, 150, 150, 170, 170, 170},
    /* Wood */                {true,  false, false, false, true,  false, false,  180,    40,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,   150, 100,  50, 130,  90,  40},
    /* Fire */                {false, false, false, true,  false, false, true,    10,     0,     true,  false, false,  false,  190,  220,     false,  true,   true,   true,      0,     0,   120,     0,   255, 127,  32, 255, 200,  20},
    /* Oil */                 {false, true,  false, false, true,  false, false,   90,     0,     false, true,  true,   false,  200,    0,     true,   false,  false,  false,    40,    15,     0,    50,   140, 120,  60, 100,  90,  40},
    /* Grass */               {true,  false, false, false, true,  true,  false,  150,    20,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,    85, 180,  65,  70, 160,  50},
    /* GrassStalks */         {false, false, false, false, true,  true,  true,    50,     5,     false, false, false,  false,  235,    0,     false,  false,  false,  false,     0,     0,     0,     0,    70, 200,  55,  60, 180,  45},
    /* Dirt */                {true,  false, false, false, false, false, false,  180,    30,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,   110,  80,  40, 100,  75,  35},
    /* TopSoil */             {true,  false, false, false, false, false, false,  160,    25,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,    95,  70,  45,  85,  65,  40},
    /* Gravel */              {false, false, true,  false, false, false, false,  210,    35,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,   130, 130, 130, 120, 120, 120},
    /* Smoke */               {false, false, false, true,  false, false, true,     5,     0,     false, false, true,   false,  150,    0,     false,  false,  true,   false,     0,     0,     0,     0,    80,  80,  80, 100, 100, 100},
    /* Steam */               {false, false, false, true,  false, false, true,     5,     0,     false, false, true,   false,  180,    0,     false,  false,  true,   false,     0,     0,     0,     0,   200, 200, 200, 220, 220, 220},
    /* Mud */                 {true,  false, false, false, false, false, false,  190,    15,     false, false, false,  false,  255,    0,     false,  false,  false,  false,     0,     0,     0,     0,    90,  65,  35,  80,  60,  30},
    /* Coal */                {true,  false, false, false, true,  false, false,  220,    45,     true,  false, false,  false,  255,   20,     false,  true,   false,  false,     0,     0,    50,     0,    40,  40,  40,  70,  70,  70},
    /* Moss */                {true,  false, false, false, true,  true,  true,    70,    15,     true,  false, false,  false,  255,   30,     false,  true,   false,  false,     0,     0,    30,     0,    60, 160,  70,  50, 140,  60},
    /* Lava */                {false, true,  false, false, false, false, false,  230,    50,     true,  true,  false,  false,  255,  180,     true,   true,   true,   true,     50,    40,    90,    30,   255,  80,   0, 255, 150,  20},
    /* Ice */                 {true,  false, false, false, false, false, false,  120,    10,     false, true,  true,   true,   180,    0,     false,  false,  false,  true,      0,     0,     0,   100,   200, 220, 255, 220, 240, 255},
    /* ToxicSludge */         {false, true,  false, false, false, false, false,  150,     0,     true,  false, true,   false,  220,   40,     true,   true,   false,  false,    30,    20,    60,    20,    80, 220,  60, 100, 255,  80},
    /* Acid */                {false, true,  false, false, false, false, false,  130,     0,     true,  true,  true,   false,  200,   50,     true,   true,   true,   true,     60,    25,    70,    40,    60, 230,  30,  90, 255,  50},
    /* Blood */               {false, true,  false, false, false, false, false,  140,     0,     false, false, false,  false,  225,    0,     true,   false,  false,  false,    40,    20,     0,    20,   200,  20,  20, 160,  10,  10},
    /* Slime */               {false, true,  false, false, false, false, false,  180,     5,     false, true,  true,   false,  200,    0,     true,   false,  false,  false,    20,    60,     0,    60,   180, 100, 220, 200, 130, 240},
    /* Glass */               {true,  false, false, false, false, false, false,  180,     5,     false, true,  true,   true,   100,    0,     false,  false,  false,  true,      0,     0,     0,   120,   220, 240, 255, 240, 255, 255},
    /* Metal */               {true,  false, false, false, false, false, false,  250,   120,     false, false, false,  true,   255,    0,     false,  false,  false,  true,      0,     0,     0,    30,   180, 190, 200, 200, 210, 220},
    /* Snow */                {false, false, true,  false, false, false, false,   70,     5,     false, false, false,  false,  255,    0,     false,  false,  false,  true,      0,     0,     0,    20,   250, 250, 255, 240, 240, 250},
    /* Poison */              {false, true,  false, false, false, false, false,  120,     0,     true,  false, true,   false,  210,   30,     true,   true,   false,  false,    30,    20,    50,    30,   180,  60, 220, 200,  80, 240},
    /* PoisonGas */           {false, false, false, true,  false, false, true,     5,     0,     true,  false, true,   false,  140,   20,     false,  true,   true,   false,     0,     0,    40,     0,   160,  50, 200, 180,  60, 230},
    /* FlammableGas */        {false, false, false, true,  true,  false, true,     5,     0,     false, false, true,   false,  130,    0,     false,  false,  true,   false,     0,     0,     0,     0,    50, 180,  50,  70, 200,  70},
    /* FreezingLiquid */      {false, true,  false, false, false, false, false,  110,     0,     true,  true,  true,   false,  190,   60,     true,   true,   true,   true,     70,    15,    80,    70,    70, 200, 255,  90, 220, 255},
    /* Honey */               {false, true,  false, false, true,  false, false,  175,    10,     false, true,  true,   false,  240,    0,     true,   false,  false,  true,     15,    20,     0,    50,   240, 200,  30, 255, 220,  40},
    /* VoidLiquid */          {false, true,  false, false, false, false, false,  255,   150,     true,  false, false,  true,   255,  100,     true,   true,   true,   true,     30,    10,   120,    10,    10,  10,  20,  40,  20,  60}
}};

} // namespace PixelPhys