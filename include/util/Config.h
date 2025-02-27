#pragma once

#include <string>
#include <cstdint>

namespace dyg {
namespace util {

/**
 * Configuration class for the world generator
 */
class Config {
public:
    Config();
    Config(int argc, char* argv[]);
    
    // World generation parameters
    uint32_t seed;                     // Random seed for world generation
    int viewDistance;                  // View distance in chunks
    int chunkSize;                     // Size of each chunk (width/length)
    int worldHeight;                   // Height of the world in voxels
    
    // Performance parameters
    int numThreads;                    // Number of worker threads
    int frameDelay;                    // Delay between simulation updates (ms)
    
    // Terrain generation parameters
    float baseNoiseScale;              // Scale for the base terrain noise
    float detailNoiseScale;            // Scale for terrain detail noise
    int caveIterations;                // Number of CA iterations for cave generation
    float oreDensity;                  // Density of ore veins
    
    // Biome parameters
    float temperatureScale;            // Scale for temperature noise
    float humidityScale;               // Scale for humidity noise

    // File I/O parameters
    std::string saveDirectory;         // Directory for saving world data
    bool useCompression;               // Whether to use compression for saved chunks
    
    // Parse command line arguments
    bool parseArguments(int argc, char* argv[]);
    void printHelp() const;
};

} // namespace util
} // namespace dyg