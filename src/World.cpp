#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <SDL_stdinc.h>
#include <cfloat> // For FLT_MAX

namespace PixelPhys {

// Chunk implementation

Chunk::Chunk(int posX, int posY) : m_posX(posX), m_posY(posY), m_isDirty(true) {
    // Initialize chunk with empty cells
    m_grid.resize(WIDTH * HEIGHT, MaterialType::Empty);
    
    // Initialize pixel data for rendering (RGBA for each cell)
    m_pixelData.resize(WIDTH * HEIGHT * 4, 0);
}

MaterialType Chunk::get(int x, int y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return MaterialType::Empty;
    }
    // Position-based material access for pixel-perfect alignment
    int idx = y * WIDTH + x;
    
    // Make absolutely sure we're in bounds
    if (idx < 0 || idx >= static_cast<int>(m_grid.size())) {
        return MaterialType::Empty;
    }
    
    return m_grid[idx];
}

void Chunk::set(int x, int y, MaterialType material) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }
    
    // Position-based material access for pixel-perfect alignment
    int idx = y * WIDTH + x;
    
    // Make absolutely sure we're in bounds
    if (idx < 0 || idx >= static_cast<int>(m_grid.size())) {
        return;
    }
    
    MaterialType oldMaterial = m_grid[idx];
    if (oldMaterial != material) {
        // Always replace the existing material rather than placing on top
        m_grid[idx] = material;
        m_isDirty = true;
        
        // Update pixel data for this cell with pixel-perfect alignment
        int pixelIdx = idx * 4;
        
        // Ensure we're in bounds for the pixel data too
        if (pixelIdx < 0 || pixelIdx + 3 >= static_cast<int>(m_pixelData.size())) {
            return;
        }
        
        const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
        
        // Create a position-based variation for more natural look
        // Enhanced variation using multiple hash functions for more texture
        int posHash1 = ((x * 13) + (y * 7)) % 32;
        int posHash2 = ((x * 23) + (y * 17)) % 64;
        int posHash3 = ((x * 5) + (y * 31)) % 16;
        
        // Different variation for each color channel - INCREASED VARIATION
        int rVariation = (posHash1 % 35) - 17;  // Much stronger variation
        int gVariation = (posHash2 % 31) - 15;  // Much stronger variation
        int bVariation = (posHash3 % 27) - 13;  // Much stronger variation
        
        // Apply material-specific variation patterns
        // Various materials have their own unique texture patterns
        switch (material) {
            case MaterialType::Stone:
                // Stone has gray variations with strong texture
                rVariation = gVariation = bVariation = (posHash1 % 45) - 22;
                // Add dark speckles
                if (posHash2 % 5 == 0) {
                    rVariation -= 25;
                    gVariation -= 25;
                    bVariation -= 25;
                }
                break;
            case MaterialType::Grass:
                // Grass has strong green variations with patches
                gVariation = (posHash1 % 50) - 15; // Strong green variation
                rVariation = (posHash2 % 25) - 15; // Variation for yellowish tints
                // Add occasional darker patches
                if (posHash1 % 3 == 0) {
                    gVariation -= 20;
                    rVariation -= 10;
                }
                break;
            case MaterialType::Sand:
                // Sand has strong yellow-brown variations with visible texture
                rVariation = (posHash1 % 30) - 10;
                gVariation = (posHash1 % 25) - 12;
                bVariation = (posHash3 % 15) - 10;
                // Add occasional darker grains
                if (posHash2 % 4 == 0) {
                    rVariation -= 15;
                    gVariation -= 15;
                }
                break;
            case MaterialType::Dirt:
                // Dirt has rich brown variations with texture
                rVariation = (posHash1 % 40) - 15;
                gVariation = (posHash2 % 30) - 15;
                bVariation = (posHash3 % 20) - 12;
                // Add occasional darker and lighter patches
                if (posHash2 % 5 == 0) {
                    rVariation -= 20;
                    gVariation -= 20;
                    bVariation -= 10;
                } else if (posHash2 % 7 == 0) {
                    rVariation += 15;
                    gVariation += 10;
                }
                break;
            case MaterialType::Snow:
                // Snow has very subtle blue-white variations
                rVariation = gVariation = bVariation = (posHash1 % 7) - 3;
                break;
            case MaterialType::Sandstone:
                // Sandstone has beige-tan variations
                rVariation = (posHash1 % 16) - 8;
                gVariation = (posHash2 % 14) - 7;
                bVariation = (posHash3 % 8) - 4;
                break;
            case MaterialType::Bedrock:
                // Bedrock has dark gray variations with some texture
                rVariation = gVariation = bVariation = (posHash1 % 20) - 8;
                // Add some occasional darker spots for texture
                if (posHash2 % 8 == 0) {
                    rVariation -= 10;
                    gVariation -= 10;
                    bVariation -= 10;
                }
                break;
            case MaterialType::Gravel:
                // Gravel has strong texture with varied gray tones
                rVariation = gVariation = bVariation = (posHash1 % 35) - 17;
                // Add mixed size pebble effect
                if (posHash2 % 7 == 0) {
                    rVariation -= 25;
                    gVariation -= 25;
                    bVariation -= 25;
                } else if (posHash2 % 11 == 0) {
                    rVariation += 15;
                    gVariation += 15;
                    bVariation += 15;
                }
                break;
            case MaterialType::TopSoil:
                // Topsoil has rich brown variations with organic texture
                rVariation = (posHash1 % 25) - 10;
                gVariation = (posHash2 % 20) - 10;
                bVariation = (posHash3 % 12) - 6;
                // Add darker organic matter patches
                if (posHash2 % 4 == 0) {
                    rVariation -= 15;
                    gVariation -= 12;
                    bVariation -= 5;
                }
                break;
            case MaterialType::DenseRock:
                // Dense rock has dark blue-gray coloration with crystalline texture
                rVariation = (posHash1 % 18) - 9;
                gVariation = (posHash1 % 18) - 9;
                bVariation = (posHash1 % 22) - 9; // Slight blue tint
                // Add occasional mineral veins or crystalline structures
                if (posHash2 % 9 == 0) {
                    rVariation += 10;
                    gVariation += 12;
                    bVariation += 15; // Blueish highlights
                } else if (posHash2 % 16 == 0) {
                    rVariation -= 15;
                    gVariation -= 15;
                    bVariation -= 10; // Dark patches
                }
                break;
            case MaterialType::Water:
                // Water has blue variations with some subtle waves
                bVariation = (posHash1 % 18) - 9;
                // Slight green tint variations for depth perception
                gVariation = (posHash2 % 10) - 5;
                // Very minimal red variation
                rVariation = (posHash3 % 4) - 2;
                break;
            case MaterialType::Lava:
                // Lava has hot red-orange variations with bright spots
                rVariation = (posHash1 % 30) - 5; // More red, less reduction
                gVariation = (posHash2 % 25) - 15; // More variation in orange
                bVariation = (posHash3 % 6) - 3; // Minor blue variation
                // Add occasional bright yellow-white spots
                if (posHash2 % 10 == 0) {
                    rVariation += 20;
                    gVariation += 15;
                }
                break;
            case MaterialType::GrassStalks:
                // Grass stalks have varied green shades
                gVariation = (posHash1 % 22) - 8; // Strong green variation
                rVariation = (posHash2 % 10) - 5; // Some red variation for yellowish/brownish tints
                bVariation = (posHash3 % 8) - 4; // Minor blue variation
                break;
            case MaterialType::Fire:
                // Fire has flickering yellow-orange-red variations
                rVariation = (posHash1 % 20) - 5; // Strong red
                gVariation = (posHash2 % 30) - 15; // Varied green for yellow/orange
                bVariation = (posHash3 % 10) - 8; // Minimal blue
                // Random bright spots
                if (posHash2 % 5 == 0) {
                    rVariation += 15;
                    gVariation += 10;
                }
                break;
            case MaterialType::Oil:
                // Oil has dark brown-black variations with slight shine
                rVariation = (posHash1 % 12) - 8;
                gVariation = (posHash2 % 10) - 7;
                bVariation = (posHash3 % 8) - 6;
                // Occasional slight shine
                if (posHash2 % 12 == 0) {
                    rVariation += 8;
                    gVariation += 8;
                    bVariation += 8;
                }
                break;
            case MaterialType::FlammableGas:
                // Flammable gas has subtle greenish variations with transparency
                gVariation = (posHash1 % 15) - 5;
                rVariation = (posHash2 % 8) - 4;
                bVariation = (posHash3 % 8) - 4;
                break;
            default:
                // Default variation - still apply some texture for any other materials
                rVariation = (posHash1 % 12) - 6;
                gVariation = (posHash2 % 12) - 6;
                bVariation = (posHash3 % 12) - 6;
                break;
        }
        
        // Apply the enhanced variation to the base color
        int r = props.r + rVariation + props.varR;
        int g = props.g + gVariation + props.varG;
        int b = props.b + bVariation + props.varB;
        
        // Clamp values to valid range
        m_pixelData[pixelIdx] = std::max(0, std::min(255, r));
        m_pixelData[pixelIdx+1] = std::max(0, std::min(255, g));
        m_pixelData[pixelIdx+2] = std::max(0, std::min(255, b));
        m_pixelData[pixelIdx+3] = props.transparency;
    }
}

void Chunk::update(Chunk* chunkBelow, Chunk* chunkLeft, Chunk* chunkRight) {
    // Simplified version - just update pixel data
    updatePixelData();
}

void Chunk::updatePixelData() {
    // Update pixel data for all cells in the chunk
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType material = m_grid[idx];
            int pixelIdx = idx * 4;
            
            if (material == MaterialType::Empty) {
                // Empty cells are transparent
                m_pixelData[pixelIdx] = 0;
                m_pixelData[pixelIdx+1] = 0;
                m_pixelData[pixelIdx+2] = 0;
                m_pixelData[pixelIdx+3] = 0;
            } else {
                const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                
                // Create a position-based variation for more natural look
                // Enhanced variation using multiple hash functions for more texture
                int posHash1 = ((x * 13) + (y * 7)) % 32;
                int posHash2 = ((x * 23) + (y * 17)) % 64;
                int posHash3 = ((x * 5) + (y * 31)) % 16;
                
                // Different variation for each color channel - EXTREMELY STRONG VARIATION (5x normal)
                int rVariation = ((posHash1 % 35) - 17) * 5;  // 5x stronger variation
                int gVariation = ((posHash2 % 31) - 15) * 5;  // 5x stronger variation
                int bVariation = ((posHash3 % 27) - 13) * 5;  // 5x stronger variation
                
                // Apply material-specific variation patterns
                // Various materials have their own unique texture patterns
                switch (material) {
                    case MaterialType::Stone:
                        // Stone has gray variations with strong texture
                        rVariation = gVariation = bVariation = ((posHash1 % 45) - 22) * 5;
                        // Add dark speckles
                        if (posHash2 % 5 == 0) {
                            rVariation -= 50;
                            gVariation -= 50;
                            bVariation -= 50;
                        }
                        break;
                    case MaterialType::Grass:
                        // Grass has strong green variations with patches
                        gVariation = ((posHash1 % 50) - 15) * 5; // 5x stronger green variation
                        rVariation = ((posHash2 % 25) - 15) * 5; // 5x stronger yellowish tints
                        // Add occasional darker patches
                        if (posHash1 % 3 == 0) {
                            gVariation -= 60;
                            rVariation -= 40;
                        }
                        break;
                    case MaterialType::Sand:
                        // Sand has strong yellow-brown variations with visible texture
                        rVariation = (posHash1 % 30) - 10;
                        gVariation = (posHash1 % 25) - 12;
                        bVariation = (posHash3 % 15) - 10;
                        // Add occasional darker grains
                        if (posHash2 % 4 == 0) {
                            rVariation -= 15;
                            gVariation -= 15;
                        }
                        break;
                    case MaterialType::Dirt:
                        // Dirt has rich brown variations with texture
                        rVariation = (posHash1 % 40) - 15;
                        gVariation = (posHash2 % 30) - 15;
                        bVariation = (posHash3 % 20) - 12;
                        // Add occasional darker and lighter patches
                        if (posHash2 % 5 == 0) {
                            rVariation -= 20;
                            gVariation -= 20;
                            bVariation -= 10;
                        } else if (posHash2 % 7 == 0) {
                            rVariation += 15;
                            gVariation += 10;
                        }
                        break;
                    case MaterialType::Snow:
                        // Snow has very subtle blue-white variations
                        rVariation = gVariation = bVariation = (posHash1 % 7) - 3;
                        break;
                    case MaterialType::Sandstone:
                        // Sandstone has beige-tan variations
                        rVariation = (posHash1 % 16) - 8;
                        gVariation = (posHash2 % 14) - 7;
                        bVariation = (posHash3 % 8) - 4;
                        break;
                    case MaterialType::Bedrock:
                        // Bedrock has dark gray variations with some texture
                        rVariation = gVariation = bVariation = (posHash1 % 20) - 8;
                        // Add some occasional darker spots for texture
                        if (posHash2 % 8 == 0) {
                            rVariation -= 10;
                            gVariation -= 10;
                            bVariation -= 10;
                        }
                        break;
                    case MaterialType::Gravel:
                        // Gravel has strong texture with varied gray tones
                        rVariation = gVariation = bVariation = (posHash1 % 35) - 17;
                        // Add mixed size pebble effect
                        if (posHash2 % 7 == 0) {
                            rVariation -= 25;
                            gVariation -= 25;
                            bVariation -= 25;
                        } else if (posHash2 % 11 == 0) {
                            rVariation += 15;
                            gVariation += 15;
                            bVariation += 15;
                        }
                        break;
                    case MaterialType::TopSoil:
                        // Topsoil has rich brown variations with organic texture
                        rVariation = (posHash1 % 25) - 10;
                        gVariation = (posHash2 % 20) - 10;
                        bVariation = (posHash3 % 12) - 6;
                        // Add darker organic matter patches
                        if (posHash2 % 4 == 0) {
                            rVariation -= 15;
                            gVariation -= 12;
                            bVariation -= 5;
                        }
                        break;
                    case MaterialType::DenseRock:
                        // Dense rock has dark blue-gray coloration with crystalline texture
                        rVariation = (posHash1 % 18) - 9;
                        gVariation = (posHash1 % 18) - 9;
                        bVariation = (posHash1 % 22) - 9; // Slight blue tint
                        // Add occasional mineral veins or crystalline structures
                        if (posHash2 % 9 == 0) {
                            rVariation += 10;
                            gVariation += 12;
                            bVariation += 15; // Blueish highlights
                        } else if (posHash2 % 16 == 0) {
                            rVariation -= 15;
                            gVariation -= 15;
                            bVariation -= 10; // Dark patches
                        }
                        break;
                    case MaterialType::Water:
                        // Water has blue variations with some subtle waves
                        bVariation = (posHash1 % 18) - 9;
                        // Slight green tint variations for depth perception
                        gVariation = (posHash2 % 10) - 5;
                        // Very minimal red variation
                        rVariation = (posHash3 % 4) - 2;
                        break;
                    case MaterialType::Lava:
                        // Lava has hot red-orange variations with bright spots
                        rVariation = (posHash1 % 30) - 5; // More red, less reduction
                        gVariation = (posHash2 % 25) - 15; // More variation in orange
                        bVariation = (posHash3 % 6) - 3; // Minor blue variation
                        // Add occasional bright yellow-white spots
                        if (posHash2 % 10 == 0) {
                            rVariation += 20;
                            gVariation += 15;
                        }
                        break;
                    case MaterialType::GrassStalks:
                        // Grass stalks have varied green shades
                        gVariation = (posHash1 % 22) - 8; // Strong green variation
                        rVariation = (posHash2 % 10) - 5; // Some red variation for yellowish/brownish tints
                        bVariation = (posHash3 % 8) - 4; // Minor blue variation
                        break;
                    case MaterialType::Fire:
                        // Fire has flickering yellow-orange-red variations
                        rVariation = (posHash1 % 20) - 5; // Strong red
                        gVariation = (posHash2 % 30) - 15; // Varied green for yellow/orange
                        bVariation = (posHash3 % 10) - 8; // Minimal blue
                        // Random bright spots
                        if (posHash2 % 5 == 0) {
                            rVariation += 15;
                            gVariation += 10;
                        }
                        break;
                    case MaterialType::Oil:
                        // Oil has dark brown-black variations with slight shine
                        rVariation = (posHash1 % 12) - 8;
                        gVariation = (posHash2 % 10) - 7;
                        bVariation = (posHash3 % 8) - 6;
                        // Occasional slight shine
                        if (posHash2 % 12 == 0) {
                            rVariation += 8;
                            gVariation += 8;
                            bVariation += 8;
                        }
                        break;
                    case MaterialType::FlammableGas:
                        // Flammable gas has subtle greenish variations with transparency
                        gVariation = (posHash1 % 15) - 5;
                        rVariation = (posHash2 % 8) - 4;
                        bVariation = (posHash3 % 8) - 4;
                        break;
                        
                    // Ore material texture variations
                    case MaterialType::IronOre:
                        // Iron ore has gray-blue tints with metallic specks
                        rVariation = gVariation = (posHash1 % 25) - 12;
                        bVariation = (posHash1 % 30) - 12;
                        // Add metallic highlights
                        if (posHash2 % 4 == 0) {
                            rVariation += 25;
                            gVariation += 25;
                            bVariation += 30;
                        }
                        break;
                        
                    case MaterialType::CopperOre:
                        // Copper has orange-brown coloration with patchy texture
                        rVariation = (posHash1 % 40) - 10;  // Strong orange-red variation
                        gVariation = (posHash2 % 25) - 15;  // Less green variation
                        bVariation = (posHash3 % 15) - 10;  // Minimal blue
                        // Add verdigris tint patches
                        if (posHash2 % 6 == 0) {
                            rVariation -= 10;
                            gVariation += 15;
                            bVariation += 5;
                        }
                        break;
                        
                    case MaterialType::GoldOre:
                        // Gold has shiny yellow with highlight sparkles
                        rVariation = (posHash1 % 30) - 10;  // Strong yellow-red
                        gVariation = (posHash2 % 30) - 15;  // Yellow-green
                        bVariation = (posHash3 % 10) - 8;   // Minimal blue
                        // Add shiny spots
                        if (posHash2 % 4 == 0) {
                            rVariation += 30;
                            gVariation += 20;
                        }
                        break;
                        
                    case MaterialType::CoalOre:
                        // Coal has dark with occasional shiny bits
                        rVariation = gVariation = bVariation = (posHash1 % 12) - 9;  // Generally dark
                        // Add occasional shiny anthracite highlights
                        if (posHash2 % 7 == 0) {
                            rVariation += 20;
                            gVariation += 20;
                            bVariation += 20;
                        }
                        break;
                        
                    case MaterialType::DiamondOre:
                        // Diamond has blue-white sparkles in dark matrix
                        rVariation = (posHash1 % 20) - 10;
                        gVariation = (posHash2 % 25) - 10;
                        bVariation = (posHash3 % 35) - 10;  // More blue variation
                        // Add bright sparkles
                        if (posHash2 % 3 == 0) {
                            rVariation += 30;
                            gVariation += 40;
                            bVariation += 50;
                        }
                        break;
                        
                    case MaterialType::SilverOre:
                        // Silver has white-gray with metallic sheen
                        rVariation = gVariation = bVariation = (posHash1 % 30) - 15;
                        // Add reflective highlights
                        if (posHash2 % 5 == 0) {
                            rVariation += 30;
                            gVariation += 30;
                            bVariation += 35;  // Slightly blue tint to highlights
                        }
                        break;
                        
                    case MaterialType::EmeraldOre:
                        // Emerald has vivid green with internal facets
                        rVariation = (posHash1 % 15) - 10;  // Little red
                        gVariation = (posHash2 % 45) - 15;  // Strong green variation
                        bVariation = (posHash3 % 20) - 15;  // Some blue variation
                        // Add crystal facet highlights
                        if (posHash2 % 6 == 0) {
                            rVariation += 5;
                            gVariation += 35;
                            bVariation += 10;
                        }
                        break;
                        
                    case MaterialType::SapphireOre:
                        // Sapphire has deep blue with lighter facets
                        rVariation = (posHash1 % 15) - 12;  // Minimal red
                        gVariation = (posHash2 % 20) - 15;  // Some green
                        bVariation = (posHash3 % 50) - 15;  // Strong blue variation
                        // Add facet highlights
                        if (posHash2 % 5 == 0) {
                            rVariation += 5;
                            gVariation += 15;
                            bVariation += 40;
                        }
                        break;
                        
                    case MaterialType::RubyOre:
                        // Ruby has deep red with bright facets
                        rVariation = (posHash1 % 50) - 15;  // Strong red variation
                        gVariation = (posHash2 % 15) - 12;  // Minimal green
                        bVariation = (posHash3 % 15) - 12;  // Minimal blue
                        // Add facet highlights
                        if (posHash2 % 5 == 0) {
                            rVariation += 40;
                            gVariation += 5;
                            bVariation += 10;
                        }
                        break;
                        
                    case MaterialType::SulfurOre:
                        // Sulfur has bright yellow with matrix patterns
                        rVariation = (posHash1 % 35) - 10;  // Strong red-yellow
                        gVariation = (posHash2 % 35) - 10;  // Strong green-yellow
                        bVariation = (posHash3 % 10) - 8;   // Minimal blue
                        // Add darker matrix
                        if (posHash2 % 4 == 0) {
                            rVariation -= 30;
                            gVariation -= 30;
                        }
                        break;
                        
                    case MaterialType::QuartzOre:
                        // Quartz has white-clear crystal in gray matrix
                        rVariation = gVariation = bVariation = (posHash1 % 25) - 12;
                        // Add bright white crystal
                        if (posHash2 % 3 == 0) {
                            rVariation += 35;
                            gVariation += 35;
                            bVariation += 35;
                        }
                        break;
                        
                    case MaterialType::UraniumOre:
                        // Uranium has greenish glow spots in dark matrix
                        rVariation = (posHash1 % 15) - 10;  // Limited red
                        gVariation = (posHash2 % 40) - 15;  // Strong green variation
                        bVariation = (posHash3 % 15) - 12;  // Limited blue
                        // Add glowing spots
                        if (posHash2 % 4 == 0) {
                            rVariation += 5;
                            gVariation += 60;  // Very bright green glow
                            bVariation += 10;
                        }
                        break;
                        
                    default:
                        // Default variation - still apply some texture for any other materials
                        rVariation = (posHash1 % 12) - 6;
                        gVariation = (posHash2 % 12) - 6;
                        bVariation = (posHash3 % 12) - 6;
                        break;
                }
                
                // Apply the enhanced variation to the base color
                int r = props.r + rVariation + props.varR;
                int g = props.g + gVariation + props.varG;
                int b = props.b + bVariation + props.varB;
                
                // Clamp values to valid range
                m_pixelData[pixelIdx] = std::max(0, std::min(255, r));
                m_pixelData[pixelIdx+1] = std::max(0, std::min(255, g));
                m_pixelData[pixelIdx+2] = std::max(0, std::min(255, b));
                m_pixelData[pixelIdx+3] = props.transparency;
            }
        }
    }
    
    // Mark chunk as clean after updating
    m_isDirty = false;
}

// World implementation

World::World(int width, int height) : m_width(width), m_height(height) {
    // Compute chunk dimensions
    m_chunksX = (width + Chunk::WIDTH - 1) / Chunk::WIDTH;
    m_chunksY = (height + Chunk::HEIGHT - 1) / Chunk::HEIGHT;
    
    // Create all chunks
    m_chunks.resize(m_chunksX * m_chunksY);
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            m_chunks[y * m_chunksX + x] = std::make_unique<Chunk>(x * Chunk::WIDTH, y * Chunk::HEIGHT);
        }
    }
    
    // Initialize pixel data for rendering
    m_pixelData.resize(width * height * 4, 0);
    
    // Initialize RNG
    std::random_device rd;
    m_rng = std::mt19937(rd());
    
    std::cout << "Created world with dimensions: " << width << "x" << height << std::endl;
    std::cout << "Chunk grid size: " << m_chunksX << "x" << m_chunksY << std::endl;
}

MaterialType World::get(int x, int y) const {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return MaterialType::Empty;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    const Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return MaterialType::Empty;
    }
    
    // Get material at local coordinates
    return chunk->get(localX, localY);
}

void World::set(int x, int y, MaterialType material) {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return;
    }
    
    // Set material at local coordinates
    chunk->set(localX, localY, material);
    
    // Update pixel data
    int idx = y * m_width + x;
    int pixelIdx = idx * 4;
    
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    
    // Create a position-based variation for more natural look
    int posHash = ((x * 13) + (y * 7)) % 32;
    int variation = (posHash % 7) - 3;
    
    m_pixelData[pixelIdx] = std::max(0, std::min(255, static_cast<int>(props.r) + variation));     // R
    m_pixelData[pixelIdx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + variation));   // G
    m_pixelData[pixelIdx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + variation));   // B
    m_pixelData[pixelIdx+3] = props.transparency;  // A
}

void World::update() {
    // Update all chunks
    for (int i = 0; i < (int)m_chunks.size(); ++i) {
        auto& chunk = m_chunks[i];
        if (chunk && chunk->isDirty()) {
            int y = i / m_chunksX;
            int x = i % m_chunksX;
    
            Chunk* chunkBelow  = (y < m_chunksY - 1) ? getChunkAt(x, y + 1) : nullptr;
            Chunk* chunkLeft   = (x > 0)             ? getChunkAt(x - 1, y) : nullptr;
            Chunk* chunkRight  = (x < m_chunksX - 1) ? getChunkAt(x + 1, y) : nullptr;
    
            chunk->update(chunkBelow, chunkLeft, chunkRight);
        }
    }
    
    // Update the combined pixel data from all chunks
    updatePixelData();
}

// Simple Perlin noise implementation
float perlinNoise2D(float x, float y, unsigned int seed) {
    // Hash function for simple pseudo-random gradients
    auto hash = [seed](int x, int y) -> int {
        int h = seed + x * 374761393 + y * 668265263;
        h = (h ^ (h >> 13)) * 1274126177;
        return h ^ (h >> 16);
    };

    // Get gradient at grid point
    auto gradient = [&hash](int ix, int iy, float x, float y) -> float {
        int h = hash(ix, iy);
        // Convert hash to one of 8 unit vectors
        float angle = static_cast<float>(h % 8) * 0.7853981634f; // pi/4
        float gx = std::cos(angle);
        float gy = std::sin(angle);
        // Dot product of distance and gradient
        return gx * (x - static_cast<float>(ix)) + gy * (y - static_cast<float>(iy));
    };

    // Get grid cell coordinates
    int x0 = std::floor(x);
    int y0 = std::floor(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Get distances from grid point
    float dx0 = x - static_cast<float>(x0);
    float dy0 = y - static_cast<float>(y0);
    // Note: dx1 and dy1 aren't needed with our gradient function approach

    // Calculate gradients at grid points
    float g00 = gradient(x0, y0, x, y);
    float g10 = gradient(x1, y0, x, y);
    float g01 = gradient(x0, y1, x, y);
    float g11 = gradient(x1, y1, x, y);

    // Cubic interpolation (smootherstep)
    auto interpolate = [](float a, float b, float t) -> float {
        return a + t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f) * (b - a);
    };

    // Interpolate along x
    float x0y0x1y0 = interpolate(g00, g10, dx0);
    float x0y1x1y1 = interpolate(g01, g11, dx0);

    // Interpolate along y
    return interpolate(x0y0x1y0, x0y1x1y1, dy0);
}

// Function to get fractal noise (multiple octaves)
float fractalNoise(float x, float y, int octaves, float persistence, float scale, unsigned int seed) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += perlinNoise2D(x * scale * frequency, y * scale * frequency, seed + i) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    // Normalize
    return total / maxValue;
}

// BiomeType is already defined in World.h

void World::generate(unsigned int seed) {
    // Seed the RNG
    m_rng.seed(seed);
    
    std::cout << "Generating world with seed: " << seed << std::endl;
    
    // World generation constants
    const int WORLD_WIDTH = m_width;
    const int WORLD_HEIGHT = m_height;
    int octaves = 4;
    float persistence = 0.5f;
    
    // Biome noise - set to a value that creates distinct but natural regions
    float biomeNoiseScale = 0.001f;
    
    // Transition width - use moderate width for cleaner transitions
    // Using about 10% of world width for transition zones
    const int transitionWidth = WORLD_WIDTH / 10;
    
    // Clear the world first with empty space
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            set(x, y, MaterialType::Empty);
        }
    }
    
    // Flag to track for step-by-step rendering of cave generation
    bool renderStepByStep = true;
    
    // Step 1: Generate heightmap using Perlin noise
    std::vector<int> heightMap(WORLD_WIDTH);
    
    // Define biome regions - using wider, more varied distributions
    // Allowing more variation in biome size rather than exact thirds
    const int desertEndX = WORLD_WIDTH / 4;
    const int grasslandEndX = 3 * WORLD_WIDTH / 4;
    
    // Different noise parameters for each biome type
    const float desertNoiseScale = 0.006f;    // Higher frequency, more dunes
    const float grasslandNoiseScale = 0.003f; // Lower frequency, gentler hills
    const float mountainNoiseScale = 0.0035f; // Reduced frequency for smoother mountains
    
    // Height scale factors for each biome
    const float desertHeightScale = 0.3f;     // Moderate dunes
    const float grasslandHeightScale = 0.25f; // Flatter
    const float mountainHeightScale = 0.6f;   // Taller peaks, but not extreme
    
    // Base height level for each biome (proportion of world height)
    const float desertBaseHeight = 0.3f;      // Medium base height
    const float grasslandBaseHeight = 0.25f;  // Lower base height
    const float mountainBaseHeight = 0.4f;    // Higher base level
    
    // Generate erosion factor for desert transition (smaller values = more erosion)
    std::vector<float> erosionFactor(WORLD_WIDTH, 1.0f);
    for (int x = desertEndX - transitionWidth; x < desertEndX + transitionWidth; ++x) {
        if (x >= 0 && x < WORLD_WIDTH) {
            float t = (float)(x - (desertEndX - transitionWidth)) / (2.0f * transitionWidth);
            
            // Apply smooth easing function (cubic) 
            t = t * t * (3.0f - 2.0f * t);
            
            // Erosion is strongest in the middle of the transition zone
            float erosion = 0.6f + 0.4f * std::abs(t - 0.5f) * 2.0f;
            erosionFactor[x] = erosion;
            
            // Add some noise to the erosion factor for natural variation
            float erosionNoise = perlinNoise2D(x * 0.1f, 0.0f, seed + 523);
            erosionFactor[x] *= (0.9f + 0.2f * erosionNoise);
        }
    }
    
    // Generate base biome noise for both biome selection and height transitions
    // Need to declare this before we use it in the following section
    std::vector<float> primaryBiomeNoise(WORLD_WIDTH);
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        // Primary biome selection noise - continuous value between -1 and 1
        primaryBiomeNoise[x] = perlinNoise2D(x * biomeNoiseScale, 0.0f, seed + 123);
        // Add secondary noise at different frequency for more variation
        float secondaryNoise = perlinNoise2D(x * biomeNoiseScale * 2.0f, 0.0f, seed + 456) * 0.3f;
        primaryBiomeNoise[x] += secondaryNoise;
    }
    
    // Generate a height transition field using the same noise that will control biomes
    std::vector<float> heightTransitionFactor(WORLD_WIDTH, 0.0f);
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        // Calculate a normalized [0-1] version of the biome noise
        float biomeMappingValue = (primaryBiomeNoise[x] + 1.0f) * 0.5f; // Convert from [-1,1] to [0,1]
        // Use position as primary control with noise adding natural variation
        float positionInfluence = ((float)x / WORLD_WIDTH) * 2.0f - 1.0f; // -1.0 to 1.0
        // Convert position factor to 0-1 range
        positionInfluence = (positionInfluence + 1.0f) * 0.5f;
        // Blend position (70%) with noise (30%) for height transitions
        heightTransitionFactor[x] = positionInfluence * 0.7f + biomeMappingValue * 0.3f;
        // Ensure value is in [0,1] range
        heightTransitionFactor[x] = std::max(0.0f, std::min(1.0f, heightTransitionFactor[x]));
    }
    
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        // Determine biome terrain parameters based on the transition factor
        float currentNoiseScale, heightScale, baseHeight;
        float transitionValue = heightTransitionFactor[x];
        
        // Determine base biome for this column's height parameters
        if (transitionValue < 0.3f) {
            // Desert region and gradual transition to grassland
            float t = std::min(1.0f, transitionValue / 0.3f);
            
            // Apply smooth easing function for gradual transitions
            t = t * t * (3.0f - 2.0f * t);
            
            currentNoiseScale = desertNoiseScale * (1.0f - t) + grasslandNoiseScale * t;
            heightScale = desertHeightScale * (1.0f - t) + grasslandHeightScale * t;
            baseHeight = desertBaseHeight * (1.0f - t) + grasslandBaseHeight * t;
            
            // Apply erosion to height for desert transitions
            if (t > 0.1f && t < 0.9f) {
                float erosionAmount = (1.0f - std::abs(t - 0.5f) * 2.0f) * 0.7f; // Strongest in middle
                heightScale *= (1.0f - erosionAmount * 0.3f);
            }
        }
        else if (transitionValue < 0.7f) {
            // Grassland region
            currentNoiseScale = grasslandNoiseScale;
            heightScale = grasslandHeightScale;
            baseHeight = grasslandBaseHeight;
        }
        else {
            // Mountain region and transition from grassland
            float t = (transitionValue - 0.7f) / 0.3f; // 0-1 for mountain transition
            
            // Apply mountain-specific transition curve - foothills to peaks
            // Use a more complex transition curve - starts very gradual, then steepens
            float mountainFactor = std::pow(t, 1.2f); // Slightly sharper curve for mountains
            
            // Add noise variation for natural, uneven mountain ranges
            float mountainNoise = perlinNoise2D(x * 0.01f, 0.0f, seed + 567) * 0.4f + 0.8f;
            mountainFactor = std::pow(mountainFactor, 0.8f) * mountainNoise;
            
            // Apply to all mountain parameters for consistent transition
            currentNoiseScale = grasslandNoiseScale * (1.0f - mountainFactor) + mountainNoiseScale * mountainFactor;
            heightScale = grasslandHeightScale * (1.0f - mountainFactor) + mountainHeightScale * mountainFactor * 1.2f; // Boost mountain height
            baseHeight = grasslandBaseHeight * (1.0f - mountainFactor) + mountainBaseHeight * mountainFactor;
        }
        
        // Generate fractal noise with biome-specific parameters
        // Use more octaves for mountains to get smoother large shapes with finer details
        int biomeOctaves = octaves;
        if (transitionValue > 0.7f) {
            biomeOctaves = 6; // More octaves for mountains
        }
        
        // Second noise component for secondary features
        float mainHeight = fractalNoise(static_cast<float>(x), 0.0f, biomeOctaves, persistence, currentNoiseScale, seed);
        float detailHeight = fractalNoise(static_cast<float>(x), 10.0f, biomeOctaves + 2, persistence, currentNoiseScale * 2.0f, seed + 777);
        
        // Combine main terrain with detailed features
        float height = mainHeight * 0.8f + detailHeight * 0.2f;
        
        // Apply biome-specific scaling
        height = (height * 0.5f + 0.5f) * (WORLD_HEIGHT * heightScale);
        
        // Add biome-specific base level
        heightMap[x] = static_cast<int>(height) + static_cast<int>(WORLD_HEIGHT * baseHeight);
    }
    
    // Step 2: Generate biome map - using more noise-based approach with position influence
    std::vector<BiomeType> biomeMap(WORLD_WIDTH);
    
    // We already generated primaryBiomeNoise above, reuse that for biome selection
    
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        float biomeValue = primaryBiomeNoise[x];
        
        // Strong positional influence to ensure we get distinct biome regions
        // Use position as primary factor with noise as variation
        float positionInfluence = ((float)x / WORLD_WIDTH) * 2.0f - 1.0f; // -1.0 to 1.0
        biomeValue = positionInfluence * 0.8f + biomeValue * 0.2f; // Position is 80% of the factor
        
        // Assign biomes based on noise thresholds
        BiomeType baseBiome;
        
        // Clear biome regions with non-linear transition zones
        if (biomeValue < -0.35f) {
            // Desert region
            baseBiome = BiomeType::DESERT;
        }
        else if (biomeValue < -0.15f) {
            // Desert-grassland transition zone
            float t = (biomeValue + 0.35f) / 0.2f; // Normalize to 0-1
            t = t * t * (3.0f - 2.0f * t); // Smooth transition
            
            // Create irregular border with noise
            float transitionNoise = perlinNoise2D(x * 0.03f, 0.0f, seed + 789) * 0.4f;
            float threshold = 0.5f + transitionNoise;
            
            // Check local erosion if in original transition zone area
            if (x >= desertEndX - transitionWidth && x < desertEndX + transitionWidth) {
                float localErosion = erosionFactor[x];
                threshold += (localErosion - 1.0f) * 0.2f;
            }
            
            baseBiome = (t < threshold) ? BiomeType::DESERT : BiomeType::GRASSLAND;
        }
        else if (biomeValue < 0.15f) {
            // Grassland region
            baseBiome = BiomeType::GRASSLAND;
        }
        else if (biomeValue < 0.35f) {
            // Grassland-mountain transition zone
            float t = (biomeValue - 0.15f) / 0.2f; // Normalize to 0-1
            t = t * t * (3.0f - 2.0f * t); // Smooth transition
            
            float mountainNoise = perlinNoise2D(x * 0.02f, 0.0f, seed + 987) * 0.3f;
            float threshold = 0.5f + mountainNoise;
            
            baseBiome = (t < threshold) ? BiomeType::GRASSLAND : BiomeType::MOUNTAIN;
        }
        else {
            // Mountain region
            baseBiome = BiomeType::MOUNTAIN;
        }
        
        // Mountain peaks can appear anywhere if height is very high
        if (heightMap[x] > 0.9f * WORLD_HEIGHT) {
            biomeMap[x] = BiomeType::MOUNTAIN;
        }
        // Snow caps appear on high locations in mountain biome
        else if (baseBiome == BiomeType::MOUNTAIN && heightMap[x] > 0.75f * WORLD_HEIGHT) {
            biomeMap[x] = BiomeType::SNOW;
        }
        // Jungle can appear in grassland areas with proper noise value
        else if (baseBiome == BiomeType::GRASSLAND && primaryBiomeNoise[x] > 0.4f) {
            biomeMap[x] = BiomeType::JUNGLE;
        }
        else {
            biomeMap[x] = baseBiome;
        }
    }
    
    // Step 3: Fill terrain with blocks based on height and biome - with proper layering
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        int terrainHeight = heightMap[x];
        BiomeType biome = biomeMap[x];
        
        // Invert the terrain height to make 0 the bottom of the world and WORLD_HEIGHT the top
        terrainHeight = WORLD_HEIGHT - terrainHeight;
        
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            if (y < terrainHeight) {
                // Below ground is empty (air)
                continue; // Already filled with Empty
            } 
            else if (y == terrainHeight) {
                // Surface block at the ground level
                switch (biome) {
                    case BiomeType::DESERT:
                        set(x, y, MaterialType::Sand);
                        break;
                    case BiomeType::SNOW:
                        set(x, y, MaterialType::Snow);
                        break;
                    case BiomeType::MOUNTAIN:
                        // Mountain tops are either stone or snow depending on height
                        if (terrainHeight < WORLD_HEIGHT * 0.3f) { // Higher parts get snow
                            set(x, y, MaterialType::Snow);
                        } else {
                            set(x, y, MaterialType::Stone);
                        }
                        break;
                    case BiomeType::JUNGLE:
                    case BiomeType::GRASSLAND:
                        // Add grass stalks occasionally on top of grass
                        if (m_rng() % 10 == 0) {
                            set(x, y, MaterialType::GrassStalks);
                            // Add the grass block below
                            if (y + 1 < WORLD_HEIGHT) {
                                set(x, y + 1, MaterialType::Grass);
                            }
                        } else {
                            set(x, y, MaterialType::Grass);
                        }
                        break;
                }
            } 
            else {
                // Underground layers - each biome has distinct layering
                int depth = y - terrainHeight; // How deep we are from the surface
                
                switch (biome) {
                    case BiomeType::DESERT: {
                        // Desert layering: Sand → Sandstone → Stone → Dense Rock
                        if (depth < 12) {
                            set(x, y, MaterialType::Sand); // Top sand layer - thicker
                        } 
                        else if (depth < 45) {
                            set(x, y, MaterialType::Sandstone); // Sandstone layer - much thicker
                        } 
                        else if (depth < 75) {
                            set(x, y, MaterialType::Stone); // Deep stone - thicker
                        } 
                        else if (y > 0.85 * WORLD_HEIGHT) {
                            set(x, y, MaterialType::Bedrock); // Bottom layer
                        } 
                        else {
                            set(x, y, MaterialType::DenseRock); // Deepest layer
                        }
                        break;
                    }
                    
                    case BiomeType::GRASSLAND:
                    case BiomeType::JUNGLE: {
                        // Grassland layering: Grass → Topsoil → Dirt → Stone → Dense Rock
                        if (depth == 1) {
                            set(x, y, MaterialType::Grass); // Top grass layer
                        } 
                        else if (depth < 5) {
                            set(x, y, MaterialType::TopSoil); // Rich topsoil
                        } 
                        else if (depth < 15) {
                            set(x, y, MaterialType::Dirt); // Dirt layer
                        } 
                        else if (depth < 70) {
                            set(x, y, MaterialType::Stone); // Stone layer - much thicker
                        }
                        else if (y > 0.85 * WORLD_HEIGHT) {
                            set(x, y, MaterialType::Bedrock); // Bottom layer
                        }
                        else {
                            set(x, y, MaterialType::DenseRock); // Deepest layer
                        }
                        break;
                    }
                    
                    case BiomeType::MOUNTAIN: {
                        // Mountain layering: Snow (top) → Stone → Gravel → Dense Rock
                        if (terrainHeight < WORLD_HEIGHT * 0.3f && depth < 5) {
                            set(x, y, MaterialType::Snow); // Snow cap on highest mountains
                        }
                        else if (depth < 20) {
                            set(x, y, MaterialType::Stone); // Upper mountain rock - thicker
                        }
                        else if (depth < 30) {
                            set(x, y, MaterialType::Gravel); // Gravel layer - thicker
                        }
                        else if (depth < 75) {
                            set(x, y, MaterialType::Stone); // Middle layer stone - much thicker
                        }
                        else if (y > 0.85 * WORLD_HEIGHT) {
                            set(x, y, MaterialType::Bedrock); // Bottom layer
                        }
                        else {
                            set(x, y, MaterialType::DenseRock); // Mountain base
                        }
                        break;
                    }
                    
                    case BiomeType::SNOW: {
                        // Snow biome: Snow → Stone → Gravel → Dense Rock
                        if (depth < 8) {
                            set(x, y, MaterialType::Snow); // Snow layer
                        }
                        else if (depth < 20) {
                            set(x, y, MaterialType::Stone); // Stone layer
                        }
                        else if (depth < 30) {
                            set(x, y, MaterialType::Gravel); // Gravel transition
                        }
                        else if (y > 0.85 * WORLD_HEIGHT) {
                            set(x, y, MaterialType::Bedrock); // Bottom layer 
                        }
                        else {
                            set(x, y, MaterialType::DenseRock); // Deep rock
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Step 4: Add biome transition features for more natural borders
    
    // Desert-Grassland transition - add erosion features
    for (int x = desertEndX - transitionWidth; x < desertEndX + transitionWidth; ++x) {
        if (x >= 0 && x < WORLD_WIDTH) {
            // Add scattered sand patches and exposed rock in the transition
            for (int y = 0; y < WORLD_HEIGHT; ++y) {
                if (get(x, y) == MaterialType::Grass || get(x, y) == MaterialType::TopSoil) {
                    // Use noise to create patches of sand over grass (erosion patterns)
                    float sandNoise = perlinNoise2D(x * 0.1f, y * 0.1f, seed + 345);
                    if (sandNoise > 0.4f && erosionFactor[x] < 0.85f) {
                        set(x, y, MaterialType::Sand);
                    }
                }
                else if (get(x, y) == MaterialType::Dirt) {
                    // Occasionally expose sandstone where dirt would be
                    float stoneNoise = perlinNoise2D(x * 0.08f, y * 0.08f, seed + 678);
                    if (stoneNoise > 0.6f && erosionFactor[x] < 0.8f) {
                        set(x, y, MaterialType::Sandstone);
                    }
                }
            }
        }
    }
    
    // Grassland-Mountain transition - add foothills and exposed rock features
    for (int x = grasslandEndX - transitionWidth; x < grasslandEndX + transitionWidth; ++x) {
        if (x >= 0 && x < WORLD_WIDTH) {
            // Calculate a transition factor for gradual change
            float t = (float)(x - (grasslandEndX - transitionWidth)) / (2.0f * transitionWidth);
            t = t * t; // Squared to make it more mountain-like toward the end
            
            for (int y = 0; y < WORLD_HEIGHT; ++y) {
                if (get(x, y) == MaterialType::Grass) {
                    // Occasionally replace grass with stone for rocky outcrops
                    float rockNoise = perlinNoise2D(x * 0.05f, y * 0.05f, seed + 123);
                    if (rockNoise > 0.8f - t * 0.3f) { // More rocks as we get closer to mountains
                        set(x, y, MaterialType::Stone);
                    }
                }
                else if (get(x, y) == MaterialType::TopSoil || get(x, y) == MaterialType::Dirt) {
                    // Replace some dirt with gravel/stone (rocky soil) for a more mountain-like transition
                    float soilNoise = perlinNoise2D(x * 0.07f, y * 0.07f, seed + 234);
                    if (soilNoise > 0.7f - t * 0.4f) {
                        if (soilNoise > 0.85f) {
                            set(x, y, MaterialType::Stone); // Occasional stone outcrops
                        } else {
                            set(x, y, MaterialType::Gravel); // Gravelly soil transition
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "Enhanced terrain generation complete." << std::endl;
    
    // Step 5: Generate ore deposits
    // Step 6: Generate caves
    if (renderStepByStep) {
        // Update pixel data after terrain generation but before caves
        updatePixelData();
        std::cout << "Terrain generated. Build and render to see the result before cave generation." << std::endl;
        std::cout << "Press any key to continue with Perlin worm cave generation..." << std::endl;
        // Here we'd pause in an interactive application
    }
    
    // Generate caves using Perlin worms method
    generateCavesCellularAutomata(seed);
    
    if (renderStepByStep) {
        // Update pixel data after cave generation
        updatePixelData();
        std::cout << "Perlin worm caves generated. Build and render to see the final cave system." << std::endl;
        std::cout << "Press any key to continue with ore generation..." << std::endl;
        // Here we'd pause in an interactive application
    }
    
    std::cout << "Generating ore deposits..." << std::endl;
    
    // Create ore distribution based on biome types
    struct OrePlacement {
        MaterialType oreType;
        float frequency;  // How common the ore is (higher = more common)
        int minDepth;     // Minimum depth from surface (0 = at surface)
        int maxDepth;     // Maximum depth (WORLD_HEIGHT = all the way to bottom)
        int minVeinSize;  // Minimum vein size
        int maxVeinSize;  // Maximum vein size
        float density;    // Ore density within veins (0-1)
        int maxRadius;    // Maximum radius for ore clusters
        BiomeType preferredBiome; // Biome where this ore is most common (optional)
    };
    
    std::vector<OrePlacement> oreTypes = {
        // Common ores found in all biomes
        {MaterialType::CoalOre,     0.7f,  10, WORLD_HEIGHT, 8,  20, 0.7f, 2, BiomeType::GRASSLAND},
        {MaterialType::IronOre,     0.5f,  15, WORLD_HEIGHT, 5,  15, 0.6f, 2, BiomeType::MOUNTAIN},
        {MaterialType::CopperOre,   0.5f,  10, WORLD_HEIGHT, 5,  15, 0.6f, 2, BiomeType::JUNGLE},
        
        // Mid-tier ores
        {MaterialType::SilverOre,   0.3f,  30, WORLD_HEIGHT, 4,  12, 0.5f, 2, BiomeType::SNOW},
        {MaterialType::GoldOre,     0.2f,  40, WORLD_HEIGHT, 3,  10, 0.4f, 2, BiomeType::DESERT},
        
        // Rare gems with biome preference
        {MaterialType::EmeraldOre,  0.15f, 50, WORLD_HEIGHT, 3,  8,  0.3f, 1, BiomeType::JUNGLE},
        {MaterialType::SapphireOre, 0.15f, 50, WORLD_HEIGHT, 3,  8,  0.3f, 1, BiomeType::SNOW},
        {MaterialType::RubyOre,     0.15f, 50, WORLD_HEIGHT, 3,  8,  0.3f, 1, BiomeType::MOUNTAIN},
        
        // Biome-specific ores
        {MaterialType::SulfurOre,   0.25f, 30, WORLD_HEIGHT, 4,  12, 0.5f, 2, BiomeType::DESERT},
        {MaterialType::QuartzOre,   0.4f,  20, WORLD_HEIGHT, 5,  15, 0.6f, 2, BiomeType::MOUNTAIN},
        
        // Super rare deposits
        {MaterialType::DiamondOre,  0.08f, 70, WORLD_HEIGHT, 2,  6,  0.2f, 1, BiomeType::MOUNTAIN},
        {MaterialType::UraniumOre,  0.06f, 80, WORLD_HEIGHT, 2,  5,  0.2f, 1, BiomeType::DESERT}
    };
    
    // For each ore type, generate multiple veins throughout the world
    std::cout << "Generating ore veins..." << std::endl;
    
    // Increased ore amount for better visibility
    int totalOreVeins = 3500; // Even more ore veins
    int smallVeins = 1500;    // Additional small veins to fill in gaps
    
    for (int i = 0; i < totalOreVeins; i++) {
        // Select a random ore type from our list
        const OrePlacement& ore = oreTypes[m_rng() % oreTypes.size()];
        
        // Random position in the world
        int x = m_rng() % WORLD_WIDTH;
        int y = m_rng() % WORLD_HEIGHT;
        
        // Skip positions too close to the surface
        int minDepth = 15; // Minimum depth from surface to generate ores
        
        // Find surface at this x position
        int surfaceY = 0;
        for (int sy = 0; sy < WORLD_HEIGHT; sy++) {
            MaterialType material = get(x, sy);
            if (material != MaterialType::Empty) {
                surfaceY = sy;
                break;
            }
        }
        
        // Check depth
        int depth = y - surfaceY;
        if (depth < minDepth || y >= WORLD_HEIGHT - 10) {
            continue; // Too close to surface or bottom
        }
        
        // Determine biome based on surface and use for ore preference
        BiomeType localBiome = BiomeType::GRASSLAND; // Default
        MaterialType surfaceMaterial = get(x, surfaceY);
        
        if (surfaceMaterial == MaterialType::Sand) {
            localBiome = BiomeType::DESERT;
        } else if (surfaceMaterial == MaterialType::Snow) {
            localBiome = BiomeType::SNOW;
        } else if (surfaceMaterial == MaterialType::Stone) {
            localBiome = BiomeType::MOUNTAIN;
        } else if (surfaceMaterial == MaterialType::Grass) {
            // Check for jungle vs grassland
            float jungleNoise = perlinNoise2D(x * 0.01f, y * 0.01f, seed + 888);
            localBiome = (jungleNoise > 0.4f) ? BiomeType::JUNGLE : BiomeType::GRASSLAND;
        }
        
        // Adjust generation chance based on biome preference
        float chance = 1.0f;
        if (localBiome == ore.preferredBiome) {
            chance = 1.5f; // Higher chance in preferred biome
        } else {
            chance = 0.5f; // Lower chance elsewhere
        }
        
        // Apply depth-based chance adjustment
        if (y > WORLD_HEIGHT * 0.7f) { // Deeper = more rare ores
            // Deep area - favor rare ores
            if (ore.oreType == MaterialType::DiamondOre || 
                ore.oreType == MaterialType::UraniumOre ||
                ore.oreType == MaterialType::GoldOre) {
                chance *= 2.0f;
            }
        } else if (y < WORLD_HEIGHT * 0.4f) {
            // Upper area - favor common ores
            if (ore.oreType == MaterialType::CoalOre || 
                ore.oreType == MaterialType::IronOre ||
                ore.oreType == MaterialType::CopperOre) {
                chance *= 2.0f;
            }
        }
        
        // Final check if we should generate this vein
        if (static_cast<float>(m_rng()) / m_rng.max() < chance * ore.frequency * 5.0f) { // Multiplied by 5 for more ore
            // Check if position is valid for ore placement
            if (isValidPositionForOre(x, y)) {
                // Significantly larger vein size for better visibility
                int veinSize = std::max(15, ore.minVeinSize * 3) + m_rng() % (ore.maxVeinSize * 3 - ore.minVeinSize * 3 + 10);
                
                // Generate the ore vein with boosted density and larger radius
                float adjustedDensity = std::min(1.0f, ore.density * 1.8f); // Higher density
                int adjustedRadius = ore.maxRadius * 3; // Triple the radius
                
                std::cout << "Generating " << static_cast<int>(ore.oreType) << " vein at " << x << "," << y 
                         << " (size: " << veinSize << ", biome: " << static_cast<int>(localBiome) << ")" << std::endl;
                
                generateOreVein(x, y, ore.oreType, veinSize, adjustedDensity, adjustedRadius, localBiome);
            }
        }
    }
    
    // Generate additional small ore veins to fill in gaps
    std::cout << "Generating small ore veins..." << std::endl;
    
    for (int i = 0; i < smallVeins; i++) {
        // Select a random ore type from our list
        const OrePlacement& ore = oreTypes[m_rng() % oreTypes.size()];
        
        // Random position in the world
        int x = m_rng() % WORLD_WIDTH;
        int y = m_rng() % WORLD_HEIGHT;
        
        // Skip positions too close to the surface
        int minDepth = 8; // Smaller minimum depth for small veins
        
        // Find surface at this x position
        int surfaceY = 0;
        for (int sy = 0; sy < WORLD_HEIGHT; sy++) {
            MaterialType material = get(x, sy);
            if (material != MaterialType::Empty) {
                surfaceY = sy;
                break;
            }
        }
        
        // Check depth
        int depth = y - surfaceY;
        if (depth < minDepth || y >= WORLD_HEIGHT - 10) {
            continue; // Too close to surface or bottom
        }
        
        // Determine biome based on surface (for future use if needed)
        // We keep this code even though it's currently unused
        MaterialType surfaceMaterial = get(x, surfaceY);
        
        // Check if position is valid for ore placement
        if (isValidPositionForOre(x, y)) {
            // Small vein size (2-8)
            int veinSize = 2 + (m_rng() % 7);
            
            // Density is high for these small veins
            float adjustedDensity = std::min(1.0f, ore.density * 2.0f);
            
            // Small radius (1-3)
            int radius = 1 + (m_rng() % 3);
            
            // Generate the ore vein with completely random starting angle
            float randomAngle = static_cast<float>(m_rng() % 1000) / 1000.0f * 2.0f * 3.14159f;
            
            // Place a small cluster directly
            placeOreCluster(x, y, ore.oreType, radius, adjustedDensity);
            
            // Create short random vein from the cluster
            for (int step = 0; step < veinSize; step++) {
                // Randomize direction completely for each step to avoid directional bias
                randomAngle = static_cast<float>(m_rng() % 1000) / 1000.0f * 2.0f * 3.14159f;
                
                int dx = static_cast<int>(std::cos(randomAngle) + 0.5f);
                int dy = static_cast<int>(std::sin(randomAngle) + 0.5f);
                
                // Ensure movement
                if (dx == 0 && dy == 0) {
                    dx = m_rng() % 3 - 1;
                    dy = (dx == 0) ? (m_rng() % 2 ? 1 : -1) : (m_rng() % 3 - 1);
                }
                
                x += dx;
                y += dy;
                
                if (isValidPositionForOre(x, y)) {
                    placeOreCluster(x, y, ore.oreType, radius, adjustedDensity);
                }
            }
        }
    }
    
    std::cout << "Ore generation complete." << std::endl;
    
    if (renderStepByStep) {
        // Update pixel data after ore generation
        updatePixelData();
        std::cout << "Ore deposits generated. Build and render to see the final world." << std::endl;
    } else {
        // Update pixel data
        updatePixelData();
    }
}

Chunk* World::getChunkAt(int x, int y) {
    if (x < 0 || x >= m_chunksX || y < 0 || y >= m_chunksY) {
        return nullptr;
    }
    
    int idx = y * m_chunksX + x;
    return m_chunks[idx].get();
}

const Chunk* World::getChunkAt(int x, int y) const {
    if (x < 0 || x >= m_chunksX || y < 0 || y >= m_chunksY) {
        return nullptr;
    }
    
    int idx = y * m_chunksX + x;
    return m_chunks[idx].get();
}

void World::worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const {
    chunkX = worldX / Chunk::WIDTH;
    chunkY = worldY / Chunk::HEIGHT;
    localX = worldX % Chunk::WIDTH;
    localY = worldY % Chunk::HEIGHT;
}

bool World::isValidPosition(int x, int y) const {
    return (x >= 0 && x < m_width && y >= 0 && y < m_height);
}

bool World::isValidPositionForOre(int x, int y) const {
    if (!isValidPosition(x, y)) {
        return false;
    }
    
    // Check if the material at this position is suitable for ore replacement
    MaterialType material = get(x, y);
    
    // Suitable host rocks for ores
    return (material == MaterialType::Stone || 
            material == MaterialType::DenseRock || 
            material == MaterialType::Sandstone);
}

void World::generateOreVein(int startX, int startY, MaterialType oreType, int maxSize, float density, int maxRadius, BiomeType biome) {
    // Drunkard's walk algorithm for ore generation
    int x = startX;
    int y = startY;
    
    // Skip if starting position is invalid
    if (!isValidPositionForOre(x, y)) {
        return;
    }
    
    // Create cluster at starting point
    placeOreCluster(x, y, oreType, 1 + (m_rng() % maxRadius), density);
    
    int steps = 0;
    int veinsPlaced = 1; // Count starting cluster as first vein
    
    // Different ore types have different "personalities" for their vein generation
    float directionRandomness = 0.3f;   // How random the direction changes are (0-1)
    float branchChance = 0.15f;         // Chance to create a branch (0-1)
    float upwardBias = 0.0f;            // Bias toward moving upward (-1 to 1)
    float horizontalBias = 0.0f;        // Bias toward moving horizontally (-1 to 1)
    int minStepsBetweenClusters = 2;    // Minimum steps before placing another cluster
    int maxStepsBetweenClusters = 5;    // Maximum steps before placing another cluster
    int stepsToNextCluster = minStepsBetweenClusters + m_rng() % (maxStepsBetweenClusters - minStepsBetweenClusters + 1);
    
    // Customize vein behavior based on ore type and biome
    switch (oreType) {
        case MaterialType::IronOre:
            // Iron forms large, somewhat random veins with occasional branches
            directionRandomness = 0.4f;
            branchChance = 0.2f;
            break;
            
        case MaterialType::CopperOre:
            // Copper forms meandering, very branchy veins
            directionRandomness = 0.5f;
            branchChance = 0.25f;
            break;
            
        case MaterialType::GoldOre:
            // Gold forms more directed veins that tend to go horizontal
            directionRandomness = 0.3f;
            branchChance = 0.15f;
            horizontalBias = 0.3f;
            break;
            
        case MaterialType::CoalOre:
            // Coal forms large, winding, horizontal layers
            directionRandomness = 0.25f;
            branchChance = 0.3f;
            horizontalBias = 0.5f;
            maxStepsBetweenClusters = 3; // More dense
            break;
            
        case MaterialType::DiamondOre:
            // Diamond forms small, tight clusters with few branches
            directionRandomness = 0.2f;
            branchChance = 0.05f;
            break;
            
        case MaterialType::SilverOre:
            // Silver forms thin veins that tend to go up
            directionRandomness = 0.35f;
            branchChance = 0.1f;
            upwardBias = 0.2f;
            break;
            
        case MaterialType::EmeraldOre:
            // Emerald in jungle biomes forms in small vertical clusters
            if (biome == BiomeType::JUNGLE) {
                directionRandomness = 0.2f;
                branchChance = 0.05f;
                upwardBias = 0.4f;
            }
            break;
            
        case MaterialType::SapphireOre:
            // Sapphire in snow biomes forms crystalline clusters
            if (biome == BiomeType::SNOW) {
                directionRandomness = 0.15f; // Very directed
                branchChance = 0.1f;
                minStepsBetweenClusters = 4; // More spread out
                maxStepsBetweenClusters = 8;
            }
            break;
            
        case MaterialType::RubyOre:
            // Ruby in mountains forms in veins that follow mountain contours
            if (biome == BiomeType::MOUNTAIN) {
                directionRandomness = 0.3f;
                horizontalBias = 0.3f;
            }
            break;
            
        case MaterialType::SulfurOre:
            // Sulfur in desert forms in horizontal layers with occasional upward pockets
            if (biome == BiomeType::DESERT) {
                directionRandomness = 0.3f;
                horizontalBias = 0.4f;
                branchChance = 0.25f;
            }
            break;
            
        case MaterialType::QuartzOre:
            // Quartz forms crystalline structures in vertical veins
            directionRandomness = 0.25f;
            upwardBias = 0.3f;
            break;
            
        case MaterialType::UraniumOre:
            // Uranium forms small, isolated pockets
            directionRandomness = 0.5f; // Very random
            branchChance = 0.05f; // Few branches
            minStepsBetweenClusters = 5; // Very spread out
            maxStepsBetweenClusters = 10;
            break;
            
        default:
            // Generic ore behavior
            break;
    }
    
    // Initial direction - use uniform distribution across all angles
    // Random angle in radians (0 to 2π)
    float angle = static_cast<float>(m_rng() % 1000) / 1000.0f * 2.0f * 3.14159f;
    
    // Use the ore type to vary the starting angle
    angle += static_cast<float>(static_cast<unsigned int>(oreType) * 0.753f);
    // Keep angle in range [0, 2π)
    angle = std::fmod(angle, 2.0f * 3.14159f);
    
    // Main vein generation loop
    while (veinsPlaced < maxSize) {
        // Change direction with more randomness to prevent directional bias
        // Mix between Perlin noise and pure randomness
        float noiseVal = perlinNoise2D(x * 0.1f, y * 0.1f, static_cast<unsigned int>(oreType) + 123);
        float randomVal = static_cast<float>(m_rng() % 1000) / 1000.0f * 2.0f - 1.0f; // -1 to 1
        
        // Return to a more controlled, predictable angle change
    float angleChange = (noiseVal * 2.0f - 1.0f) * directionRandomness * 3.14159f;
    
    // Occasionally make a turn
    if (m_rng() % 10 == 0) {
        angleChange = (m_rng() % 60) * 3.14159f / 180.0f; // 0-60 degree turn
        if (m_rng() % 2) angleChange *= -1.0f; // 50% chance of left or right
    }
    
    angle += angleChange;
        
        // Apply biases
        if (horizontalBias > 0) {
            // Bias toward horizontal movement: push angle toward 0 or π
            float hBias = std::sin(angle * 2.0f) * horizontalBias;
            angle -= hBias;
        }
        
        if (upwardBias > 0) {
            // Bias toward upward movement: push angle toward π/2
            float targetAngle = 3.14159f * 1.5f; // 3π/2 is upward in screen coordinates
            float diff = std::fmod(angle - targetAngle + 3.14159f, 2.0f * 3.14159f) - 3.14159f;
            angle -= diff * upwardBias * 0.1f;
        }
        
        // Move in the current direction
        int dx = static_cast<int>(std::cos(angle) + 0.5f); // Round to nearest int
        int dy = static_cast<int>(std::sin(angle) + 0.5f);
        
        // Ensure we move at least 1 tile (no zero movement)
        if (dx == 0 && dy == 0) {
            dx = m_rng() % 3 - 1; // -1, 0, or 1
            dy = (dx == 0) ? (m_rng() % 2 ? 1 : -1) : (m_rng() % 3 - 1);
        }
        
        // Move the walker
        x += dx;
        y += dy;
        
        // Boundary check
        if (!isValidPosition(x, y)) {
            break;
        }
        
        // Place ore clusters periodically along the path
        steps++;
        if (steps >= stepsToNextCluster) {
            // Only place if this is a valid position for ore
            if (isValidPositionForOre(x, y)) {
                // Place ore cluster with variable radius based on ore type
                int radius = 1 + (m_rng() % maxRadius);
                
                // Use Perlin noise to create varied cluster shapes based on position
                float clusterNoise = perlinNoise2D(x * 0.2f, y * 0.2f, static_cast<unsigned int>(oreType) + 456);
                int adjustedRadius = static_cast<int>(radius * (0.8f + clusterNoise * 0.4f));
                
                placeOreCluster(x, y, oreType, adjustedRadius, density);
                veinsPlaced++;
                
                // Set next cluster placement
                stepsToNextCluster = minStepsBetweenClusters + m_rng() % (maxStepsBetweenClusters - minStepsBetweenClusters + 1);
                steps = 0;
                
                // Add a moderate chance of branching
                // Not too many branches to maintain ore vein feel rather than chaotic networks
                
                if (static_cast<float>(m_rng()) / m_rng.max() < branchChance && veinsPlaced < maxSize - 1) {
                    // Choose a branch angle roughly perpendicular to current direction
                    // This creates more natural looking veins with orthogonal branches
                    float branchAngle = angle + (3.14159f / 2.0f);
                    if (m_rng() % 2) branchAngle += 3.14159f; // 50% chance to go other direction
                        int branchX = x;
                        int branchY = y;
                    
                    // Move the branch start position slightly away from main vein
                    branchX += static_cast<int>(std::cos(branchAngle) + 0.5f);
                    branchY += static_cast<int>(std::sin(branchAngle) + 0.5f);
                    
                    // Create smaller branch vein
                    if (isValidPosition(branchX, branchY)) {
                        // Make branch veins much larger - closer to main vein size
                        int branchSize = std::max(5, maxSize / 2); // Larger branch size
                        int remainingSize = maxSize - veinsPlaced;
                        branchSize = std::min(branchSize, remainingSize - 1);
                        
                        // Create branch vein - use larger branch veins
                        generateOreVeinBranch(branchX, branchY, oreType, branchSize, density, maxRadius, branchAngle, biome);
                        veinsPlaced += branchSize;
                    }
                }
                }
            }
            
            // Stop if we've placed all the ore clusters
            if (veinsPlaced >= maxSize) {
                break;
            }
        }
    }


void World::generateOreVeinBranch(int startX, int startY, MaterialType oreType, int maxSize, float density, 
                                 int maxRadius, float startAngle, BiomeType biome) {
    // Similar to main vein generation but used for branches
    int x = startX;
    int y = startY;
    
    if (!isValidPositionForOre(x, y)) {
        return;
    }
    
    // Place initial cluster
    placeOreCluster(x, y, oreType, 1 + (m_rng() % maxRadius), density);
    
    int steps = 0;
    int veinsPlaced = 1;
    float angle = startAngle;
    
    // Branch veins are now more substantial with more clusters
    float directionRandomness = 0.3f; // More randomness
    int minStepsBetweenClusters = 1;  // Place clusters more frequently
    int maxStepsBetweenClusters = 3;
    int stepsToNextCluster = minStepsBetweenClusters + m_rng() % (maxStepsBetweenClusters - minStepsBetweenClusters + 1);
    
    // Reduced version of main vein generation
    while (veinsPlaced < maxSize) {
        // Get noise value for angle change
        float noiseVal = perlinNoise2D(x * 0.1f, y * 0.1f, static_cast<unsigned int>(oreType) + 789);
        
        // Simple angle change based on perlin noise
        float angleChange = (noiseVal * 2.0f - 1.0f) * 0.3f * 3.14159f;
        
        // Occasional turn for variety (20% chance)
        if (m_rng() % 5 == 0) {
            // Small turn up to 45 degrees
            angleChange = (m_rng() % 45) * 3.14159f / 180.0f;
            if (m_rng() % 2) angleChange *= -1.0f;
        }
        
        angle += angleChange;
        
        // Move in the current direction
        int dx = static_cast<int>(std::cos(angle) + 0.5f);
        int dy = static_cast<int>(std::sin(angle) + 0.5f);
        
        // Ensure we move at least 1 tile
        if (dx == 0 && dy == 0) {
            dx = m_rng() % 3 - 1;
            dy = (dx == 0) ? (m_rng() % 2 ? 1 : -1) : (m_rng() % 3 - 1);
        }
        
        x += dx;
        y += dy;
        
        if (!isValidPosition(x, y)) {
            break;
        }
        
        steps++;
        if (steps >= stepsToNextCluster) {
            if (isValidPositionForOre(x, y)) {
                // Branch veins now have larger clusters
                int radius = 2 + (m_rng() % std::max(2, maxRadius));
                placeOreCluster(x, y, oreType, radius, density);
                veinsPlaced++;
                
                stepsToNextCluster = minStepsBetweenClusters + m_rng() % (maxStepsBetweenClusters - minStepsBetweenClusters + 1);
                steps = 0;
            }
            
            if (veinsPlaced >= maxSize) {
                break;
            }
        }
    }
}

void World::placeOreCluster(int centerX, int centerY, MaterialType oreType, int radius, float density) {
    // Place a cluster of ore centered at (centerX, centerY)
    // Use a slightly larger radius check (1.1x) to make clusters a bit more irregular
    float radiusSquared = radius * radius * 1.1f;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Skip if outside the circle radius (with some noise for irregular edges)
            float distSquared = dx*dx + dy*dy;
            if (distSquared > radiusSquared) {
                continue;
            }
            
            int x = centerX + dx;
            int y = centerY + dy;
            
            if (!isValidPosition(x, y)) {
                continue;
            }
            
            // Only place ore within appropriate materials
            if (!isValidPositionForOre(x, y)) {
                continue;
            }
            
            // Use Perlin noise for irregular cluster shapes
            float noise = perlinNoise2D(x * 0.5f, y * 0.5f, static_cast<unsigned int>(oreType));
            
            // Adjust density based on distance from center and noise
            float distFactor = 1.0f - (std::sqrt(dx*dx + dy*dy) / radius);
            float placementChance = density * distFactor * (0.7f + noise * 0.5f);
            
            // Place ore based on adjusted density
            if (static_cast<float>(m_rng()) / m_rng.max() < placementChance) {
                set(x, y, oreType);
            }
        }
    }
}

void World::generateCavesDrunkenWalk(unsigned int seed) {
    // Initialize RNG with seed for cave generation
    std::mt19937 caveRng(seed + 12345); // Different seed offset for caves
    
    std::cout << "Generating caves using drunken walk algorithm..." << std::endl;
    
    // Parameters for Terraria-like worm tunnels
    // Calculate number of caves based on world size (similar to Terraria)
    const int WORLD_WIDTH_BLOCKS = m_width;
    const int NUM_CAVES = 6; // Fixed number of main large caves
    
    // Calculate surface entrances based on world width (Terraria-like)
    const int NUM_TUNNELS = std::max(4, WORLD_WIDTH_BLOCKS / 1500); // ~4-5 per medium world
    
    // Deep connecting tunnels
    const int NUM_DEEP_TUNNELS = 3; // Connecting tunnels between caves
    
    // Much longer caves for Terraria-like tunnel systems
    const int MIN_CAVE_LENGTH = 250;
    const int MAX_CAVE_LENGTH = 600;
    
    // Longer surface tunnels
    const int MIN_TUNNEL_LENGTH = 120; 
    const int MAX_TUNNEL_LENGTH = 350;
    
    // Radius parameters for cave/tunnel carving
    const int MIN_TUNNEL_RADIUS = 2; // Minimum radius for tunnels
    const int MAX_TUNNEL_RADIUS = 3; // Maximum radius for tunnels
    const int MIN_CAVE_RADIUS = 2; // Minimum radius for caves
    const int MAX_CAVE_RADIUS = 5; // Maximum radius for caves
    
    // Noise scale for direction changes
    const float TUNNEL_TURN_FREQ = 0.1f; // How frequently tunnels turn
    const float CAVE_TURN_FREQ = 0.05f; // How frequently caves turn
    
    // Enhanced carve function that creates natural-looking cave walls with varied edges
    auto carveNaturalCave = [&](float cx, float cy, float radius, float variationScale, bool isSurfaceTunnel) {
        // Calculate base radius with some inherent variation
        float baseRadius = radius * (0.8f + (perlinNoise2D(cx * 0.1f, cy * 0.1f, seed + 123) * 0.4f));
        
        // Get ground level at this X position to protect surface
        int groundLevel = 0;
        for(int y = 0; y < m_height; ++y) {
            if(get(static_cast<int>(cx), y) != MaterialType::Empty) {
                groundLevel = y;
                break;
            }
        }
        
        // Surface protection - much stronger
        const int SURFACE_BUFFER = 15; // Increased protection zone
        
        // Define the bounds for carving with some margin
        int maxRadius = static_cast<int>(baseRadius * (1.0f + variationScale)) + 2;
        int minX = static_cast<int>(cx) - maxRadius;
        int maxX = static_cast<int>(cx) + maxRadius;
        int minY = static_cast<int>(cy) - maxRadius;
        int maxY = static_cast<int>(cy) + maxRadius;
        
        // Ensure bounds are within world
        minX = std::max(0, minX);
        maxX = std::min(m_width - 1, maxX);
        minY = std::max(0, minY);
        maxY = std::min(m_height - 1, maxY);
        
        // Carve the cave with natural variation at the edges
        for(int y = minY; y <= maxY; ++y) {
            for(int x = minX; x <= maxX; ++x) {
                // Calculate distance from center
                float dx = x - cx;
                float dy = y - cy;
                float distSq = dx*dx + dy*dy;
                
                // Get local ground level for this specific x-coordinate for better surface protection
                int localGroundLevel = groundLevel;
                if(abs(x - static_cast<int>(cx)) > 2) { // Only recalculate if we're not at the center
                    for(int ly = 0; ly < m_height; ++ly) {
                        if(get(x, ly) != MaterialType::Empty) {
                            localGroundLevel = ly;
                            break;
                        }
                    }
                }
                
                // Surface protection logic - MUCH stronger
                bool tooCloseToSurface = false;
                if(!isSurfaceTunnel || y > localGroundLevel + 5) { // Allow explicit surface tunnels to breach
                    if(y < localGroundLevel + SURFACE_BUFFER && y > localGroundLevel - 2) {
                        // Very close to surface - never carve
                        tooCloseToSurface = true;
                    }
                }
                
                if(tooCloseToSurface) {
                    continue; // Skip this position
                }
                
                // Calculate radius with variation based on position
                // This creates natural-looking irregular edges instead of perfect circles
                float noiseVal = perlinNoise2D(x * 0.15f, y * 0.15f, seed + 789);
                float angleToCenter = atan2(dy, dx);
                float positionNoise = perlinNoise2D(angleToCenter * 2.0f, 0.0f, seed + 456) * 0.3f;
                
                // Combine noise effects for natural cave edges
                float radiusVariation = variationScale * (noiseVal + positionNoise);
                float actualRadius = baseRadius * (1.0f + radiusVariation);
                
                // Only carve if within the varied radius
                if(distSq <= actualRadius * actualRadius) {
                    MaterialType type = get(x, y);
                    
                    // Only carve through carvable materials
                    if(type != MaterialType::Empty && 
                       type != MaterialType::Water && 
                       type != MaterialType::Lava &&
                       type != MaterialType::Oil &&
                       type != MaterialType::FlammableGas &&
                       type != MaterialType::Bedrock) {
                        set(x, y, MaterialType::Empty);
                    }
                }
            }
        }
    };
    
    // Distribution for tunnel angle
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    
    // 1. Generate surface tunnels (entrances to cave system)
    std::cout << "  Generating " << NUM_TUNNELS << " surface tunnels..." << std::endl;
    for(int t = 0; t < NUM_TUNNELS; ++t) {
        // Select a random position on land
        int startX = caveRng() % m_width;
        
        // Find ground level at this X position
        int startY = 0;
        for(int y = 0; y < m_height; ++y) {
            if(get(startX, y) != MaterialType::Empty) {
                startY = y;
                break;
            }
        }
        
        // Skip if we didn't find ground
        if(startY == 0) continue;
        
        // Start a little bit into the ground
        startY += 3;
        
        // Tunnel parameters - more Terraria-like worm tunnels
        // Start with a mostly downward angle, but with some variation
        float angle = 3.14159f * 1.5f + (angleDist(caveRng) - 3.14159f) * 0.3f; // More strictly downward
        float turnFrequency = TUNNEL_TURN_FREQ * 0.7f; // Reduced turn frequency for less winding
        int length = MIN_TUNNEL_LENGTH + caveRng() % (MAX_TUNNEL_LENGTH - MIN_TUNNEL_LENGTH + 1);
        
        // Vary radius with depth - wider tunnels with consistent size
        int radius = MIN_TUNNEL_RADIUS + 1; // Use a more consistent radius for caves
        
        // Generate tunnel using more natural drunken walk with improved smoothing
        float x = startX;
        float y = startY;
        
        // Track previous points to smooth the tunnel path
        std::vector<std::pair<float, float>> lastPoints;
        const int SMOOTH_POINTS = 3; // Number of points to look back for smoothing
        
        // Initialize with start point
        for (int i = 0; i < SMOOTH_POINTS; i++) {
            lastPoints.push_back({x, y});
        }
        
        // Carve entrance with surface flag to allow breach
        carveNaturalCave(x, y, radius, 0.3f, true);
        
        // Parameters for natural winding tunnels
        float preferredTurnDir = (caveRng() % 2 == 0) ? 1.0f : -1.0f; // Prefer turning one way
        float turnStrength = 0.0f; // Start with no turn
        int straightCounter = 0; // Count straight sections
        
        for(int step = 0; step < length; ++step) {
            // Use Perlin noise for smooth directional changes
            float primaryNoise = perlinNoise2D(x * turnFrequency, y * turnFrequency, seed + t * 10);
            float secondaryNoise = perlinNoise2D(x * turnFrequency * 2.0f, y * turnFrequency * 2.0f, seed + t * 20);
            
            // Calculate turn amount - smooth but with some randomness
            float baseTurn = (primaryNoise * 2.0f - 1.0f) * 0.1f; // Small baseline turn
            
            // Increase turn strength gradually when turning in the preferred direction
            if ((baseTurn > 0 && preferredTurnDir > 0) || (baseTurn < 0 && preferredTurnDir < 0)) {
                turnStrength = std::min(turnStrength + 0.02f, 0.2f); // Gradually increase turn strength
            } else {
                turnStrength = std::max(turnStrength - 0.01f, 0.0f); // Gradually decrease turn strength
            }
            
            // Apply turn with strength modifier
            float turnAmount = baseTurn * (1.0f + turnStrength) + secondaryNoise * 0.05f;
            
            // Occasional natural S-curves 
            straightCounter++;
            if (straightCounter > 15 + (caveRng() % 10)) { // After a straight section
                // Reverse preferred turn direction for natural S-curves
                preferredTurnDir *= -1.0f;
                // Slight curve ahead
                turnAmount = 0.15f * preferredTurnDir;
                straightCounter = 0; // Reset counter
            }
            
            angle += turnAmount;
            
            // Gravity bias - stronger at the beginning, weaker deeper
            float gravityInfluence = 0.05f * std::max(0.0f, 1.0f - static_cast<float>(step) / (length * 0.3f));
            angle = angle * (1.0f - gravityInfluence) + 3.14159f * 1.5f * gravityInfluence;
            
            // Move based on angle
            float moveX = cos(angle);
            float moveY = sin(angle);
            
            // Apply some path smoothing using last few points
            float smoothX = moveX;
            float smoothY = moveY;
            for (int i = 0; i < SMOOTH_POINTS; i++) {
                smoothX += 0.2f * (lastPoints[i].first - x) / SMOOTH_POINTS;
                smoothY += 0.2f * (lastPoints[i].second - y) / SMOOTH_POINTS;
            }
            
            // Normalize the movement vector
            float moveLen = std::sqrt(smoothX*smoothX + smoothY*smoothY);
            if (moveLen > 0.001f) {
                smoothX /= moveLen;
                smoothY /= moveLen;
            }
            
            // Move with slight randomness in step size for more natural tunnels
            float stepSize = 1.0f + (perlinNoise2D(x * 0.2f, y * 0.2f, seed + t) * 0.2f);
            x += smoothX * stepSize;
            y += smoothY * stepSize;
            
            // Update last points for smoothing
            lastPoints.pop_back();
            lastPoints.insert(lastPoints.begin(), {x, y});
            
            // Carve with varied edges (false = not a surface tunnel once we're inside)
            float variationScale = 0.4f; // Higher values create more jagged edges
            carveNaturalCave(x, y, radius, variationScale, false);
            
            // Stop if out of bounds
            if(!isValidPosition(x, y)) break;
            
            // Vary tunnel radius gradually for more natural look
            if(caveRng() % 10 == 0) {
                // Gradual changes in radius - no sudden jumps
                float targetRadius = MIN_TUNNEL_RADIUS + (caveRng() % (MAX_TUNNEL_RADIUS - MIN_TUNNEL_RADIUS + 1));
                radius = radius * 0.8f + targetRadius * 0.2f; // Gradual change
            }
            
            // Occasional wider sections (chambers)
            if(caveRng() % 50 == 0 && step > 20) {
                float chamberRadius = radius * (2.0f + (caveRng() % 100) / 100.0f); // 2-3x normal radius
                carveNaturalCave(x, y, chamberRadius, 0.5f, false); // More variation for chambers
                
                // Slow down the turn rate for chambers to make a more open area
                turnFrequency *= 0.5f;
            }
        }
    }
    
    // 2. Generate main cave systems
    std::cout << "  Generating " << NUM_CAVES << " large cave systems..." << std::endl;
    for(int c = 0; c < NUM_CAVES; ++c) {
        // Start deeper underground and at better locations
        int startX = caveRng() % m_width;
        int minDepth = m_height / 3; // Start at least 1/3 down
        int maxDepth = 3 * m_height / 4; // But not too close to bottom
        int startY = minDepth + caveRng() % (maxDepth - minDepth + 1);
        
        // Try to find a better starting location - away from other caves
        for (int attempts = 0; attempts < 10; attempts++) {
            int testX = caveRng() % m_width;
            int testY = minDepth + caveRng() % (maxDepth - minDepth + 1);
            
            // Check if the location is good (not empty and not too close to surface)
            if (get(testX, testY) != MaterialType::Empty) {
                bool nearEmptySpace = false;
                
                // Check if there's empty space nearby (another cave)
                for (int dy = -20; dy <= 20; dy++) {
                    for (int dx = -20; dx <= 20; dx++) {
                        int nx = testX + dx;
                        int ny = testY + dy;
                        if (isValidPosition(nx, ny) && get(nx, ny) == MaterialType::Empty) {
                            nearEmptySpace = true;
                            break;
                        }
                    }
                    if (nearEmptySpace) break;
                }
                
                // If not near any other cave, use this location
                if (!nearEmptySpace) {
                    startX = testX;
                    startY = testY;
                    break;
                }
            }
        }
        
        // Skip if position is already empty (might be another cave)
        if(get(startX, startY) == MaterialType::Empty) continue;
        
        // Cave parameters - more Terraria-like main caves
        float angle = angleDist(caveRng); // Any initial direction
        float turnFrequency = CAVE_TURN_FREQ * 0.6f; // Much smoother turns for straighter caves
        int length = MIN_CAVE_LENGTH + caveRng() % (MAX_CAVE_LENGTH - MIN_CAVE_LENGTH + 1);
        
        // Use more consistent radius for Terraria-like worm holes
        int radius = MIN_CAVE_RADIUS + 1 + caveRng() % 2; // 3-4 radius for main caves
        
        // Generate natural winding cave using enhanced drunken walk
        float x = startX;
        float y = startY;
        
        // Track previous points for smooth tunnels
        std::vector<std::pair<float, float>> lastPoints;
        const int SMOOTH_POINTS = 5; // More smoothing for main caves
        
        // Initialize with start point
        for (int i = 0; i < SMOOTH_POINTS; i++) {
            lastPoints.push_back({x, y});
        }
        
        // Carve initial area
        carveNaturalCave(x, y, radius, 0.4f, false);
        
        // Set up natural winding parameters 
        float turnStrength = 0.0f;
        float preferredTurnDir = (caveRng() % 2 == 0) ? 1.0f : -1.0f;
        int straightCounter = 0;
        float radiusVariation = 0.0f;
        float targetRadius = radius;
        
        for(int step = 0; step < length; ++step) {
            // Blend multiple noise sources at different frequencies for natural movement
            float primaryNoise = perlinNoise2D(x * turnFrequency, y * turnFrequency, seed + c * 100 + 500);
            float secondaryNoise = perlinNoise2D(x * turnFrequency * 2.5f, y * turnFrequency * 2.5f, seed + c * 200 + 250);
            float tertiaryNoise = perlinNoise2D(x * turnFrequency * 5.0f, y * turnFrequency * 5.0f, seed + c * 300);
            
            // Blend noise for natural turns
            float blendedNoise = primaryNoise * 0.6f + secondaryNoise * 0.3f + tertiaryNoise * 0.1f;
            float baseTurn = (blendedNoise * 2.0f - 1.0f) * 0.12f;
            
            // Adjust turn strength for natural winding
            if ((baseTurn > 0 && preferredTurnDir > 0) || (baseTurn < 0 && preferredTurnDir < 0)) {
                turnStrength = std::min(turnStrength + 0.015f, 0.25f);
            } else {
                turnStrength = std::max(turnStrength - 0.008f, 0.0f);
            }
            
            // Apply turn with variable strength
            float turnAmount = baseTurn * (1.0f + turnStrength);
            
            // Create natural S-curves for winding cave passages
            straightCounter++;
            if (straightCounter > 20 + (caveRng() % 15)) {
                // Switch preferred direction
                preferredTurnDir *= -1.0f;
                // Add a bit more turn at the inflection point
                turnAmount = 0.12f * preferredTurnDir;
                straightCounter = 0;
            }
            
            angle += turnAmount;
            
            // Calculate movement direction with smoothing from previous points
            float moveX = cos(angle);
            float moveY = sin(angle);
            
            // Path smoothing for natural cave bends
            float smoothX = moveX * 0.7f; // Heavily weight current direction
            float smoothY = moveY * 0.7f;
            for (int i = 0; i < SMOOTH_POINTS; i++) {
                // Add influence from previous points with weight falloff
                float weight = 0.3f * (SMOOTH_POINTS - i) / SMOOTH_POINTS;
                smoothX += weight * (lastPoints[i].first - x) / SMOOTH_POINTS;
                smoothY += weight * (lastPoints[i].second - y) / SMOOTH_POINTS;
            }
            
            // Normalize movement vector
            float moveLen = std::sqrt(smoothX*smoothX + smoothY*smoothY);
            if (moveLen > 0.001f) {
                smoothX /= moveLen;
                smoothY /= moveLen;
            }
            
            // Variable step size for more varied terrain
            float stepSize = 0.8f + (perlinNoise2D(x * 0.3f, y * 0.3f, seed + c * 50) * 0.4f);
            x += smoothX * stepSize;
            y += smoothY * stepSize;
            
            // Update last points for smoothing
            lastPoints.pop_back();
            lastPoints.insert(lastPoints.begin(), {x, y});
            
            // Gradually interpolate toward target radius for smooth transitions
            radius = radius * 0.9f + targetRadius * 0.1f;
            
            // Carve with varied edges for natural cave walls
            float variationScale = 0.5f + radiusVariation * 0.3f; // More variation for main caves
            carveNaturalCave(x, y, radius, variationScale, false);
            
            // Stop if out of bounds
            if(!isValidPosition(x, y)) break;
            
            // Gradually vary the cave radius for more natural tunnel shape 
            if (caveRng() % 15 == 0) {
                targetRadius = MIN_CAVE_RADIUS + 1 + (caveRng() % (MAX_CAVE_RADIUS - MIN_CAVE_RADIUS));
                radiusVariation = perlinNoise2D(x * 0.05f, y * 0.05f, seed + c * 400) * 0.5f + 0.5f;
            }
            
            // Create occasional larger chambers with connecting passages
            if (caveRng() % 40 == 0 && step > 30) {
                // Create a chamber with larger radius and high variation
                float chamberRadius = radius * (2.5f + radiusVariation);
                carveNaturalCave(x, y, chamberRadius, 0.7f, false);
                
                // Add some detail to the chamber for more interest
                int numDetailPoints = 3 + caveRng() % 4;
                for (int i = 0; i < numDetailPoints; i++) {
                    // Create varied chamber shapes
                    float detailAngle = (i / static_cast<float>(numDetailPoints)) * 2.0f * 3.14159f + 
                                       (caveRng() % 100) * 0.01f;
                    float detailDist = chamberRadius * 0.5f * (0.5f + (caveRng() % 100) * 0.005f);
                    float detailX = x + cos(detailAngle) * detailDist;
                    float detailY = y + sin(detailAngle) * detailDist;
                    
                    float detailRadius = chamberRadius * 0.7f * (0.7f + (caveRng() % 60) * 0.01f);
                    if (isValidPosition(detailX, detailY)) {
                        carveNaturalCave(detailX, detailY, detailRadius, 0.6f, false);
                    }
                }
                
                // Reduce turn frequency in chambers
                turnFrequency *= 0.6f;
                
                // Slow down for chambers by adding extra carving steps without movement
                int extraSteps = 1 + caveRng() % 3;
                for (int i = 0; i < extraSteps; i++) {
                    // Extra carving at same position with slight offset
                    float offsetX = x + (caveRng() % 5 - 2) * 0.5f;
                    float offsetY = y + (caveRng() % 5 - 2) * 0.5f;
                    if (isValidPosition(offsetX, offsetY)) {
                        carveNaturalCave(offsetX, offsetY, chamberRadius * 0.9f, 0.6f, false);
                    }
                }
            }
            
            // Create branches from main cave (with better control)
            if (caveRng() % 25 == 0 && step > 40) {
                // Create branch with natural angle offset
                float branchDeviation = 0.8f + (caveRng() % 60) * 0.01f; // 0.8 to 1.4 radians (45-80 degrees)
                float branchAngle = angle + (preferredTurnDir * branchDeviation);
                
                // Start branch slightly offset for a smoother connection
                float branchStartDist = radius * 0.8f;
                float branchX = x + cos(branchAngle) * branchStartDist;
                float branchY = y + sin(branchAngle) * branchStartDist;
                
                // Reduced size for branch
                float branchRadius = radius * 0.8f;
                if (branchRadius < MIN_CAVE_RADIUS) branchRadius = MIN_CAVE_RADIUS;
                
                // Shorter length for branch
                int branchLength = length / 3 + caveRng() % (length / 6);
                
                // Higher turn frequency for branches (more winding)
                float branchTurnFreq = turnFrequency * 1.5f;
                
                // Branch starting parameters
                std::vector<std::pair<float, float>> branchPoints;
                for (int i = 0; i < 3; i++) {
                    branchPoints.push_back({branchX, branchY});
                }
                
                // Generate the branch with more winding
                carveNaturalCave(branchX, branchY, branchRadius, 0.4f, false);
                
                for (int branchStep = 0; branchStep < branchLength; ++branchStep) {
                    // Similar blended noise for branch turns
                    float bNoise1 = perlinNoise2D(branchX * branchTurnFreq, branchY * branchTurnFreq, seed + c * 500 + 300);
                    float bNoise2 = perlinNoise2D(branchX * branchTurnFreq * 2.0f, branchY * branchTurnFreq * 2.0f, seed + c * 600);
                    
                    // More winding for branches
                    float branchTurn = (bNoise1 * 0.7f + bNoise2 * 0.3f) * 2.0f - 1.0f;
                    branchAngle += branchTurn * 0.2f;
                    
                    // Calculate movement with smoothing
                    float bMoveX = cos(branchAngle);
                    float bMoveY = sin(branchAngle);
                    
                    // Simple smoothing for branch
                    float bSmoothX = bMoveX;
                    float bSmoothY = bMoveY;
                    for (int i = 0; i < branchPoints.size(); i++) {
                        bSmoothX += 0.1f * (branchPoints[i].first - branchX);
                        bSmoothY += 0.1f * (branchPoints[i].second - branchY);
                    }
                    
                    // Normalize
                    float bMoveLen = std::sqrt(bSmoothX*bSmoothX + bSmoothY*bSmoothY);
                    if (bMoveLen > 0.001f) {
                        bSmoothX /= bMoveLen;
                        bSmoothY /= bMoveLen;
                    }
                    
                    // Move branch
                    branchX += bSmoothX;
                    branchY += bSmoothY;
                    
                    // Update branch points
                    branchPoints.pop_back();
                    branchPoints.insert(branchPoints.begin(), {branchX, branchY});
                    
                    // Carve branch position
                    float branchVarScale = 0.3f + (branchStep / static_cast<float>(branchLength)) * 0.2f;
                    carveNaturalCave(branchX, branchY, branchRadius, branchVarScale, false);
                    
                    // Gradually reduce branch radius toward end
                    if (branchStep > branchLength * 0.7f) {
                        branchRadius *= 0.98f; // Taper toward end
                    }
                    
                    // Stop if out of bounds
                    if (!isValidPosition(branchX, branchY)) break;
                }
            }
        }
    }
    
    // 3. Generate deep tunnels to connect existing caves
    std::cout << "  Generating " << NUM_DEEP_TUNNELS << " deep connecting tunnels..." << std::endl;
    for(int dt = 0; dt < NUM_DEEP_TUNNELS; ++dt) {
        // Start from a random position deep underground
        int startX = caveRng() % m_width;
        int minDepth = m_height / 2; // Deeper than entrance tunnels
        int maxDepth = 4 * m_height / 5;
        int startY = minDepth + caveRng() % (maxDepth - minDepth + 1);
        
        // Find a cave nearby to start from - search for empty space
        bool foundStart = false;
        for(int i = 0; i < 100; ++i) { // Try up to 100 random positions
            int testX = startX + (caveRng() % 40) - 20;
            int testY = startY + (caveRng() % 40) - 20;
            if(isValidPosition(testX, testY) && get(testX, testY) == MaterialType::Empty) {
                startX = testX;
                startY = testY;
                foundStart = true;
                break;
            }
        }
        
        if(!foundStart) continue; // Skip if we couldn't find a cave
        
        // Tunnel parameters
        float angle = angleDist(caveRng); // Any initial direction
        float turnFrequency = TUNNEL_TURN_FREQ * 0.5f; // Smoother turns for connecting tunnels
        int length = MIN_TUNNEL_LENGTH + caveRng() % (MAX_TUNNEL_LENGTH - MIN_TUNNEL_LENGTH + 1);
        int radius = MIN_TUNNEL_RADIUS + caveRng() % (MAX_TUNNEL_RADIUS - MIN_TUNNEL_RADIUS + 1);
        
        // Adjust length based on depth - deeper tunnels can be longer
        length = static_cast<int>(length * (1.0f + static_cast<float>(startY) / m_height));
        
        // Find target cave - pick a direction and try to reach another cave
        int targetX = startX, targetY = startY;
        bool foundTarget = false;
        
        for(int i = 0; i < 20; ++i) { // Try up to 20 directions
            float searchAngle = angleDist(caveRng);
            int searchDist = 50 + caveRng() % 100; // Look 50-150 tiles away
            int searchX = startX + static_cast<int>(cos(searchAngle) * searchDist);
            int searchY = startY + static_cast<int>(sin(searchAngle) * searchDist);
            
            // Check if the target point is valid and empty (part of another cave)
            if(isValidPosition(searchX, searchY) && get(searchX, searchY) == MaterialType::Empty) {
                targetX = searchX;
                targetY = searchY;
                foundTarget = true;
                break;
            }
        }
        
        if(!foundTarget) {
            // If no other cave found, just tunnel in a random direction
            angle = angleDist(caveRng);
        } else {
            // Aim toward the target cave
            angle = atan2(targetY - startY, targetX - startX);
            // Tunnels are initially aimed at target but can wander
        }
        
        // Generate connecting tunnel using the same natural drunken walk
        float x = startX;
        float y = startY;
        
        // Track previous points for smooth tunnel path
        std::vector<std::pair<float, float>> lastPoints;
        const int SMOOTH_POINTS = 4; // Number of points to look back for smoothing
        
        // Initialize with start point
        for (int i = 0; i < SMOOTH_POINTS; i++) {
            lastPoints.push_back({x, y});
        }
        
        // Carve initial area 
        carveNaturalCave(x, y, radius, 0.3f, false);
        
        for(int step = 0; step < length; ++step) {
            // Use Perlin noise for natural directional changes
            float primaryNoise = perlinNoise2D(x * turnFrequency, y * turnFrequency, seed + dt * 300 + 1000);
            float secondaryNoise = perlinNoise2D(x * turnFrequency * 2.0f, y * turnFrequency * 2.0f, seed + dt * 400);
            
            // Calculate base turn amount
            float baseTurn = (primaryNoise * 2.0f - 1.0f) * 0.1f + secondaryNoise * 0.05f;
            
            // Add the turn to our angle
            angle += baseTurn;
            
            // If we have a target, adjust angle to move toward it
            if(foundTarget && caveRng() % 5 == 0) { // 20% chance to adjust toward target
                float targetAngle = atan2(targetY - y, targetX - x);
                // Blend current angle with target angle (more weight if we're close)
                float distToTarget = std::sqrt((targetX - x) * (targetX - x) + (targetY - y) * (targetY - y));
                float targetWeight = 0.2f;
                if (distToTarget < 20.0f) {
                    // Increase target influence when getting closer
                    targetWeight = 0.2f + (20.0f - distToTarget) * 0.03f;
                }
                angle = angle * (1.0f - targetWeight) + targetAngle * targetWeight;
            }
            
            // Calculate movement with path smoothing
            float moveX = cos(angle);
            float moveY = sin(angle);
            
            // Apply path smoothing using last few points
            float smoothX = moveX * 0.7f; // Weight current direction more heavily
            float smoothY = moveY * 0.7f;
            for (int i = 0; i < SMOOTH_POINTS; i++) {
                float weight = 0.3f * (SMOOTH_POINTS - i) / SMOOTH_POINTS;
                smoothX += weight * (lastPoints[i].first - x) / SMOOTH_POINTS;
                smoothY += weight * (lastPoints[i].second - y) / SMOOTH_POINTS;
            }
            
            // Normalize the movement vector
            float moveLen = std::sqrt(smoothX*smoothX + smoothY*smoothY);
            if (moveLen > 0.001f) {
                smoothX /= moveLen;
                smoothY /= moveLen;
            }
            
            // Move with slight randomness in step size
            float stepSize = 1.0f + (perlinNoise2D(x * 0.2f, y * 0.2f, seed + dt) * 0.2f);
            x += smoothX * stepSize;
            y += smoothY * stepSize;
            
            // Update last points for smoothing
            lastPoints.pop_back();
            lastPoints.insert(lastPoints.begin(), {x, y});
            
            // Carve with natural edges, using variable edge complexity
            carveNaturalCave(x, y, radius, 0.3f, false);
            
            // Check if we reached target or empty space (another cave)
            if(isValidPosition(x, y) && get(x, y) == MaterialType::Empty) {
                // We connected to another cave - carve a bit extra to make a nice connection
                carveNaturalCave(x, y, radius * 1.5f, 0.4f, false);
                
                // Mission accomplished
                break;
            }
            
            // Stop if out of bounds
            if(!isValidPosition(x, y)) break;
            
            // Random chance to change tunnel radius gradually
            if(caveRng() % 15 == 0) {
                float targetRadius = MIN_TUNNEL_RADIUS + (caveRng() % (MAX_TUNNEL_RADIUS - MIN_TUNNEL_RADIUS + 1));
                radius = radius * 0.8f + targetRadius * 0.2f; // Gradual change
            }
        }
    }
    
    std::cout << "Drunken walk cave generation complete." << std::endl;
}

void World::generateCavesCellularAutomata(unsigned int seed) {
    std::cout << "Generating caves using Perlin worms method..." << std::endl;
    
    // Initialize RNG with seed for cave generation
    std::mt19937 caveRng(seed + 54321);
    
    const int WORLD_WIDTH = m_width;
    const int WORLD_HEIGHT = m_height;
    
    // Create distributions for cave parameters
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> radiusDist(0.5f, 1.0f);
    std::uniform_int_distribution<int> lengthDist(80, 300);
    
    // Create a simple biome map - this affects cave properties
    std::vector<BiomeType> biomeMap(WORLD_WIDTH);
    
    // Simple approximation of world's biome distribution
    const int desertEndX = WORLD_WIDTH / 4;
    const int grasslandEndX = 3 * WORLD_WIDTH / 4;
    
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        if (x < desertEndX) {
            biomeMap[x] = BiomeType::DESERT;
        } else if (x < grasslandEndX) {
            biomeMap[x] = BiomeType::GRASSLAND;
        } else {
            biomeMap[x] = BiomeType::MOUNTAIN;
        }
        
        // Refine biome based on actual surface material
        for (int y = 0; y < WORLD_HEIGHT / 4; ++y) {
            MaterialType material = get(x, y);
            if (material != MaterialType::Empty) {
                if (material == MaterialType::Sand) {
                    biomeMap[x] = BiomeType::DESERT;
                } else if (material == MaterialType::Snow) {
                    biomeMap[x] = BiomeType::SNOW;
                } else if (material == MaterialType::Stone && y < WORLD_HEIGHT / 3) {
                    biomeMap[x] = BiomeType::MOUNTAIN;
                } else if (material == MaterialType::Grass || material == MaterialType::GrassStalks) {
                    biomeMap[x] = BiomeType::GRASSLAND;
                }
                break;
            }
        }
    }
    
    // Biome-specific parameters for caves
    struct BiomeCaveParams {
        float tunnelChance;       // Chance of creating a tunnel
        float branchChance;       // Chance of branching from a tunnel
        float minRadius;          // Minimum tunnel radius
        float maxRadius;          // Maximum tunnel radius
        float noiseStrength;      // Strength of noise variation
        float turnChance;         // Chance of changing direction
        float maxTurnAngle;       // Maximum turn angle
        float verticalBias;       // Tendency to go vertical vs horizontal (positive = more vertical)
    };
    
    // Define biome-specific cave parameters
    std::map<BiomeType, BiomeCaveParams> biomeParams;
    
    // Desert - wide horizontal tunnels
    biomeParams[BiomeType::DESERT] = {
        0.3f,   // Lower tunnel chance for fewer caves
        0.2f,   // Low branch chance for sparser networks
        2.5f,   // Wider minimum radius
        4.0f,   // Wider maximum radius
        0.3f,   // Moderate noise strength
        0.07f,  // Lower turn chance for straighter tunnels
        0.2f,   // Smaller max turn angle
        -0.3f   // Strong horizontal bias
    };
    
    // Grassland - balanced cave systems
    biomeParams[BiomeType::GRASSLAND] = {
        0.4f,   // Moderate tunnel chance
        0.3f,   // Moderate branch chance
        1.8f,   // Medium minimum radius
        3.5f,   // Medium maximum radius
        0.4f,   // Moderate noise strength
        0.15f,  // Moderate turn chance
        0.4f,   // Moderate max turn angle
        0.0f    // No vertical/horizontal bias
    };
    
    // Mountain - thinner vertical tunnels
    biomeParams[BiomeType::MOUNTAIN] = {
        0.35f,  // Moderate tunnel chance
        0.25f,  // Moderate branch chance
        1.5f,   // Smaller minimum radius
        3.0f,   // Smaller maximum radius
        0.4f,   // Moderate noise strength
        0.2f,   // Higher turn chance for winding tunnels
        0.5f,   // Larger max turn angle
        0.4f    // Strong vertical bias
    };
    
    // Snow - similar to mountains but smoother
    biomeParams[BiomeType::SNOW] = {
        0.3f,   // Lower tunnel chance
        0.2f,   // Low branch chance
        1.8f,   // Medium minimum radius
        3.2f,   // Medium maximum radius
        0.3f,   // Lower noise strength for smoother tunnels
        0.15f,  // Moderate turn chance
        0.4f,   // Moderate max turn angle
        0.3f    // Moderate vertical bias
    };
    
    // Jungle - dense networks of smaller tunnels
    biomeParams[BiomeType::JUNGLE] = {
        0.5f,   // Higher tunnel chance for denser networks
        0.4f,   // Higher branch chance
        1.5f,   // Smaller minimum radius
        2.8f,   // Smaller maximum radius
        0.5f,   // Higher noise strength for more organic shapes
        0.25f,  // High turn chance for winding tunnels
        0.6f,   // Larger max turn angle
        -0.1f   // Slight horizontal bias
    };
    
    // Define different layers of caves (deeper = more tunnels)
    const int NUM_LAYERS = 4;
    
    struct LayerConfig {
        float startDepth;  // Starting depth as proportion of world height
        float endDepth;    // Ending depth as proportion of world height
        float tunnelDensityMultiplier;  // Multiplier for tunnel chance
        float radiusMultiplier;  // Multiplier for tunnel radius
    };
    
    LayerConfig layers[NUM_LAYERS] = {
        {0.20f, 0.35f, 0.7f, 0.8f},  // Upper layer - fewer, smaller caves
        {0.35f, 0.50f, 0.9f, 0.9f},  // Middle upper layer - moderate
        {0.50f, 0.70f, 1.1f, 1.0f},  // Middle deep layer - slightly more
        {0.70f, 0.90f, 1.2f, 1.1f}   // Deep layer - most caves, slightly larger
    };
    
    // Helper function to carve a circular area
    auto carveCircle = [&](int cx, int cy, float radius, float noiseStrength, unsigned int noiseSeed) {
        // Define bounds with margin
        int minX = std::max(0, static_cast<int>(cx - radius - 2));
        int maxX = std::min(WORLD_WIDTH - 1, static_cast<int>(cx + radius + 2));
        int minY = std::max(0, static_cast<int>(cy - radius - 2));
        int maxY = std::min(WORLD_HEIGHT - 1, static_cast<int>(cy + radius + 2));
        
        // Protection for surface - don't carve too close to surface
        int surfaceY = WORLD_HEIGHT;
        for (int y = 0; y < WORLD_HEIGHT / 4; ++y) {
            if (get(cx, y) != MaterialType::Empty) {
                surfaceY = y;
                break;
            }
        }
        
        // Avoid carving near surface
        if (cy < surfaceY + 10) {
            return;
        }
        
        // Carve the circle with noise variation
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                // Calculate distance from center
                float dx = x - cx;
                float dy = y - cy;
                float distSq = dx*dx + dy*dy;
                
                // Calculate noise-modified radius
                float noise = perlinNoise2D(x * 0.15f, y * 0.15f, noiseSeed);
                float angleNoise = perlinNoise2D(atan2(dy, dx) * 3.0f, 0.5f, noiseSeed + 1000) * 0.5f;
                float combinedNoise = (noise * 0.7f + angleNoise * 0.3f) * noiseStrength;
                float adjustedRadius = radius * (1.0f + combinedNoise);
                
                // Check if point is inside the noise-distorted circle
                if (distSq <= adjustedRadius * adjustedRadius) {
                    MaterialType material = get(x, y);
                    
                    // Only carve through carvable materials
                    if (material != MaterialType::Empty && 
                        material != MaterialType::Water && 
                        material != MaterialType::Lava &&
                        material != MaterialType::Oil &&
                        material != MaterialType::FlammableGas &&
                        material != MaterialType::Bedrock) {
                        
                        // Create a more gradual edge - cells near edge have chance to remain solid
                        float edgeRatio = sqrtf(distSq) / adjustedRadius;
                        if (edgeRatio < 0.8f || (edgeRatio < 0.95f && noise > 0.0f)) {
                            set(x, y, MaterialType::Empty);
                        }
                    }
                }
            }
        }
    };
    
    // Calculate number of main caves based on world size
    int numMainCaves = WORLD_WIDTH / 300; // One main cave system every 300 blocks
    
    // Ensure we have at least 5 main cave systems
    numMainCaves = std::max(5, numMainCaves);
    
    std::cout << "  Generating " << numMainCaves << " main cave systems..." << std::endl;
    
    // Generate surface entrances - one per main cave system
    std::vector<std::pair<int, int>> surfaceEntrances;
    
    for (int i = 0; i < numMainCaves; i++) {
        // Place entrances relatively evenly across world width with some randomness
        int entranceX = (i + 1) * WORLD_WIDTH / (numMainCaves + 1);
        entranceX += caveRng() % (WORLD_WIDTH / (numMainCaves + 1) / 2) - WORLD_WIDTH / (numMainCaves + 1) / 4;
        
        // Find the surface at this x position
        int surfaceY = 0;
        for (int y = 0; y < WORLD_HEIGHT / 3; ++y) {
            if (get(entranceX, y) != MaterialType::Empty) {
                surfaceY = y;
                break;
            }
        }
        
        // Skip if we couldn't find a proper surface
        if (surfaceY < 5) continue;
        
        // Add a bit of depth for the entrance
        int entranceY = surfaceY + 5;
        
        // Record this entrance
        surfaceEntrances.push_back({entranceX, entranceY});
        
        // Get biome at this position
        BiomeType biome = biomeMap[entranceX];
        
        // Create a small entrance chamber
        float entranceRadius = 3.0f + radiusDist(caveRng) * 1.5f;
        carveCircle(entranceX, entranceY, entranceRadius, 0.3f, seed + i * 100);
        
        // Start a tunnel from this entrance going downward
        float angle = 3.14159f * 1.5f + angleDist(caveRng) * 0.3f - 0.15f; // Mostly downward
        
        // Get biome parameters
        const auto& params = biomeParams[biome];
        
        // Create tunnel from this entrance
        float x = entranceX;
        float y = entranceY;
        float tunnelRadius = params.minRadius + radiusDist(caveRng) * (params.maxRadius - params.minRadius);
        int length = lengthDist(caveRng) * 1.5f; // Longer entrance tunnels
        
        for (int step = 0; step < length; ++step) {
            // Carve at current position
            carveCircle(x, y, tunnelRadius, params.noiseStrength, seed + i * 1000 + step);
            
            // Chance to change direction
            if (caveRng() % 100 < params.turnChance * 100) {
                // Apply biome's vertical bias to direction change
                float turnBias = params.verticalBias;
                float turnAngle = (angleDist(caveRng) - 3.14159f) * params.maxTurnAngle;
                
                // Apply vertical bias - makes angle more likely to turn up/down based on bias
                if (turnBias > 0) {
                    // Positive bias - more vertical tunnels
                    if (std::abs(angle - 3.14159f * 1.5f) > 0.5f) {
                        // If not already going downward, bias toward vertical
                        turnAngle += turnBias * (3.14159f * 1.5f - angle);
                    }
                } else if (turnBias < 0) {
                    // Negative bias - more horizontal tunnels
                    turnAngle += turnBias * std::sin(angle);
                }
                
                angle += turnAngle;
            }
            
            // Chance to change radius
            if (caveRng() % 100 < 10) {
                tunnelRadius = params.minRadius + radiusDist(caveRng) * (params.maxRadius - params.minRadius);
            }
            
            // Move in current direction
            x += std::cos(angle) * (tunnelRadius * 0.5f);
            y += std::sin(angle) * (tunnelRadius * 0.5f);
            
            // Check if we're still in bounds
            if (x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT) {
                break;
            }
            
            // Chance to create a branch
            if (step > 20 && caveRng() % 100 < params.branchChance * 100) {
                // Create a branch from this point
                float branchAngle = angle + (angleDist(caveRng) - 3.14159f) * 0.5f;
                float branchX = x;
                float branchY = y;
                float branchRadius = tunnelRadius * 0.8f;
                int branchLength = lengthDist(caveRng) * 0.6f; // Shorter branches
                
                for (int branchStep = 0; branchStep < branchLength; ++branchStep) {
                    // Carve at current branch position
                    carveCircle(branchX, branchY, branchRadius, params.noiseStrength, 
                               seed + i * 10000 + step * 100 + branchStep);
                    
                    // Chance to change branch direction
                    if (caveRng() % 100 < params.turnChance * 150) { // More turns in branches
                        float branchTurnAngle = (angleDist(caveRng) - 3.14159f) * params.maxTurnAngle * 1.2f;
                        branchAngle += branchTurnAngle;
                    }
                    
                    // Move branch in its direction
                    branchX += std::cos(branchAngle) * (branchRadius * 0.6f);
                    branchY += std::sin(branchAngle) * (branchRadius * 0.6f);
                    
                    // Check if branch is still in bounds
                    if (branchX < 0 || branchX >= WORLD_WIDTH || branchY < 0 || branchY >= WORLD_HEIGHT) {
                        break;
                    }
                    
                    // Chance to end the branch early
                    if (branchStep > 10 && caveRng() % 100 < 5) {
                        break;
                    }
                }
            }
            
            // Chance to create a chamber
            if (step > 50 && caveRng() % 100 < 5) {
                // Create a larger chamber at this point
                float chamberRadius = tunnelRadius * (2.0f + radiusDist(caveRng));
                carveCircle(x, y, chamberRadius, params.noiseStrength * 1.5f, 
                           seed + i * 100000 + step);
            }
        }
    }
    
    // Generate deep cave networks not connected to surface
    for (int layer = 0; layer < NUM_LAYERS; layer++) {
        const LayerConfig& layerConfig = layers[layer];
        
        int startY = static_cast<int>(WORLD_HEIGHT * layerConfig.startDepth);
        int endY = static_cast<int>(WORLD_HEIGHT * layerConfig.endDepth);
        int layerHeight = endY - startY;
        
        // Determine how many tunnels to generate in this layer
        int tunnelsInLayer = numMainCaves + layer; // More tunnels in deeper layers
        
        std::cout << "  Generating " << tunnelsInLayer << " cave networks in layer " 
                  << layer + 1 << " (depth " 
                  << static_cast<int>(layerConfig.startDepth * 100) << "%-" 
                  << static_cast<int>(layerConfig.endDepth * 100) << "%)..." << std::endl;
        
        for (int i = 0; i < tunnelsInLayer; i++) {
            // Choose random starting position within this layer
            int startX = caveRng() % WORLD_WIDTH;
            int caveStartY = static_cast<int>(WORLD_HEIGHT * layerConfig.startDepth) + 
                            (caveRng() % static_cast<int>(layerHeight * 0.8f));
            
            // Get biome at this position
            BiomeType biome = biomeMap[startX];
            
            // Get biome parameters
            const auto& params = biomeParams[biome];
            
            // Apply layer modifiers
            float adjustedTunnelChance = params.tunnelChance * layerConfig.tunnelDensityMultiplier;
            float adjustedMinRadius = params.minRadius * layerConfig.radiusMultiplier;
            float adjustedMaxRadius = params.maxRadius * layerConfig.radiusMultiplier;
            
            // Skip based on tunnel chance
            if (caveRng() % 100 >= adjustedTunnelChance * 100) {
                continue;
            }
            
            // Starting parameters
            float x = startX;
            float y = caveStartY;
            float angle = angleDist(caveRng);
            float tunnelRadius = adjustedMinRadius + radiusDist(caveRng) * (adjustedMaxRadius - adjustedMinRadius);
            int length = lengthDist(caveRng);
            
            // Create the main tunnel
            for (int step = 0; step < length; ++step) {
                // Carve at current position
                carveCircle(x, y, tunnelRadius, params.noiseStrength, seed + layer * 10000 + i * 1000 + step);
                
                // Chance to change direction
                if (caveRng() % 100 < params.turnChance * 100) {
                    // Apply biome's vertical bias to direction change
                    float turnBias = params.verticalBias;
                    float turnAngle = (angleDist(caveRng) - 3.14159f) * params.maxTurnAngle;
                    
                    // Apply vertical bias
                    if (turnBias != 0.0f) {
                        turnAngle += turnBias * std::sin(angle - 3.14159f);
                    }
                    
                    angle += turnAngle;
                }
                
                // Chance to change radius
                if (caveRng() % 100 < 10) {
                    tunnelRadius = adjustedMinRadius + radiusDist(caveRng) * (adjustedMaxRadius - adjustedMinRadius);
                }
                
                // Move in current direction
                x += std::cos(angle) * (tunnelRadius * 0.6f);
                y += std::sin(angle) * (tunnelRadius * 0.6f);
                
                // Check if we're still in layer bounds
                if (x < 0 || x >= WORLD_WIDTH || y < startY || y >= endY) {
                    break;
                }
                
                // Chance to create a branch
                if (step > 10 && caveRng() % 100 < params.branchChance * 100) {
                    // Create a branch from this point
                    float branchAngle = angle + (angleDist(caveRng) - 3.14159f) * 0.7f;
                    float branchX = x;
                    float branchY = y;
                    float branchRadius = tunnelRadius * 0.7f;
                    int branchLength = lengthDist(caveRng) * 0.4f; // Shorter branches
                    
                    for (int branchStep = 0; branchStep < branchLength; ++branchStep) {
                        // Carve at current branch position
                        carveCircle(branchX, branchY, branchRadius, params.noiseStrength, 
                                   seed + layer * 100000 + i * 10000 + step * 100 + branchStep);
                        
                        // Chance to change branch direction
                        if (caveRng() % 100 < params.turnChance * 150) { // More turns in branches
                            float branchTurnAngle = (angleDist(caveRng) - 3.14159f) * params.maxTurnAngle * 1.2f;
                            branchAngle += branchTurnAngle;
                        }
                        
                        // Move branch in its direction
                        branchX += std::cos(branchAngle) * (branchRadius * 0.6f);
                        branchY += std::sin(branchAngle) * (branchRadius * 0.6f);
                        
                        // Check if branch is still in bounds
                        if (branchX < 0 || branchX >= WORLD_WIDTH || branchY < startY || branchY >= endY) {
                            break;
                        }
                        
                        // Chance to end the branch early
                        if (branchStep > 5 && caveRng() % 100 < 8) {
                            break;
                        }
                    }
                }
                
                // Chance to create a chamber (more common in deeper layers)
                if (step > 20 && caveRng() % 100 < 4 + layer) {
                    // Create a larger chamber at this point
                    float chamberRadius = tunnelRadius * (1.5f + radiusDist(caveRng));
                    carveCircle(x, y, chamberRadius, params.noiseStrength * 1.5f, 
                               seed + layer * 1000000 + i * 100000 + step);
                    
                    // Chamber might create additional short tunnels
                    int numExits = 1 + caveRng() % 3; // 1-3 additional exits
                    
                    for (int exit = 0; exit < numExits; exit++) {
                        float exitAngle = angleDist(caveRng);
                        float exitX = x;
                        float exitY = y;
                        float exitRadius = tunnelRadius * 0.7f;
                        int exitLength = 10 + caveRng() % 20; // Short exit tunnels
                        
                        for (int exitStep = 0; exitStep < exitLength; ++exitStep) {
                            // Carve at current exit position
                            carveCircle(exitX, exitY, exitRadius, params.noiseStrength, 
                                       seed + layer * 10000000 + i * 1000000 + step * 10000 + exit * 100 + exitStep);
                            
                            // Move exit in its direction
                            exitX += std::cos(exitAngle) * (exitRadius * 0.5f);
                            exitY += std::sin(exitAngle) * (exitRadius * 0.5f);
                            
                            // Check if exit is still in bounds
                            if (exitX < 0 || exitX >= WORLD_WIDTH || exitY < startY || exitY >= endY) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Add some special biome-specific features
    std::cout << "  Adding biome-specific cave features..." << std::endl;
    
    // Desert - add occasional sand columns in caves
    int desertFeatures = WORLD_WIDTH / 1000; // Scale with world size
    std::uniform_int_distribution<int> featureHeightDist(5, 15);
    
    for (int i = 0; i < desertFeatures; i++) {
        // Find a desert region
        int startX = caveRng() % (desertEndX);
        int startY = WORLD_HEIGHT / 3 + caveRng() % (WORLD_HEIGHT / 3);
        
        // Check if there's a cave here
        if (get(startX, startY) == MaterialType::Empty) {
            // Create a sand column/pillar
            int height = featureHeightDist(caveRng);
            int width = 1 + caveRng() % 3;
            
            for (int y = startY - height/2; y <= startY + height/2; y++) {
                if (y < 0 || y >= WORLD_HEIGHT) continue;
                
                for (int x = startX - width/2; x <= startX + width/2; x++) {
                    if (x < 0 || x >= WORLD_WIDTH) continue;
                    
                    if (get(x, y) == MaterialType::Empty) {
                        float dist = std::sqrt((x - startX)*(x - startX) + (y - startY)*(y - startY));
                        if (dist <= width) {
                            // Create sand pillar with noise
                            float noise = perlinNoise2D(x * 0.5f, y * 0.5f, seed + i * 1000);
                            if (noise > -0.3f) {
                                set(x, y, MaterialType::Sand);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Mountain - add vertical fissures
    int mountainFeatures = WORLD_WIDTH / 800; // Scale with world size
    
    for (int i = 0; i < mountainFeatures; i++) {
        // Find a mountain region
        int startX = grasslandEndX + caveRng() % (WORLD_WIDTH - grasslandEndX);
        int startY = WORLD_HEIGHT / 4; // Start in upper quarter
        int endY = WORLD_HEIGHT / 2;   // End in middle
        
        // Create a vertical fissure
        int width = 1 + caveRng() % 2;
        float xPos = startX;
        
        for (int y = startY; y < endY; y++) {
            // Add some meandering
            if (caveRng() % 100 < 30) {
                xPos += (caveRng() % 3 - 1);
            }
            
            // Ensure within bounds
            if (xPos < 0) xPos = 0;
            if (xPos >= WORLD_WIDTH) xPos = WORLD_WIDTH - 1;
            
            // Carve the fissure at this y-level
            for (int x = static_cast<int>(xPos) - width; x <= static_cast<int>(xPos) + width; x++) {
                if (x < 0 || x >= WORLD_WIDTH) continue;
                
                MaterialType material = get(x, y);
                if (material != MaterialType::Empty && 
                    material != MaterialType::Water && 
                    material != MaterialType::Lava &&
                    material != MaterialType::Oil &&
                    material != MaterialType::FlammableGas &&
                    material != MaterialType::Bedrock) {
                    
                    // Only carve with some noise for natural look
                    float distFromCenter = std::abs(x - xPos);
                    if (distFromCenter <= width * (0.7f + perlinNoise2D(y * 0.2f, 0, seed + i) * 0.3f)) {
                        set(x, y, MaterialType::Empty);
                    }
                }
            }
            
            // Chance to end the fissure early
            if (caveRng() % 100 < 2) {
                break;
            }
        }
    }
    
    // Create horizontal connection tunnels at different depths
    std::cout << "  Creating horizontal connecting tunnels..." << std::endl;
    
    int numConnectors = WORLD_WIDTH / 600; // Scale with world size
    std::uniform_int_distribution<int> connectorYDist(WORLD_HEIGHT/3, WORLD_HEIGHT*2/3);
    
    for (int i = 0; i < numConnectors; i++) {
        int y = connectorYDist(caveRng);
        int startX = caveRng() % (WORLD_WIDTH - 100);
        int endX = startX + 100 + caveRng() % 400; // 100-500 blocks long
        
        if (endX >= WORLD_WIDTH) endX = WORLD_WIDTH - 1;
        
        // Create a slightly winding horizontal tunnel
        float amplitude = 3.0f + caveRng() % 5;
        float frequency = 0.01f + static_cast<float>(caveRng() % 100) / 10000.0f;
        float radius = 2.0f + radiusDist(caveRng) * 1.5f;
        
        for (int x = startX; x <= endX; x++) {
            // Calculate winding offset
            float offset = std::sin(x * frequency * 3.14159f) * amplitude;
            int currentY = y + static_cast<int>(offset);
            
            // Carve a circular tunnel segment
            carveCircle(x, currentY, radius, 0.3f, seed + i * 10000 + x);
        }
    }
    
    std::cout << "Perlin worm cave generation complete." << std::endl;
}

void World::generateCavesGraph(unsigned int seed) {
    std::cout << "Generating cave system using graph-based approach..." << std::endl;
    
    // Initialize RNG with seed for graph cave generation
    std::mt19937 graphRng(seed + 98765); // Different seed offset for graph generation
    
    // Noise functions for cave properties
    auto widthNoise = [&](float x) {
        return 0.5f + 0.5f * perlinNoise2D(x * 0.01f, 0.0f, seed + 1111);
    };
    
    auto roughnessNoise = [&](float x, float y) {
        return 0.3f + 0.7f * perlinNoise2D(x * 0.05f, y * 0.05f, seed + 2222);
    };
    
    // Define node type distribution based on depth
    auto getNodeTypeForDepth = [&](int y, std::mt19937& rng) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float depthPercent = static_cast<float>(y) / m_height;
        float rand = dist(rng);
        
        if (depthPercent < 0.2f) {
            // Near surface - mostly entrances and tunnels
            if (rand < 0.2f) return CaveNode::Type::ENTRANCE;
            else return CaveNode::Type::TUNNEL;
        } else if (depthPercent < 0.6f) {
            // Mid depths - tunnels and some chambers and junctions
            if (rand < 0.6f) return CaveNode::Type::TUNNEL;
            else if (rand < 0.8f) return CaveNode::Type::CHAMBER;
            else return CaveNode::Type::JUNCTION;
        } else {
            // Deep - more chambers and junctions
            if (rand < 0.4f) return CaveNode::Type::TUNNEL;
            else if (rand < 0.8f) return CaveNode::Type::CHAMBER;
            else return CaveNode::Type::JUNCTION;
        }
    };
    
    // Parameters for graph generation - optimized for realistic caves
    const int NUM_NODES = 40; // Fewer nodes for more significant features
    const float MIN_NODE_DISTANCE = 60.0f; // Larger distance between nodes for less congestion
    const int MAX_CONNECTIONS_ENTRANCE = 1; // Entrances have just one connection (like real cave entrances)
    const int MAX_CONNECTIONS_TUNNEL = 2; // Tunnels typically connect just two points
    const int MAX_CONNECTIONS_CHAMBER = 4; // Chambers can connect to more tunnels
    const int MAX_CONNECTIONS_JUNCTION = 3; // Junctions connect several tunnels
    
    // Create nodes with valid positions
    std::vector<CaveNode> nodes;
    
    // First create entrance nodes - surface connected
    const int NUM_ENTRANCES = 4; // Number of cave entrances
    
    std::cout << "  Creating " << NUM_ENTRANCES << " cave entrances..." << std::endl;
    
    for (int i = 0; i < NUM_ENTRANCES; ++i) {
        // Select a random position on land
        int x = graphRng() % m_width;
        
        // Find ground level at this X position
        int y = 0;
        for (int testY = 0; testY < m_height; ++testY) {
            if (get(x, testY) != MaterialType::Empty) {
                y = testY;
                break;
            }
        }
        
        // Skip if we didn't find ground
        if (y == 0) continue;
        
        // Create entrance node slightly below surface
        CaveNode entrance;
        entrance.type = CaveNode::Type::ENTRANCE;
        entrance.x = x;
        entrance.y = y + 5; // Start a bit below surface
        entrance.radius = 3.0f + static_cast<float>(graphRng() % 2);
        entrance.properties["surfaceY"] = static_cast<float>(y);
        
        nodes.push_back(entrance);
    }
    
    std::cout << "  Creating " << (NUM_NODES - nodes.size()) << " underground nodes..." << std::endl;
    
    // Create underground nodes
    std::uniform_int_distribution<int> xDist(0, m_width - 1);
    std::uniform_real_distribution<float> radiusDist(2.0f, 6.0f);
    
    while (nodes.size() < NUM_NODES) {
        int x = xDist(graphRng);
        
        // Higher chance of nodes appearing in mid to lower depths
        int minDepth = m_height / 5; // Start at least 20% depth
        int maxDepth = m_height * 4 / 5; // End at max 80% depth to avoid bedrock
        
        std::uniform_int_distribution<int> yDist(minDepth, maxDepth);
        int y = yDist(graphRng);
        
        // Check if position is too close to existing nodes
        bool tooClose = false;
        for (const auto& node : nodes) {
            float dx = node.x - x;
            float dy = node.y - y;
            float distSq = dx*dx + dy*dy;
            if (distSq < MIN_NODE_DISTANCE * MIN_NODE_DISTANCE) {
                tooClose = true;
                break;
            }
        }
        
        // Skip if too close to another node
        if (tooClose) continue;
        
        // Create node with type based on depth
        CaveNode node;
        node.type = getNodeTypeForDepth(y, graphRng);
        node.x = x;
        node.y = y;
        
        // Set radius based on node type
        switch (node.type) {
            case CaveNode::Type::TUNNEL:
                node.radius = 2.0f + radiusDist(graphRng) * 0.5f;
                break;
            case CaveNode::Type::CHAMBER:
                node.radius = 4.0f + radiusDist(graphRng);
                break;
            case CaveNode::Type::JUNCTION:
                node.radius = 3.0f + radiusDist(graphRng) * 0.7f;
                break;
            default:
                node.radius = 3.0f;
        }
        
        // Add variation properties
        node.properties["roughness"] = roughnessNoise(x, y);
        
        nodes.push_back(node);
    }
    
    std::cout << "  Created " << nodes.size() << " cave nodes, establishing connections..." << std::endl;
    
    // Connect nodes based on proximity and rules
    for (size_t i = 0; i < nodes.size(); ++i) {
        // Get max connections for this node type
        int maxConnections;
        switch (nodes[i].type) {
            case CaveNode::Type::ENTRANCE:
                maxConnections = MAX_CONNECTIONS_ENTRANCE;
                break;
            case CaveNode::Type::TUNNEL:
                maxConnections = MAX_CONNECTIONS_TUNNEL;
                break;
            case CaveNode::Type::CHAMBER:
                maxConnections = MAX_CONNECTIONS_CHAMBER;
                break;
            case CaveNode::Type::JUNCTION:
                maxConnections = MAX_CONNECTIONS_JUNCTION;
                break;
            default:
                maxConnections = MAX_CONNECTIONS_TUNNEL;
        }
        
        // Skip if already at max connections
        if (nodes[i].connections.size() >= maxConnections) continue;
        
        // Create vector of potential connections with distances
        std::vector<std::pair<size_t, float>> potentialConnections;
        
        for (size_t j = 0; j < nodes.size(); ++j) {
            if (i == j) continue; // Skip self
            
            // Check connection compatibility
            bool compatible = true;
            
            // Entrances can only connect to tunnels
            if (nodes[i].type == CaveNode::Type::ENTRANCE && 
                nodes[j].type != CaveNode::Type::TUNNEL) {
                compatible = false;
            }
            
            // Skip if already connected
            if (std::find(nodes[i].connections.begin(), nodes[i].connections.end(), j) != nodes[i].connections.end()) {
                compatible = false;
            }
            
            // Skip if target already at max connections
            int targetMaxConnections;
            switch (nodes[j].type) {
                case CaveNode::Type::ENTRANCE:
                    targetMaxConnections = MAX_CONNECTIONS_ENTRANCE;
                    break;
                case CaveNode::Type::TUNNEL:
                    targetMaxConnections = MAX_CONNECTIONS_TUNNEL;
                    break;
                case CaveNode::Type::CHAMBER:
                    targetMaxConnections = MAX_CONNECTIONS_CHAMBER;
                    break;
                case CaveNode::Type::JUNCTION:
                    targetMaxConnections = MAX_CONNECTIONS_JUNCTION;
                    break;
                default:
                    targetMaxConnections = MAX_CONNECTIONS_TUNNEL;
            }
            
            if (nodes[j].connections.size() >= targetMaxConnections) {
                compatible = false;
            }
            
            if (compatible) {
                float dx = nodes[i].x - nodes[j].x;
                float dy = nodes[i].y - nodes[j].y;
                float dist = sqrt(dx*dx + dy*dy);
                
                // Favor closer nodes, but not too close to avoid overlapping caves
                if (dist > MIN_NODE_DISTANCE * 0.8f && dist < MIN_NODE_DISTANCE * 3.0f) {
                    potentialConnections.push_back({j, dist});
                }
            }
        }
        
        // Sort by distance
        std::sort(potentialConnections.begin(), potentialConnections.end(),
                 [](const auto& a, const auto& b) { return a.second < b.second; });
        
        // Connect to closest valid nodes up to max connections
        int connectionsToAdd = std::min(maxConnections - static_cast<int>(nodes[i].connections.size()),
                                      static_cast<int>(potentialConnections.size()));
        
        for (int c = 0; c < connectionsToAdd; ++c) {
            size_t targetIdx = potentialConnections[c].first;
            
            // Add bidirectional connection
            nodes[i].connections.push_back(targetIdx);
            nodes[targetIdx].connections.push_back(i);
        }
    }
    
    // Simulate physics to adjust node positions for better spacing
    std::cout << "  Simulating graph physics..." << std::endl;
    simulateGraphPhysics(nodes, 20);
    
    // Generate actual caves from the graph
    std::cout << "  Implementing cave system from graph..." << std::endl;
    implementGraphCaves(nodes);
    
    std::cout << "Graph-based cave generation complete." << std::endl;
}

void World::simulateGraphPhysics(std::vector<CaveNode>& nodes, int iterations) {
    // Simple spring physics to space nodes
    const float REPEL_FORCE = 2.0f;
    const float CONNECTION_FORCE = 5.0f;
    const float MAX_FORCE = 10.0f;
    
    // For each iteration, apply forces
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<std::pair<float, float>> forces(nodes.size(), {0.0f, 0.0f});
        
        // Apply repulsive forces between nodes
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                float dx = nodes[j].x - nodes[i].x;
                float dy = nodes[j].y - nodes[i].y;
                float distSq = dx*dx + dy*dy;
                
                if (distSq < 1.0f) distSq = 1.0f; // Prevent division by zero
                
                // Repulsive force inverse to distance squared
                float force = REPEL_FORCE / distSq;
                
                // Normalize direction
                float dist = sqrt(distSq);
                float forceX = force * dx / dist;
                float forceY = force * dy / dist;
                
                // Apply forces in opposite directions
                forces[i].first -= forceX;
                forces[i].second -= forceY;
                forces[j].first += forceX;
                forces[j].second += forceY;
            }
        }
        
        // Apply attractive forces for connections
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t connIdx : nodes[i].connections) {
                float dx = nodes[connIdx].x - nodes[i].x;
                float dy = nodes[connIdx].y - nodes[i].y;
                float distSq = dx*dx + dy*dy;
                float dist = sqrt(distSq);
                
                // Target distance based on node types
                float targetDist = nodes[i].radius + nodes[connIdx].radius + 15.0f;
                
                // Spring force proportional to difference from target distance
                float force = CONNECTION_FORCE * (dist - targetDist) / 100.0f;
                
                // Normalize direction
                float forceX = force * dx / dist;
                float forceY = force * dy / dist;
                
                // Apply forces
                forces[i].first += forceX;
                forces[i].second += forceY;
            }
        }
        
        // Apply forces with clamping
        for (size_t i = 0; i < nodes.size(); ++i) {
            // Don't move entrance nodes - they need to connect to surface
            if (nodes[i].type == CaveNode::Type::ENTRANCE) continue;
            
            // Clamp forces
            float forceX = forces[i].first;
            float forceY = forces[i].second;
            
            // Limit maximum force
            float forceMag = sqrt(forceX*forceX + forceY*forceY);
            if (forceMag > MAX_FORCE) {
                forceX = forceX * MAX_FORCE / forceMag;
                forceY = forceY * MAX_FORCE / forceMag;
            }
            
            // Apply forces to position
            nodes[i].x += forceX;
            nodes[i].y += forceY;
            
            // Clamp positions to world bounds with padding
            const float BORDER_PADDING = 20.0f;
            float minX = BORDER_PADDING;
            float maxX = static_cast<float>(m_width) - BORDER_PADDING;
            float minY = BORDER_PADDING;
            float maxY = static_cast<float>(m_height) - BORDER_PADDING;
            
            if (nodes[i].x < minX) nodes[i].x = minX;
            if (nodes[i].x > maxX) nodes[i].x = maxX;
            if (nodes[i].y < minY) nodes[i].y = minY;
            if (nodes[i].y > maxY) nodes[i].y = maxY;
        }
    }
}

void World::implementGraphCaves(const std::vector<CaveNode>& nodes) {
    // First carve all chambers
    for (const auto& node : nodes) {
        // For each node, carve its area based on type
        if (node.type == CaveNode::Type::CHAMBER || 
            node.type == CaveNode::Type::JUNCTION || 
            node.type == CaveNode::Type::ENTRANCE) {
            carveGraphChamber(node);
        }
    }
    
    // Then connect with tunnels
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        
        // For each connection, carve a tunnel
        for (size_t connIdx : node.connections) {
            // Only process each connection once (from the lower index node)
            if (i < connIdx) {
                const auto& target = nodes[connIdx];
                
                // Get width variation based on node types
                float widthVariation;
                
                // Tunnels between important nodes get more variation
                if ((node.type == CaveNode::Type::CHAMBER || node.type == CaveNode::Type::JUNCTION) &&
                    (target.type == CaveNode::Type::CHAMBER || target.type == CaveNode::Type::JUNCTION)) {
                    widthVariation = 0.4f;
                } else if (node.type == CaveNode::Type::ENTRANCE || target.type == CaveNode::Type::ENTRANCE) {
                    widthVariation = 0.3f; // Entrance tunnels have less variation
                } else {
                    widthVariation = 0.2f; // Regular tunnels are more uniform
                }
                
                carveGraphTunnel(node, target, widthVariation);
            }
        }
    }
}

void World::carveGraphChamber(const CaveNode& node) {
    // Carve a chamber at this node position with natural cave-like properties
    float radius = node.radius;
    float roughness = node.properties.count("roughness") ? 
                     node.properties.at("roughness") : 0.3f;
    
    // Get ground level for surface protection
    int groundLevel = 0;
    if (node.type == CaveNode::Type::ENTRANCE) {
        // For entrance nodes, use stored surface Y
        groundLevel = static_cast<int>(node.properties.at("surfaceY"));
    } else {
        // For other nodes, calculate ground level
        for(int y = 0; y < m_height; ++y) {
            if(get(static_cast<int>(node.x), y) != MaterialType::Empty) {
                groundLevel = y;
                break;
            }
        }
    }
    
    // Surface protection
    const int SURFACE_BUFFER = 15;
    
    // Scale radius based on node type - make them more variable
    float effectiveRadius = radius;
    std::mt19937 chamberRng(static_cast<unsigned int>(node.x * 1000 + node.y));
    std::uniform_real_distribution<float> sizeDist(0.9f, 1.1f);
    
    if (node.type == CaveNode::Type::CHAMBER) {
        // Chambers are larger but with more shape variation
        effectiveRadius *= 1.7f * sizeDist(chamberRng);
    } else if (node.type == CaveNode::Type::JUNCTION) {
        // Junctions are medium-sized
        effectiveRadius *= 1.4f * sizeDist(chamberRng);
    } else if (node.type == CaveNode::Type::ENTRANCE) {
        // Entrances are smaller than deep chambers, like real cave entrances
        effectiveRadius *= 1.0f * sizeDist(chamberRng);
    }
    
    // Make chamber shape more irregular - real caverns are rarely circular
    // Create multiple focal points to generate more organic cave shapes
    std::uniform_int_distribution<int> numFociDist(1, 3);
    std::uniform_real_distribution<float> offsetDist(0.2f, 0.6f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    
    int numFoci = 1; // Default to 1 focus point
    
    // Bigger chambers and junctions can have multiple focal points
    if (node.type == CaveNode::Type::CHAMBER || node.type == CaveNode::Type::JUNCTION) {
        numFoci = numFociDist(chamberRng);
    }
    
    // Create additional focal points for more complex shapes
    std::vector<std::pair<float, float>> focalPoints;
    std::vector<float> focalRadii;
    
    // Main focal point is the node position
    focalPoints.push_back({node.x, node.y});
    focalRadii.push_back(effectiveRadius);
    
    // Add secondary focal points for more complex shapes
    for (int i = 1; i < numFoci; i++) {
        float angle = angleDist(chamberRng);
        float offset = effectiveRadius * offsetDist(chamberRng);
        
        float fx = node.x + cos(angle) * offset;
        float fy = node.y + sin(angle) * offset;
        
        // Use smaller radii for secondary focal points
        float fr = effectiveRadius * (0.5f + 0.3f * sizeDist(chamberRng));
        
        focalPoints.push_back({fx, fy});
        focalRadii.push_back(fr);
    }
    
    // Define the bounds for carving with some margin
    float maxPossibleRadius = effectiveRadius * 2.0f; // Conservative upper bound
    int minX = static_cast<int>(node.x) - static_cast<int>(maxPossibleRadius) - 5;
    int maxX = static_cast<int>(node.x) + static_cast<int>(maxPossibleRadius) + 5;
    int minY = static_cast<int>(node.y) - static_cast<int>(maxPossibleRadius) - 5;
    int maxY = static_cast<int>(node.y) + static_cast<int>(maxPossibleRadius) + 5;
    
    // Ensure bounds are within world
    minX = std::max(0, minX);
    maxX = std::min(m_width - 1, maxX);
    minY = std::max(0, minY);
    maxY = std::min(m_height - 1, maxY);
    
    // Create more realistic chamber height - caverns are often wider than they are tall
    float heightStretch = 1.2f; // Makes the chamber wider than tall (typical for real caves)
    
    // Vary floor and ceiling roughness differently
    float floorRoughness = roughness * 1.5f; // Floors tend to be rougher than ceilings
    float ceilingRoughness = roughness;
    
    // Carve the chamber with natural variation at the edges
    for(int y = minY; y <= maxY; ++y) {
        for(int x = minX; x <= maxX; ++x) {
            // Get local ground level for this specific x-coordinate for better surface protection
            int localGroundLevel = groundLevel;
            if(abs(x - static_cast<int>(node.x)) > 2) { // Only recalculate if not at center
                for(int ly = 0; ly < m_height; ++ly) {
                    if(get(x, ly) != MaterialType::Empty) {
                        localGroundLevel = ly;
                        break;
                    }
                }
            }
            
            // Surface protection logic - except for entrances
            bool tooCloseToSurface = false;
            if(node.type != CaveNode::Type::ENTRANCE || y > localGroundLevel + 5) {
                if(y < localGroundLevel + SURFACE_BUFFER && y > localGroundLevel - 2) {
                    tooCloseToSurface = true;
                }
            }
            
            if(tooCloseToSurface) {
                continue; // Skip this position
            }
            
            // Check distance to each focal point and use the closest one
            bool shouldCarve = false;
            float minDistSq = FLT_MAX;
            float closestRadius = 0;
            
            for (size_t i = 0; i < focalPoints.size(); i++) {
                const auto& [fx, fy] = focalPoints[i];
                float dx = x - fx;
                float dy = (y - fy) * heightStretch; // Apply vertical stretching
                float distSq = dx*dx + dy*dy;
                
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    closestRadius = focalRadii[i];
                }
            }
            
            // Apply different noise based on whether we're looking at floor or ceiling
            float angleToCenter = atan2(y - node.y, x - node.x);
            float heightFactor = (y > node.y) ? floorRoughness : ceilingRoughness;
            
            // Generate noise based on both position and angle for natural irregularity
            float noiseScale = 0.08f; // Scale for smoother noise
            float baseNoise = perlinNoise2D(x * noiseScale, y * noiseScale, 789);
            float detailNoise = perlinNoise2D(x * noiseScale * 3.0f, y * noiseScale * 3.0f, 456) * 0.3f;
            float angularNoise = perlinNoise2D(angleToCenter * 3.0f, 0.5f, 234) * 0.5f;
            
            // Vary the radius based on a combination of noise sources for realistic caves
            float combinedNoise = (baseNoise * 0.6f + detailNoise * 0.3f + angularNoise * 0.1f);
            float radiusVariation = heightFactor * combinedNoise;
            float actualRadius = closestRadius * (1.0f + radiusVariation);
            
            // Determine if the point should be carved
            if (minDistSq <= actualRadius * actualRadius) {
                MaterialType type = get(x, y);
                
                // Only carve through carvable materials
                if (type != MaterialType::Empty && 
                    type != MaterialType::Water && 
                    type != MaterialType::Lava &&
                    type != MaterialType::Oil &&
                    type != MaterialType::FlammableGas &&
                    type != MaterialType::Bedrock) {
                    
                    // Calculate edge effect - edges are rougher than centers
                    float edgeRatio = minDistSq / (actualRadius * actualRadius);
                    
                    // Implement edge roughness - gives stalactite/stalagmite-like edges
                    // Points near the edge have a chance to remain solid
                    if (edgeRatio < 0.7f || (edgeRatio < 0.95f && combinedNoise > 0.1f)) {
                        set(x, y, MaterialType::Empty);
                    }
                }
            }
        }
    }
    
    // For non-entrance chambers, sometimes add stalactites/stalagmites
    if (node.type == CaveNode::Type::CHAMBER) {
        std::uniform_real_distribution<float> formationDist(0.0f, 1.0f);
        if (formationDist(chamberRng) > 0.5f) {
            // Add cave formations 
            int centerX = static_cast<int>(node.x);
            int centerY = static_cast<int>(node.y);
            int formationRadius = static_cast<int>(effectiveRadius * 0.7f);
            
            // Number of formations to add
            std::uniform_int_distribution<int> numFormationsDist(3, 8);
            int numFormations = numFormationsDist(chamberRng);
            
            for (int i = 0; i < numFormations; i++) {
                // Position within chamber
                std::uniform_real_distribution<float> posDist(-0.9f, 0.9f);
                float fx = centerX + formationRadius * posDist(chamberRng);
                float fy = centerY + formationRadius * posDist(chamberRng) * 0.7f; // Less vertical spread
                
                // Determine if it's a stalactite (ceiling) or stalagmite (floor)
                bool isCeiling = fy < centerY;
                
                // Height of formation
                std::uniform_real_distribution<float> heightDist(0.1f, 0.4f);
                float height = effectiveRadius * heightDist(chamberRng);
                
                // Width of formation
                std::uniform_real_distribution<float> widthDist(0.05f, 0.15f);
                float width = effectiveRadius * widthDist(chamberRng);
                
                // Create the formation - simple cone shape
                for (int oy = 0; oy < height; oy++) {
                    // Ceiling formations go up, floor formations go down
                    int py = isCeiling ? static_cast<int>(fy) + oy : static_cast<int>(fy) - oy;
                    
                    // Width decreases with height (cone shape)
                    float currentWidth = width * (1.0f - oy / height);
                    
                    // Create circular cross-section
                    for (int ox = -static_cast<int>(currentWidth); ox <= static_cast<int>(currentWidth); ox++) {
                        int px = static_cast<int>(fx) + ox;
                        
                        // Only if within world bounds
                        if (px >= 0 && px < m_width && py >= 0 && py < m_height) {
                            float distSq = static_cast<float>(ox * ox);
                            if (distSq <= currentWidth * currentWidth) {
                                // Add some noise to formation
                                float noise = perlinNoise2D(px * 0.5f, py * 0.5f, 456);
                                if (noise > 0.4f && get(px, py) == MaterialType::Empty) {
                                    // Use stone material for formations
                                    set(px, py, MaterialType::Stone);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void World::carveGraphTunnel(const CaveNode& start, const CaveNode& end, float widthVariation) {
    // Get start and end points
    float startX = start.x;
    float startY = start.y;
    float endX = end.x;
    float endY = end.y;
    
    // Calculate tunnel length and direction
    float dx = endX - startX;
    float dy = endY - startY;
    float distance = sqrt(dx*dx + dy*dy);
    
    // Normalized direction
    float dirX = dx / distance;
    float dirY = dy / distance;
    
    // Base tunnel radius - smaller for more natural tunnels
    float baseRadius = (start.radius + end.radius) * 0.3f;
    
    // Create a more natural tunnel path using multiple control points
    // for a winding path like real caves (following weakness in rock)
    std::mt19937 localRng(static_cast<unsigned int>(startX * 1000 + startY + endX + endY * 1000));
    
    // Use multiple control points for complex path
    const int NUM_CONTROL_POINTS = 3 + (distance > 100 ? 2 : 0);
    std::vector<std::pair<float, float>> controlPoints;
    
    // Start and end are fixed
    controlPoints.push_back({startX, startY});
    
    // Add intermediate control points
    float perpX = -dirY; // Perpendicular direction
    float perpY = dirX;
    
    std::uniform_real_distribution<float> curveDist(-0.4f, 0.4f);
    std::uniform_real_distribution<float> noiseDist(0.8f, 1.2f);
    
    // Create control points with natural variation
    for (int i = 1; i < NUM_CONTROL_POINTS; i++) {
        float t = static_cast<float>(i) / NUM_CONTROL_POINTS;
        
        // Base position along straight line
        float baseX = startX + dx * t;
        float baseY = startY + dy * t;
        
        // Add perpendicular offset (winding)
        float curveScale = sin(3.14159f * t); // Maximum curve in middle
        float offset = distance * 0.15f * curveScale * curveDist(localRng);
        
        // Add some vertical bias for caves that tend to go up and down
        float verticalBias = distance * 0.05f * curveDist(localRng);
        
        float ctrlX = baseX + perpX * offset;
        float ctrlY = baseY + perpY * offset + verticalBias;
        
        controlPoints.push_back({ctrlX, ctrlY});
    }
    
    // Add end point
    controlPoints.push_back({endX, endY});
    
    // Number of steps for smooth tunnel depends on distance
    int steps = std::max(20, static_cast<int>(distance / 1.5f));
    
    // Depth influence - deeper tunnels tend to be narrower
    float depthFactor = 1.0f - (std::min(startY, endY) / static_cast<float>(m_height)) * 0.3f;
    
    // Path smoothing using Catmull-Rom spline
    auto interpolate = [](float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        
        // Catmull-Rom coefficients
        float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        float c = -0.5f * p0 + 0.5f * p2;
        float d = p1;
        
        return a * t3 + b * t2 + c * t + d;
    };
    
    // Get segment for t value
    auto getPointOnSpline = [&](float t) -> std::pair<float, float> {
        int numSegments = static_cast<int>(controlPoints.size() - 1);
        float segmentT = t * numSegments;
        int segment = std::min(static_cast<int>(segmentT), numSegments - 1);
        float localT = segmentT - segment;
        
        // Get 4 control points for Catmull-Rom spline
        int i0 = std::max(0, segment - 1);
        int i1 = segment;
        int i2 = std::min(segment + 1, static_cast<int>(controlPoints.size()) - 1);
        int i3 = std::min(segment + 2, static_cast<int>(controlPoints.size()) - 1);
        
        float x = interpolate(
            controlPoints[i0].first, controlPoints[i1].first,
            controlPoints[i2].first, controlPoints[i3].first,
            localT
        );
        
        float y = interpolate(
            controlPoints[i0].second, controlPoints[i1].second,
            controlPoints[i2].second, controlPoints[i3].second,
            localT
        );
        
        return {x, y};
    };
    
    // Carve along the spline with natural variation
    float prevX = startX, prevY = startY;
    
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        auto [x, y] = getPointOnSpline(t);
        
        // Calculate path direction for this segment
        float segDirX = x - prevX;
        float segDirY = y - prevY;
        float segLength = sqrt(segDirX * segDirX + segDirY * segDirY);
        
        // Skip if no movement
        if (segLength < 0.01f) continue;
        
        // Normalize segment direction
        segDirX /= segLength;
        segDirY /= segLength;
        
        // Create wider areas occasionally (small chambers along tunnel)
        float widenFactor = 1.0f;
        float widenNoise = perlinNoise2D(x * 0.02f, y * 0.02f, 789);
        if (widenNoise > 0.7f) {
            widenFactor = 1.5f + widenNoise * 0.5f;
        }
        
        // Width variation along path
        float localWidth = baseRadius * depthFactor * widenFactor;
        
        // Apply noise to tunnel width - use both position noise and angular noise
        float angular = atan2(segDirY, segDirX) * 5.0f;
        float positionNoise = perlinNoise2D(x * 0.2f, y * 0.2f, 456);
        float angularNoise = perlinNoise2D(angular, t * 3.0f, 789);
        float combinedNoise = (positionNoise * 0.6f + angularNoise * 0.4f) * widthVariation;
        
        // Non-uniform radius - somewhat flatter in vertical direction like real caves
        // which follow bedding planes and are wider than tall
        float radius = localWidth * (1.0f + combinedNoise);
        
        // Get perpendicular direction for tunnel cross-section
        float perpX = -segDirY;
        float perpY = segDirX;
        
        // Carve more complex tunnel cross-section (not just a circle)
        // Real caves often follow weakness in rock layers creating irregular shapes
        int radiusInt = static_cast<int>(radius) + 1;
        for (int dy = -radiusInt; dy <= radiusInt; ++dy) {
            for (int dx = -radiusInt; dx <= radiusInt; ++dx) {
                int px = static_cast<int>(x) + dx;
                int py = static_cast<int>(y) + dy;
                
                if (px >= 0 && px < m_width && py >= 0 && py < m_height) {
                    // Calculate elliptical distance - slightly squashed in y-direction like real caves
                    float stretchY = 1.2f; // 1.2x wider than tall
                    float dx2 = static_cast<float>(dx * dx);
                    float dy2 = static_cast<float>(dy * dy) * stretchY;
                    float distSq = dx2 + dy2;
                    
                    // Edge variation for natural tunnel borders using noise
                    float edgeNoise = perlinNoise2D(px * 0.2f, py * 0.2f, 123) * 0.3f;
                    float adjustedRadius = radius * (1.0f + edgeNoise);
                    
                    if (distSq <= adjustedRadius * adjustedRadius) {
                        MaterialType type = get(px, py);
                        
                        // Only carve through carvable materials
                        if (type != MaterialType::Empty && 
                            type != MaterialType::Water && 
                            type != MaterialType::Lava &&
                            type != MaterialType::Oil &&
                            type != MaterialType::FlammableGas &&
                            type != MaterialType::Bedrock) {
                            
                            // The closer to the edge, the higher chance to leave some material
                            // This creates more natural, rough tunnel walls
                            float edgeRatio = distSq / (adjustedRadius * adjustedRadius);
                            
                            if (edgeRatio < 0.7f || edgeNoise > 0.1f) {
                                set(px, py, MaterialType::Empty);
                            }
                        }
                    }
                }
            }
        }
        
        prevX = x;
        prevY = y;
    }
}

void World::updatePixelData() {
    // Update combined pixel data from all chunks
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (!chunk) continue;
            
            // Copy chunk's pixel data to the world's pixel data
            int chunkBaseX = x * Chunk::WIDTH;
            int chunkBaseY = y * Chunk::HEIGHT;
            
            const uint8_t* chunkPixelData = chunk->getPixelData();
            
            for (int cy = 0; cy < Chunk::HEIGHT; ++cy) {
                int worldY = chunkBaseY + cy;
                if (worldY >= m_height) continue;
                
                for (int cx = 0; cx < Chunk::WIDTH; ++cx) {
                    int worldX = chunkBaseX + cx;
                    if (worldX >= m_width) continue;
                    
                    int chunkIdx = (cy * Chunk::WIDTH + cx) * 4;
                    int worldIdx = (worldY * m_width + worldX) * 4;

                    // Just check worldIdx; chunkIdx is guaranteed good by the for loops
                    if (worldIdx + 3 < (int)m_pixelData.size()) {
                        m_pixelData[worldIdx + 0] = chunkPixelData[chunkIdx + 0];
                        m_pixelData[worldIdx + 1] = chunkPixelData[chunkIdx + 1];
                        m_pixelData[worldIdx + 2] = chunkPixelData[chunkIdx + 2];
                        m_pixelData[worldIdx + 3] = chunkPixelData[chunkIdx + 3];
                    }

                }
            }
        }
    }
}

} // namespace PixelPhys
