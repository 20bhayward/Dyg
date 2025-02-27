#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <random>

namespace dyg {
namespace generation {

/**
 * Fast noise generator for terrain and biome generation
 */
class NoiseGenerator {
public:
    /**
     * Constructor
     * @param seed Random seed for the noise generator
     */
    explicit NoiseGenerator(uint32_t seed);
    
    /**
     * Generate 2D Perlin noise
     * @param x X coordinate
     * @param z Z coordinate
     * @param scale Scale of the noise
     * @param octaves Number of octaves for fractal noise
     * @param persistence Persistence for fractal noise
     * @param lacunarity Lacunarity for fractal noise
     * @return Noise value in the range [-1, 1]
     */
    float perlin2D(float x, float z, float scale, int octaves = 1, float persistence = 0.5f, float lacunarity = 2.0f) const;
    
    /**
     * Generate 3D Perlin noise
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @param scale Scale of the noise
     * @param octaves Number of octaves for fractal noise
     * @param persistence Persistence for fractal noise
     * @param lacunarity Lacunarity for fractal noise
     * @return Noise value in the range [-1, 1]
     */
    float perlin3D(float x, float y, float z, float scale, int octaves = 1, float persistence = 0.5f, float lacunarity = 2.0f) const;
    
    /**
     * Generate a 2D heightmap for a chunk
     * @param chunkX X coordinate of the chunk
     * @param chunkZ Z coordinate of the chunk
     * @param chunkSize Size of the chunk
     * @param baseScale Scale of the base noise
     * @param detailScale Scale of the detail noise
     * @return Heightmap as a 2D array of values in the range [0, 1]
     */
    std::vector<float> generateHeightMap(int chunkX, int chunkZ, int chunkSize, float baseScale, float detailScale) const;
    
    /**
     * Generate a 3D noise field for cave generation
     * @param chunkX X coordinate of the chunk
     * @param chunkY Y coordinate of the chunk
     * @param chunkZ Z coordinate of the chunk
     * @param chunkSize Size of the chunk
     * @param chunkHeight Height of the chunk
     * @param scale Scale of the noise
     * @return 3D noise field as a flattened array of values in the range [0, 1]
     */
    std::vector<float> generate3DNoiseField(int chunkX, int chunkY, int chunkZ, int chunkSize, int chunkHeight, float scale) const;
    
    /**
     * Generate a value in the range [0, 1] based on a threshold
     * @param value Input value in the range [0, 1]
     * @param threshold Threshold value in the range [0, 1]
     * @return 1 if value >= threshold, 0 otherwise
     */
    static float thresholdNoise(float value, float threshold);
    
private:
    // Random seed
    uint32_t seed;
    
    // Permutation table
    std::array<uint8_t, 512> perm;
    
    // Initialize the permutation table
    void initPermutationTable();
    
    // Gradient for Perlin noise
    float gradient2D(int hash, float x, float y) const;
    float gradient3D(int hash, float x, float y, float z) const;
    
    // Linear interpolation
    static float lerp(float a, float b, float t);
    
    // Smooth step function
    static float smoothStep(float t);
    
    // 2D and 3D Perlin noise base functions
    float basePerlin2D(float x, float y) const;
    float basePerlin3D(float x, float y, float z) const;
};

} // namespace generation
} // namespace dyg