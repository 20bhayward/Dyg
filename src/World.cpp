#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <SDL_stdinc.h>

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
    
    // Biome noise - use moderate frequency for reasonable variation
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
    
    // Step 1: Generate heightmap using Perlin noise
    std::vector<int> heightMap(WORLD_WIDTH);
    
    // Define biome regions - keep them simple with exact thirds of the world
    const int desertEndX = WORLD_WIDTH / 3;
    const int grasslandEndX = 2 * WORLD_WIDTH / 3;
    
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
    
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        // Determine which biome region this x belongs to
        float currentNoiseScale, heightScale, baseHeight;
        
        if (x < desertEndX - transitionWidth) {
            // Pure desert region
            currentNoiseScale = desertNoiseScale;
            heightScale = desertHeightScale;
            baseHeight = desertBaseHeight;
        } 
        else if (x < desertEndX + transitionWidth) {
            // Desert-grassland transition - interpolate parameters with smooth easing function
            float t = (float)(x - (desertEndX - transitionWidth)) / (2.0f * transitionWidth);
            
            // Apply smooth easing function (cubic) to make transition more natural
            // This is a smooth S-curve transition: t = t^2 * (3 - 2*t)
            t = t * t * (3.0f - 2.0f * t);
            
            currentNoiseScale = desertNoiseScale * (1.0f - t) + grasslandNoiseScale * t;
            heightScale = desertHeightScale * (1.0f - t) + grasslandHeightScale * t;
            baseHeight = desertBaseHeight * (1.0f - t) + grasslandBaseHeight * t;
            
            // Apply erosion to height - this creates a more natural transition with eroded features
            heightScale *= erosionFactor[x];
        }
        else if (x < grasslandEndX - transitionWidth) {
            // Pure grassland region
            currentNoiseScale = grasslandNoiseScale;
            heightScale = grasslandHeightScale;
            baseHeight = grasslandBaseHeight;
        }
        else if (x < grasslandEndX + transitionWidth) {
            // Grassland-mountain transition - interpolate parameters
            float t = (float)(x - (grasslandEndX - transitionWidth)) / (2.0f * transitionWidth);
            
            // Apply smooth easing function (cubic) to make transition more natural
            t = t * t * (3.0f - 2.0f * t);
            
            // Create a smoother slope up to mountains
            // Use t^2 to make it start slow then rise more rapidly
            // Use a more complex transition curve for mountains - starts very gradual, then steepens
            // This creates foothills that slowly build to mountains
            float mountainFactor = t * t * (3.0f - 2.0f * t); // Cubic easing
            
            // Further modulate with noise for uneven, natural mountain ranges
            float mountainNoise = perlinNoise2D(x * 0.01f, 0.0f, seed + 567) * 0.4f + 0.8f;
            mountainFactor = std::pow(mountainFactor, 0.8f) * mountainNoise;
            
            // Apply to all mountain parameters for consistent transition
            currentNoiseScale = grasslandNoiseScale * (1.0f - mountainFactor) + mountainNoiseScale * mountainFactor;
            heightScale = grasslandHeightScale * (1.0f - mountainFactor) + mountainHeightScale * mountainFactor * 1.2f; // Boost mountain height
            baseHeight = grasslandBaseHeight * (1.0f - mountainFactor) + mountainBaseHeight * mountainFactor;
        }
        else {
            // Pure mountain region
            currentNoiseScale = mountainNoiseScale;
            heightScale = mountainHeightScale;
            baseHeight = mountainBaseHeight;
        }
        
        // Generate fractal noise with biome-specific parameters
        // Use more octaves for mountains to get smoother large shapes with finer details
        int biomeOctaves = octaves;
        if (x >= grasslandEndX - transitionWidth) {
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
    
    // Step 2: Generate biome map - based primarily on X position for the three main biomes
    std::vector<BiomeType> biomeMap(WORLD_WIDTH);
    
    // Reuse the same region constants from step 1 for biome assignment
    for (int x = 0; x < WORLD_WIDTH; ++x) {
        float biomeNoise = perlinNoise2D(x * biomeNoiseScale, 42.0f, seed + 123);
        
        // Base biome by X position
        BiomeType baseBiome;
        
        if (x < desertEndX - transitionWidth) {
            // Pure desert region
            baseBiome = BiomeType::DESERT;
        } 
        else if (x < desertEndX + transitionWidth) {
            // Desert-to-grassland transition zone with erosion patterns
            float t = (float)(x - (desertEndX - transitionWidth)) / (2.0f * transitionWidth);
            t = t * t * (3.0f - 2.0f * t); // Smooth cubic easing
            
            // Use varied noise threshold for more natural, eroded transition
            float erosionNoise = perlinNoise2D(x * 0.05f, 0.0f, seed + 456);
            float localErosion = erosionFactor[x] * (0.8f + 0.4f * erosionNoise);
            
            // Adjust threshold based on erosion to create patches of mixed biomes
            // Simple transition with minimal noise to create cleaner borders
            // Use just enough noise to avoid perfectly straight lines
            float blendingNoise = perlinNoise2D(x * 0.01f, 0.0f, seed + 345) * 0.3f;
            
            // Simple threshold with minimal noise factors for cleaner transitions
            float threshold = -0.2f + t + (localErosion - 1.0f) * 0.2f + blendingNoise;
            
            baseBiome = (biomeNoise < threshold) ? BiomeType::DESERT : BiomeType::GRASSLAND;
        }
        else if (x < grasslandEndX - transitionWidth) {
            // Pure grassland region
            baseBiome = BiomeType::GRASSLAND;
        }
        else if (x < grasslandEndX + transitionWidth) {
            // Grassland-to-mountain transition zone
            float t = (float)(x - (grasslandEndX - transitionWidth)) / (2.0f * transitionWidth);
            t = t * t * (3.0f - 2.0f * t); // Smooth cubic easing
            
            // Use a gradual threshold that favors foothills first
            float mountainAmount = t * t; // Squared to increase mountain presence toward the end
            // Use simpler transition for mountains too
            float mtnNoise = perlinNoise2D(x * 0.015f, 0.0f, seed + 890) * 0.25f;
            
            // Simpler threshold with minimal noise sources - creates cleaner mountain border
            float threshold = -0.2f + mountainAmount * 0.7f + mtnNoise;
            baseBiome = (biomeNoise < threshold) ? BiomeType::GRASSLAND : BiomeType::MOUNTAIN;
        }
        else {
            // Pure mountain region
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
        else if (baseBiome == BiomeType::GRASSLAND && biomeNoise > 0.4f) {
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
    
    // Update pixel data
    updatePixelData();
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
