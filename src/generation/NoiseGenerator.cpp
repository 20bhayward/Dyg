#include "generation/NoiseGenerator.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace dyg {
namespace generation {

NoiseGenerator::NoiseGenerator(uint32_t seed) : seed(seed) {
    initPermutationTable();
}

void NoiseGenerator::initPermutationTable() {
    // Create a vector with values 0-255
    std::vector<uint8_t> values(256);
    for (int i = 0; i < 256; i++) {
        values[i] = i;
    }
    
    // Shuffle the values using the seed
    std::mt19937 rng(seed);
    std::shuffle(values.begin(), values.end(), rng);
    
    // Fill the permutation table
    for (int i = 0; i < 256; i++) {
        perm[i] = values[i];
        perm[i + 256] = values[i]; // Duplicate for easy indexing
    }
}

float NoiseGenerator::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float NoiseGenerator::smoothStep(float t) {
    return t * t * (3 - 2 * t);
}

float NoiseGenerator::gradient2D(int hash, float x, float y) const {
    // Convert low 3 bits of hash code into 8 simple gradient directions
    const int h = hash & 7;
    const float u = h < 4 ? x : y;
    const float v = h < 4 ? y : x;
    
    // Convert the direction to a gradient
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float NoiseGenerator::gradient3D(int hash, float x, float y, float z) const {
    // Convert low 4 bits of hash code into 12 simple gradient directions
    const int h = hash & 15;
    const float u = h < 8 ? x : y;
    const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    
    // Convert the direction to a gradient
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float NoiseGenerator::basePerlin2D(float x, float y) const {
    // Find unit grid cell containing the point
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    
    // Get relative position within the cell
    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);
    
    // Compute fade curves
    const float u = smoothStep(xf);
    const float v = smoothStep(yf);
    
    // Hash coordinates of the 4 cell corners
    const int a = perm[xi] + yi;
    const int b = perm[xi + 1] + yi;
    const int aa = perm[a];
    const int ba = perm[b];
    const int ab = perm[a + 1];
    const int bb = perm[b + 1];
    
    // Calculate the gradients at each corner
    const float g00 = gradient2D(perm[aa], xf, yf);
    const float g10 = gradient2D(perm[ba], xf - 1, yf);
    const float g01 = gradient2D(perm[ab], xf, yf - 1);
    const float g11 = gradient2D(perm[bb], xf - 1, yf - 1);
    
    // Bilinear interpolation with fade function
    const float x1 = lerp(g00, g10, u);
    const float x2 = lerp(g01, g11, u);
    
    return lerp(x1, x2, v);
}

float NoiseGenerator::basePerlin3D(float x, float y, float z) const {
    // Find unit grid cell containing the point
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;
    
    // Get relative position within the cell
    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);
    const float zf = z - std::floor(z);
    
    // Compute fade curves
    const float u = smoothStep(xf);
    const float v = smoothStep(yf);
    const float w = smoothStep(zf);
    
    // Hash coordinates of the 8 cell corners
    const int a = perm[xi] + yi;
    const int b = perm[xi + 1] + yi;
    const int aa = perm[a] + zi;
    const int ba = perm[b] + zi;
    const int ab = perm[a + 1] + zi;
    const int bb = perm[b + 1] + zi;
    
    // Calculate the gradients at each corner
    const float g000 = gradient3D(perm[aa], xf, yf, zf);
    const float g100 = gradient3D(perm[ba], xf - 1, yf, zf);
    const float g010 = gradient3D(perm[ab], xf, yf - 1, zf);
    const float g110 = gradient3D(perm[bb], xf - 1, yf - 1, zf);
    const float g001 = gradient3D(perm[aa + 1], xf, yf, zf - 1);
    const float g101 = gradient3D(perm[ba + 1], xf - 1, yf, zf - 1);
    const float g011 = gradient3D(perm[ab + 1], xf, yf - 1, zf - 1);
    const float g111 = gradient3D(perm[bb + 1], xf - 1, yf - 1, zf - 1);
    
    // Trilinear interpolation with fade function
    const float x1 = lerp(g000, g100, u);
    const float x2 = lerp(g010, g110, u);
    const float y1 = lerp(x1, x2, v);
    
    const float x3 = lerp(g001, g101, u);
    const float x4 = lerp(g011, g111, u);
    const float y2 = lerp(x3, x4, v);
    
    return lerp(y1, y2, w);
}

float NoiseGenerator::perlin2D(float x, float z, float scale, int octaves, float persistence, float lacunarity) const {
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    
    // Add successive octaves
    for (int i = 0; i < octaves; ++i) {
        total += basePerlin2D(x * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    // Normalize the result
    return total / maxValue;
}

float NoiseGenerator::perlin3D(float x, float y, float z, float scale, int octaves, float persistence, float lacunarity) const {
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    
    // Add successive octaves
    for (int i = 0; i < octaves; ++i) {
        total += basePerlin3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    // Normalize the result
    return total / maxValue;
}

std::vector<float> NoiseGenerator::generateHeightMap(int chunkX, int chunkZ, int chunkSize, float baseScale, float detailScale) const {
    std::vector<float> heightMap(chunkSize * chunkSize);
    
    // World coordinates of the chunk's corner
    const float worldX = chunkX * chunkSize;
    const float worldZ = chunkZ * chunkSize;
    
    // Generate base heightmap
    for (int z = 0; z < chunkSize; ++z) {
        for (int x = 0; x < chunkSize; ++x) {
            float wx = worldX + x;
            float wz = worldZ + z;
            
            // Generate base terrain with multiple octaves
            float baseNoise = perlin2D(wx, wz, baseScale, 4, 0.5f, 2.0f);
            
            // Add detail with higher frequency noise
            float detailNoise = perlin2D(wx, wz, detailScale, 2, 0.5f, 2.0f) * 0.1f;
            
            // Combine base and detail noise
            float height = baseNoise + detailNoise;
            
            // Normalize to [0, 1] range
            height = (height + 1.0f) * 0.5f;
            
            // Store in the heightmap
            heightMap[z * chunkSize + x] = height;
        }
    }
    
    return heightMap;
}

std::vector<float> NoiseGenerator::generate3DNoiseField(int chunkX, int chunkY, int chunkZ, int chunkSize, int chunkHeight, float scale) const {
    std::vector<float> noiseField(chunkSize * chunkHeight * chunkSize);
    
    // World coordinates of the chunk's corner
    const float worldX = chunkX * chunkSize;
    const float worldY = chunkY * chunkHeight;
    const float worldZ = chunkZ * chunkSize;
    
    // Generate 3D noise field
    for (int y = 0; y < chunkHeight; ++y) {
        for (int z = 0; z < chunkSize; ++z) {
            for (int x = 0; x < chunkSize; ++x) {
                float wx = worldX + x;
                float wy = worldY + y;
                float wz = worldZ + z;
                
                // Generate 3D noise
                float noise = perlin3D(wx, wy, wz, scale, 1, 0.5f, 2.0f);
                
                // Normalize to [0, 1] range
                noise = (noise + 1.0f) * 0.5f;
                
                // Store in the noise field
                noiseField[(y * chunkSize * chunkSize) + (z * chunkSize) + x] = noise;
            }
        }
    }
    
    return noiseField;
}

float NoiseGenerator::thresholdNoise(float value, float threshold) {
    return value >= threshold ? 1.0f : 0.0f;
}

} // namespace generation
} // namespace dyg