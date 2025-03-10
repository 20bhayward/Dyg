#pragma once

namespace PixelPhys {
namespace Physics {
    // Liquid dynamics
    constexpr float LIQUID_DISPERSAL = 0.3f;
    constexpr int MAX_LIQUID_PRESSURE = 8;
    
    // Powder dynamics
    constexpr float POWDER_INERTIA_MULTIPLIER = 1.5f;
    constexpr int MAX_FALL_SPEED = 5;
    
    // Material interaction constants
    constexpr float FIRE_SPREAD_CHANCE = 0.15f;
    constexpr int FIRE_LIFETIME = 120;
    
    // Simulation parameters
    constexpr int SIMULATION_REGION_SIZE = 256; // Region size for optimized physics
    constexpr int CHUNK_UPDATE_PRIORITY = 5;    // Update priority for chunks near the player
    constexpr int MAX_INACTIVE_TIME = 100;      // Maximum frames a chunk can be inactive
    
    // Coordinate conversion helpers
    constexpr float WORLD_TO_CHUNK_FACTOR = 512.0f;
    constexpr float PIXEL_SIZE_DEFAULT = 1.0f;
}
} // namespace PixelPhys