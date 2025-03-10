#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <SDL_stdinc.h>
#include <cfloat> // For FLT_MAX

namespace PixelPhys {

// Chunk implementation

Chunk::Chunk(int posX, int posY) : m_posX(posX), m_posY(posY), m_isDirty(true), 
                                 m_shouldUpdateNextFrame(true), m_inactivityCounter(0) {
    // Initialize chunk with empty cells
    m_grid.resize(WIDTH * HEIGHT, MaterialType::Empty);
    
    // Initialize pixel data for rendering (RGBA for each cell)
    m_pixelData.resize(WIDTH * HEIGHT * 4, 0);
    
    // Initialize freeFalling status for each cell (none are falling initially)
    m_isFreeFalling.resize(WIDTH * HEIGHT, false);
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
    // At the start of each frame, assume this chunk won't need processing next frame
    setShouldUpdateNextFrame(false);
    
    // If this chunk isn't marked as dirty, skip updating it entirely
    if (!m_isDirty) {
        m_inactivityCounter++;
        return;
    } else {
        // Reset inactivity counter since we're updating this frame
        m_inactivityCounter = 0;
    }
    
    // Create a copy of the grid for processing (to avoid updating cells already processed this frame)
    std::vector<MaterialType> oldGrid = m_grid;
    
    // Flag to track if any materials moved during this update
    bool anyMaterialMoved = false;
    
    // First handle powders (falling materials like sand, gravel)
    // Process bottom-to-top, right-to-left to ensure natural falling behavior
    for (int y = HEIGHT - 1; y >= 0; --y) {
        for (int x = WIDTH - 1; x >= 0; --x) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            // Skip empty cells
            if (material == MaterialType::Empty) {
                continue;
            }
            
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Handle powders (like sand)
            if (props.isPowder) {
                // Always assume powders are falling - simulate continuous motion without requiring clicks
                bool isFalling = true;
                // Keep track of whether this material is currently moving
                m_isFreeFalling[idx] = true;
                
                // Try to move powder down
                if (y < HEIGHT - 1) {
                    // Check directly below
                    int belowIdx = (y + 1) * WIDTH + x;
                    MaterialType belowMaterial = (belowIdx < static_cast<int>(m_grid.size())) ? 
                                                m_grid[belowIdx] : MaterialType::Empty;
                    
                    // If below is out of chunk, check the chunk below
                    if (y == HEIGHT - 1 && chunkBelow) {
                        belowMaterial = chunkBelow->get(x, 0);
                        
                        // If below is empty, move down
                        if (belowMaterial == MaterialType::Empty) {
                            // Move directly to empty space below
                            chunkBelow->set(x, 0, material);
                            m_grid[idx] = MaterialType::Empty;
                            anyMaterialMoved = true;
                            
                            // Mark the particle as falling in the other chunk
                            chunkBelow->setFreeFalling(x, true);
                            
                            // Mark the chunk below as needing update next frame
                            chunkBelow->setShouldUpdateNextFrame(true);
                            continue;
                        } 
                        // If we can't move down, check diagonally - always try to move diagonally for better flow
                        else {
                            bool moved = false;
                            
                            // Try diagonal movement - first pick a random side to check
                            bool checkLeftFirst = (m_rng() % 2 == 0);
                            
                            // Check down-left
                            if (checkLeftFirst && x > 0) {
                                MaterialType downLeftMaterial = chunkBelow->get(x - 1, 0);
                                if (downLeftMaterial == MaterialType::Empty) {
                                    chunkBelow->set(x - 1, 0, material);
                                    m_grid[idx] = MaterialType::Empty;
                                    chunkBelow->m_isFreeFalling[x - 1] = true;
                                    anyMaterialMoved = true;
                                    chunkBelow->setShouldUpdateNextFrame(true);
                                    moved = true;
                                }
                            }
                            
                            // Check down-right if didn't move left
                            if (!moved && x < WIDTH - 1) {
                                MaterialType downRightMaterial = chunkBelow->get(x + 1, 0);
                                if (downRightMaterial == MaterialType::Empty) {
                                    chunkBelow->set(x + 1, 0, material);
                                    m_grid[idx] = MaterialType::Empty;
                                    chunkBelow->m_isFreeFalling[x + 1] = true;
                                    anyMaterialMoved = true;
                                    chunkBelow->setShouldUpdateNextFrame(true);
                                    moved = true;
                                }
                            }
                            
                            // If not already moved and we need to check the right first
                            if (!moved && !checkLeftFirst && x > 0) {
                                MaterialType downLeftMaterial = chunkBelow->get(x - 1, 0);
                                if (downLeftMaterial == MaterialType::Empty) {
                                    chunkBelow->set(x - 1, 0, material);
                                    m_grid[idx] = MaterialType::Empty;
                                    chunkBelow->m_isFreeFalling[x - 1] = true;
                                    anyMaterialMoved = true;
                                    chunkBelow->setShouldUpdateNextFrame(true);
                                    moved = true;
                                }
                            }
                            
                            if (moved) {
                                continue;
                            }
                        }
                        
                        // Always mark the chunk below as needing update next frame
                        chunkBelow->setShouldUpdateNextFrame(true);
                    }
                    // Handle within the same chunk
                    else if (belowMaterial == MaterialType::Empty) {
                        // Move powder straight down
                        m_grid[belowIdx] = material;
                        m_grid[idx] = MaterialType::Empty;
                        anyMaterialMoved = true;
                        
                        // Mark the particle as free-falling
                        m_isFreeFalling[belowIdx] = true;
                        continue; // Done moving this particle
                    }
                    // Handle diagonal movement - always attempt for continual flow
                    else {
                        bool movedDiagonally = false;
                        
                        // Randomly choose which side to check first for more natural behavior
                        bool checkLeftFirst = (m_rng() % 2 == 0);
                        
                        // Try moving down-left or down-right
                        if (checkLeftFirst) {
                            // Try down-left if possible
                            if (x > 0) {
                                int downLeftIdx = (y + 1) * WIDTH + (x - 1);
                                MaterialType downLeftMaterial = MaterialType::Empty;
                                
                                // Handle cross-chunk boundaries if needed
                                if (x == 0 && y == HEIGHT - 1 && chunkBelow && chunkLeft) {
                                    downLeftMaterial = chunkBelow->get(WIDTH - 1, 0);
                                } else if (x == 0 && chunkLeft) {
                                    downLeftMaterial = chunkLeft->get(WIDTH - 1, y + 1);
                                } else if (y == HEIGHT - 1 && chunkBelow) {
                                    downLeftMaterial = chunkBelow->get(x - 1, 0);
                                } else if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                    downLeftMaterial = m_grid[downLeftIdx];
                                }
                                
                                if (downLeftMaterial == MaterialType::Empty) {
                                    // Handle cross-chunk boundaries
                                    if (x == 0 && y == HEIGHT - 1 && chunkBelow && chunkLeft) {
                                        chunkBelow->set(WIDTH - 1, 0, material);
                                        chunkBelow->m_isFreeFalling[WIDTH * 0 + WIDTH - 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (x == 0 && chunkLeft) {
                                        chunkLeft->set(WIDTH - 1, y + 1, material);
                                        chunkLeft->m_isFreeFalling[WIDTH * (y + 1) + WIDTH - 1] = true;
                                        chunkLeft->setShouldUpdateNextFrame(true);
                                    } else if (y == HEIGHT - 1 && chunkBelow) {
                                        chunkBelow->set(x - 1, 0, material);
                                        chunkBelow->m_isFreeFalling[x - 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                        m_grid[downLeftIdx] = material;
                                        m_isFreeFalling[downLeftIdx] = true;
                                    }
                                    
                                    m_grid[idx] = MaterialType::Empty;
                                    movedDiagonally = true;
                                    anyMaterialMoved = true;
                                }
                            }
                            
                            // If couldn't move left, try right
                            if (!movedDiagonally && x < WIDTH - 1) {
                                int downRightIdx = (y + 1) * WIDTH + (x + 1);
                                MaterialType downRightMaterial = MaterialType::Empty;
                                
                                // Handle cross-chunk boundaries if needed
                                if (x == WIDTH - 1 && y == HEIGHT - 1 && chunkBelow && chunkRight) {
                                    downRightMaterial = chunkBelow->get(0, 0);
                                } else if (x == WIDTH - 1 && chunkRight) {
                                    downRightMaterial = chunkRight->get(0, y + 1);
                                } else if (y == HEIGHT - 1 && chunkBelow) {
                                    downRightMaterial = chunkBelow->get(x + 1, 0);
                                } else if (downRightIdx < static_cast<int>(m_grid.size())) {
                                    downRightMaterial = m_grid[downRightIdx];
                                }
                                
                                if (downRightMaterial == MaterialType::Empty) {
                                    // Handle cross-chunk boundaries
                                    if (x == WIDTH - 1 && y == HEIGHT - 1 && chunkBelow && chunkRight) {
                                        chunkBelow->set(0, 0, material);
                                        chunkBelow->m_isFreeFalling[0] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (x == WIDTH - 1 && chunkRight) {
                                        chunkRight->set(0, y + 1, material);
                                        chunkRight->m_isFreeFalling[WIDTH * (y + 1)] = true;
                                        chunkRight->setShouldUpdateNextFrame(true);
                                    } else if (y == HEIGHT - 1 && chunkBelow) {
                                        chunkBelow->set(x + 1, 0, material);
                                        chunkBelow->m_isFreeFalling[x + 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (downRightIdx < static_cast<int>(m_grid.size())) {
                                        m_grid[downRightIdx] = material;
                                        m_isFreeFalling[downRightIdx] = true;
                                    }
                                    
                                    m_grid[idx] = MaterialType::Empty;
                                    movedDiagonally = true;
                                    anyMaterialMoved = true;
                                }
                            }
                        } else {
                            // Try right-first instead (same code but order is reversed)
                            // Try down-right first
                            if (x < WIDTH - 1) {
                                int downRightIdx = (y + 1) * WIDTH + (x + 1);
                                MaterialType downRightMaterial = MaterialType::Empty;
                                
                                // Handle cross-chunk boundaries
                                if (x == WIDTH - 1 && y == HEIGHT - 1 && chunkBelow && chunkRight) {
                                    downRightMaterial = chunkBelow->get(0, 0);
                                } else if (x == WIDTH - 1 && chunkRight) {
                                    downRightMaterial = chunkRight->get(0, y + 1);
                                } else if (y == HEIGHT - 1 && chunkBelow) {
                                    downRightMaterial = chunkBelow->get(x + 1, 0);
                                } else if (downRightIdx < static_cast<int>(m_grid.size())) {
                                    downRightMaterial = m_grid[downRightIdx];
                                }
                                
                                if (downRightMaterial == MaterialType::Empty) {
                                    // Handle cross-chunk boundaries
                                    if (x == WIDTH - 1 && y == HEIGHT - 1 && chunkBelow && chunkRight) {
                                        chunkBelow->set(0, 0, material);
                                        chunkBelow->m_isFreeFalling[0] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (x == WIDTH - 1 && chunkRight) {
                                        chunkRight->set(0, y + 1, material);
                                        chunkRight->m_isFreeFalling[WIDTH * (y + 1)] = true;
                                        chunkRight->setShouldUpdateNextFrame(true);
                                    } else if (y == HEIGHT - 1 && chunkBelow) {
                                        chunkBelow->set(x + 1, 0, material);
                                        chunkBelow->m_isFreeFalling[x + 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (downRightIdx < static_cast<int>(m_grid.size())) {
                                        m_grid[downRightIdx] = material;
                                        m_isFreeFalling[downRightIdx] = true;
                                    }
                                    
                                    m_grid[idx] = MaterialType::Empty;
                                    movedDiagonally = true;
                                    anyMaterialMoved = true;
                                }
                            }
                            
                            // If couldn't move right, try left
                            if (!movedDiagonally && x > 0) {
                                int downLeftIdx = (y + 1) * WIDTH + (x - 1);
                                MaterialType downLeftMaterial = MaterialType::Empty;
                                
                                // Handle cross-chunk boundaries
                                if (x == 0 && y == HEIGHT - 1 && chunkBelow && chunkLeft) {
                                    downLeftMaterial = chunkBelow->get(WIDTH - 1, 0);
                                } else if (x == 0 && chunkLeft) {
                                    downLeftMaterial = chunkLeft->get(WIDTH - 1, y + 1);
                                } else if (y == HEIGHT - 1 && chunkBelow) {
                                    downLeftMaterial = chunkBelow->get(x - 1, 0);
                                } else if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                    downLeftMaterial = m_grid[downLeftIdx];
                                }
                                
                                if (downLeftMaterial == MaterialType::Empty) {
                                    // Handle cross-chunk boundaries
                                    if (x == 0 && y == HEIGHT - 1 && chunkBelow && chunkLeft) {
                                        chunkBelow->set(WIDTH - 1, 0, material);
                                        chunkBelow->m_isFreeFalling[WIDTH * 0 + WIDTH - 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (x == 0 && chunkLeft) {
                                        chunkLeft->set(WIDTH - 1, y + 1, material);
                                        chunkLeft->m_isFreeFalling[WIDTH * (y + 1) + WIDTH - 1] = true;
                                        chunkLeft->setShouldUpdateNextFrame(true);
                                    } else if (y == HEIGHT - 1 && chunkBelow) {
                                        chunkBelow->set(x - 1, 0, material);
                                        chunkBelow->m_isFreeFalling[x - 1] = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                    } else if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                        m_grid[downLeftIdx] = material;
                                        m_isFreeFalling[downLeftIdx] = true;
                                    }
                                    
                                    m_grid[idx] = MaterialType::Empty;
                                    movedDiagonally = true;
                                    anyMaterialMoved = true;
                                }
                            }
                        }
                        
                        // If this material moved, continue to the next cell
                        if (movedDiagonally) {
                            continue;
                        }
                    }
                }
                
                // If we reached here, the material didn't move this frame
                // So we update its falling status
                if (anyMaterialMoved) {
                    // Material didn't move, but others have, so consider whether to set it to free falling 
                    // This simulates neighboring particles knocking it loose
                    // The higher the inertial resistance, the less likely it is to be set to freefalling
                    uint8_t resistance = props.inertialResistance; // 0-100 scale
                    if (m_rng() % 100 < (100 - resistance)) {
                        // Set to free falling with a probability inversely proportional to inertial resistance
                        m_isFreeFalling[idx] = true;
                    } else {
                        // Material stays settled
                        m_isFreeFalling[idx] = false;
                    }
                } else {
                    // Nothing moved, material stays where it is
                    m_isFreeFalling[idx] = false;
                }
            }
        }
    }
    
    // Now handle liquids - process bottom-to-top for falling, then left-to-right for spreading
    // First pass: vertical movement (falling)
    for (int y = HEIGHT - 1; y >= 0; --y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            // Skip empty cells or cells that have moved
            if (material == MaterialType::Empty || oldGrid[idx] != m_grid[idx]) {
                continue;
            }
            
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Handle liquid falling
            if (props.isLiquid) {
                if (y < HEIGHT - 1) {
                    // Check directly below
                    int belowIdx = (y + 1) * WIDTH + x;
                    MaterialType belowMaterial = (belowIdx < static_cast<int>(m_grid.size())) ? 
                                                m_grid[belowIdx] : MaterialType::Empty;
                    
                    // Handle cross-chunk boundary for liquids
                    if (y == HEIGHT - 1 && chunkBelow) {
                        belowMaterial = chunkBelow->get(x, 0);
                        
                        // Check if liquid can fall through chunk boundary
                        if (canDisplace(material, belowMaterial)) {
                            chunkBelow->set(x, 0, material);
                            m_grid[idx] = MaterialType::Empty; // Always remove source for volume conservation
                            anyMaterialMoved = true;
                            chunkBelow->setShouldUpdateNextFrame(true);
                            continue;
                        }
                        
                        // Special case for handling liquids at chunk boundary
                        // Try diagonal movement if direct downward movement isn't possible
                        if (belowMaterial != MaterialType::Empty) {
                            bool moved = false;
                            
                            // Randomly choose which side to try first for more natural-looking flow
                            bool tryLeftFirst = (m_rng() % 2 == 0);
                            
                            if (tryLeftFirst) {
                                // Try down-left first
                                if (x > 0) {
                                    MaterialType downLeftMaterial = chunkBelow->get(x - 1, 0);
                                    if (downLeftMaterial == MaterialType::Empty) {
                                        chunkBelow->set(x - 1, 0, material);
                                        m_grid[idx] = MaterialType::Empty;
                                        anyMaterialMoved = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                        moved = true;
                                    }
                                }
                                
                                // Then try down-right
                                if (!moved && x < WIDTH - 1) {
                                    MaterialType downRightMaterial = chunkBelow->get(x + 1, 0);
                                    if (downRightMaterial == MaterialType::Empty) {
                                        chunkBelow->set(x + 1, 0, material);
                                        m_grid[idx] = MaterialType::Empty;
                                        anyMaterialMoved = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                        moved = true;
                                    }
                                }
                            } else {
                                // Try down-right first
                                if (x < WIDTH - 1) {
                                    MaterialType downRightMaterial = chunkBelow->get(x + 1, 0);
                                    if (downRightMaterial == MaterialType::Empty) {
                                        chunkBelow->set(x + 1, 0, material);
                                        m_grid[idx] = MaterialType::Empty;
                                        anyMaterialMoved = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                        moved = true;
                                    }
                                }
                                
                                // Then try down-left
                                if (!moved && x > 0) {
                                    MaterialType downLeftMaterial = chunkBelow->get(x - 1, 0);
                                    if (downLeftMaterial == MaterialType::Empty) {
                                        chunkBelow->set(x - 1, 0, material);
                                        m_grid[idx] = MaterialType::Empty;
                                        anyMaterialMoved = true;
                                        chunkBelow->setShouldUpdateNextFrame(true);
                                        moved = true;
                                    }
                                }
                            }
                            
                            if (moved) {
                                continue;
                            }
                            
                            // Always mark the chunk below for next frame update
                            chunkBelow->setShouldUpdateNextFrame(true);
                        }
                    } else if (belowIdx < static_cast<int>(m_grid.size())) {
                        // Within same chunk - check if liquid can displace what's below
                        if (canDisplace(material, belowMaterial)) {
                            // Move liquid down, potentially displacing another liquid
                            m_grid[belowIdx] = material;
                            m_grid[idx] = MaterialType::Empty; // For volume conservation
                            anyMaterialMoved = true;
                            continue;
                        }
                        
                        // If can't move down, try diagonal
                        if (belowMaterial != MaterialType::Empty) {
                            bool moved = false;
                            
                            // Randomly choose which side to try first
                            bool tryLeftFirst = (m_rng() % 2 == 0);
                            
                            if (tryLeftFirst) {
                                // Try down-left
                                if (x > 0) {
                                    int downLeftIdx = (y + 1) * WIDTH + (x - 1);
                                    if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                        MaterialType downLeftMaterial = m_grid[downLeftIdx];
                                        if (downLeftMaterial == MaterialType::Empty) {
                                            m_grid[downLeftIdx] = material;
                                            m_grid[idx] = MaterialType::Empty;
                                            anyMaterialMoved = true;
                                            moved = true;
                                        }
                                    }
                                }
                                
                                // Try down-right if didn't move left
                                if (!moved && x < WIDTH - 1) {
                                    int downRightIdx = (y + 1) * WIDTH + (x + 1);
                                    if (downRightIdx < static_cast<int>(m_grid.size())) {
                                        MaterialType downRightMaterial = m_grid[downRightIdx];
                                        if (downRightMaterial == MaterialType::Empty) {
                                            m_grid[downRightIdx] = material;
                                            m_grid[idx] = MaterialType::Empty;
                                            anyMaterialMoved = true;
                                            moved = true;
                                        }
                                    }
                                }
                            } else {
                                // Try down-right first
                                if (x < WIDTH - 1) {
                                    int downRightIdx = (y + 1) * WIDTH + (x + 1);
                                    if (downRightIdx < static_cast<int>(m_grid.size())) {
                                        MaterialType downRightMaterial = m_grid[downRightIdx];
                                        if (downRightMaterial == MaterialType::Empty) {
                                            m_grid[downRightIdx] = material;
                                            m_grid[idx] = MaterialType::Empty;
                                            anyMaterialMoved = true;
                                            moved = true;
                                        }
                                    }
                                }
                                
                                // Try down-left if didn't move right
                                if (!moved && x > 0) {
                                    int downLeftIdx = (y + 1) * WIDTH + (x - 1);
                                    if (downLeftIdx < static_cast<int>(m_grid.size())) {
                                        MaterialType downLeftMaterial = m_grid[downLeftIdx];
                                        if (downLeftMaterial == MaterialType::Empty) {
                                            m_grid[downLeftIdx] = material;
                                            m_grid[idx] = MaterialType::Empty;
                                            anyMaterialMoved = true;
                                            moved = true;
                                        }
                                    }
                                }
                            }
                            
                            if (moved) {
                                continue;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Second pass: horizontal spreading for liquids with improved mechanics
    std::vector<MaterialType> spreadGrid = m_grid; // Copy current state for consistent spreading
    
    // Process liquids from bottom to top
    for (int y = HEIGHT - 1; y >= 0; --y) {
        // Use alternating directions for balanced spreading
        for (int iteration = 0; iteration < 2; ++iteration) {
            bool leftToRight = (iteration == 0);
            
            for (int i = 0; i < WIDTH; ++i) {
                int x = leftToRight ? i : (WIDTH - 1 - i);
                int idx = y * WIDTH + x;
                
                // Skip cells that aren't liquids or were already processed
                MaterialType material = spreadGrid[idx];
                if (material == MaterialType::Empty || !(m_grid[idx] == material)) {
                    continue;
                }
                
                const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                if (!props.isLiquid) {
                    continue;
                }
                
                // Check if there's a fluid cell directly below
                bool hasLiquidBelow = false;
                bool hasEmptyBelow = false;
                MaterialType belowMaterial = MaterialType::Empty;
                
                if (y < HEIGHT - 1) {
                    int belowIdx = (y + 1) * WIDTH + x;
                    belowMaterial = (belowIdx < static_cast<int>(m_grid.size())) ? 
                                         m_grid[belowIdx] : MaterialType::Empty;
                    
                    if (y == HEIGHT - 1 && chunkBelow) {
                        belowMaterial = chunkBelow->get(x, 0);
                    }
                    
                    const auto& belowProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(belowMaterial)];
                    hasLiquidBelow = (belowMaterial == material); // Same liquid type below
                    hasEmptyBelow = (belowMaterial == MaterialType::Empty);
                }
                
                // ONLY spread horizontally if:
                // 1. We can't fall downward (blocked by something that's not empty)
                // 2. OR we have same liquid below us (part of a water column)
                if (!hasEmptyBelow || hasLiquidBelow) {
                    // Look for water level discrepancies
                    int liquidColumnHeight = 0;
                    
                    // Calculate height of fluid column at current position
                    for (int checkY = y; checkY >= 0; --checkY) {
                        int checkIdx = checkY * WIDTH + x;
                        if (checkIdx >= 0 && checkIdx < static_cast<int>(m_grid.size())) {
                            if (m_grid[checkIdx] == material) {
                                liquidColumnHeight++;
                            } else {
                                break; // Stop at first non-matching material
                            }
                        }
                    }
                    
                    // Set spread direction based on iteration
                    int spreadDirection = leftToRight ? 1 : -1;
                    
                    // Get material-specific dispersal rate from properties
                    int dispersalRate = props.dispersalRate;
                    
                    // Higher liquid columns create more pressure (further spreading)
                    int liquidPressure = std::min(dispersalRate + (liquidColumnHeight / 2), 8);
                    
                    // Search for empty spaces or lower liquid columns to flow to
                    for (int spread = 1; spread <= liquidPressure; ++spread) {
                        int nx = x + (spreadDirection * spread);
                        MaterialType sideNeighbor = MaterialType::Empty;
                        int sideIdx = -1;
                        bool isCrossingChunk = false;
                        Chunk* targetChunk = nullptr;
                        int targetX = -1;
                        
                        // Handle cross-chunk boundaries for horizontal spread
                        if (nx < 0 && chunkLeft) {
                            // Crossing left chunk boundary
                            int leftChunkX = WIDTH + nx; // Convert to other chunk coordinates
                            sideNeighbor = chunkLeft->get(leftChunkX, y);
                            isCrossingChunk = true;
                            targetChunk = chunkLeft;
                            targetX = leftChunkX;
                        } else if (nx >= WIDTH && chunkRight) {
                            // Crossing right chunk boundary
                            int rightChunkX = nx - WIDTH; // Convert to other chunk coordinates
                            sideNeighbor = chunkRight->get(rightChunkX, y);
                            isCrossingChunk = true;
                            targetChunk = chunkRight;
                            targetX = rightChunkX;
                        } else if (nx >= 0 && nx < WIDTH) {
                            // Within same chunk
                            sideIdx = y * WIDTH + nx;
                            if (sideIdx < static_cast<int>(m_grid.size())) {
                                sideNeighbor = m_grid[sideIdx];
                            }
                        } else {
                            break; // Out of bounds with no chunk
                        }
                        
                        // If we hit same liquid, check column height
                        if (sideNeighbor == material) {
                            // Calculate height of side column
                            int sideColumnHeight = 0;
                            
                            if (isCrossingChunk) {
                                // Count liquid cells in the neighboring chunk column
                                for (int checkY = y; checkY >= 0; --checkY) {
                                    MaterialType checkMaterial = targetChunk->get(targetX, checkY);
                                    if (checkMaterial == material) {
                                        sideColumnHeight++;
                                    } else {
                                        break;
                                    }
                                }
                            } else {
                                // Count liquid cells in current chunk
                                for (int checkY = y; checkY >= 0; --checkY) {
                                    int checkIdx = checkY * WIDTH + nx;
                                    if (checkIdx >= 0 && checkIdx < static_cast<int>(m_grid.size())) {
                                        if (m_grid[checkIdx] == material) {
                                            sideColumnHeight++;
                                        } else {
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            // If this column is higher, continue searching outward
                            if (liquidColumnHeight <= sideColumnHeight) {
                                continue;
                            }
                            
                            // If current column is significantly higher, try to level it out
                            if (liquidColumnHeight > sideColumnHeight + 1) {
                                // Found a lower column - try to equalize heights
                                if (isCrossingChunk) {
                                    // Move material to neighbor chunk column
                                    targetChunk->set(targetX, y - sideColumnHeight, material);
                                    m_grid[idx] = MaterialType::Empty;
                                    targetChunk->setShouldUpdateNextFrame(true);
                                    anyMaterialMoved = true;
                                } else {
                                    // Move material to lower column within same chunk
                                    m_grid[y * WIDTH + nx] = material;
                                    m_grid[idx] = MaterialType::Empty;
                                    anyMaterialMoved = true;
                                }
                                break;
                            }
                        }
                        // If found empty space
                        else if (sideNeighbor == MaterialType::Empty) {
                            // Check for cell below this empty space for support
                            bool hasSupport = false;
                            
                            if (isCrossingChunk) {
                                if (y < HEIGHT - 1) {
                                    MaterialType belowNeighbor = targetChunk->get(targetX, y + 1);
                                    hasSupport = (belowNeighbor != MaterialType::Empty);
                                }
                            } else {
                                if (y < HEIGHT - 1 && nx >= 0 && nx < WIDTH) {
                                    int belowNeighborIdx = (y + 1) * WIDTH + nx;
                                    if (belowNeighborIdx < static_cast<int>(m_grid.size())) {
                                        MaterialType belowNeighbor = m_grid[belowNeighborIdx];
                                        hasSupport = (belowNeighbor != MaterialType::Empty);
                                    }
                                }
                            }
                            
                            // Check each cell along the path to ensure we don't skip over solid blocks
                            bool pathBlocked = false;
                            for (int checkSpread = 1; checkSpread < spread; checkSpread++) {
                                int checkX = x + (spreadDirection * checkSpread);
                                // Handle cross-chunk checks
                                MaterialType checkMaterial = MaterialType::Empty;
                                
                                if (checkX < 0 && chunkLeft) {
                                    int leftCheckX = WIDTH + checkX;
                                    checkMaterial = chunkLeft->get(leftCheckX, y);
                                } else if (checkX >= WIDTH && chunkRight) {
                                    int rightCheckX = checkX - WIDTH;
                                    checkMaterial = chunkRight->get(rightCheckX, y);
                                } else if (checkX >= 0 && checkX < WIDTH) {
                                    checkMaterial = m_grid[y * WIDTH + checkX];
                                }
                                
                                // If a solid block is in the way, path is blocked
                                if (checkMaterial != MaterialType::Empty && checkMaterial != material) {
                                    const auto& checkProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(checkMaterial)];
                                    if (checkProps.isSolid) {
                                        pathBlocked = true;
                                        break;
                                    }
                                }
                            }
                            
                            // Only flow horizontally if the path is clear and the destination has support
                            // or if it's very close to the source
                            if (!pathBlocked && (hasSupport || spread <= 1)) {
                                if (isCrossingChunk) {
                                    // Move material to neighboring chunk
                                    targetChunk->set(targetX, y, material);
                                    m_grid[idx] = MaterialType::Empty;
                                    targetChunk->setShouldUpdateNextFrame(true);
                                    anyMaterialMoved = true;
                                } else {
                                    // Move material within same chunk
                                    m_grid[sideIdx] = material;
                                    m_grid[idx] = MaterialType::Empty;
                                    anyMaterialMoved = true;
                                }
                                break;
                            }
                        } 
                        // If hit different material type, stop searching
                        else if (sideNeighbor != material) {
                            break;
                        }
                    }
                }
            }
        }
    }
    

    
    // Handle gas rise (for fire, flammable gas, etc.)
    for (int y = 0; y < HEIGHT; ++y) {  // Bottom-up for gases (they rise)
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            if (material != MaterialType::Empty && material != m_grid[idx]) {
                continue; // Skip if already moved
            }
            
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            if (props.isGas) {
                // Try to rise upward
                if (y > 0) {
                    int aboveIdx = (y - 1) * WIDTH + x;
                    
                    if (aboveIdx >= 0 && aboveIdx < static_cast<int>(m_grid.size())) {
                        MaterialType aboveMaterial = m_grid[aboveIdx];
                        
                        // Gases can rise through empty space
                        if (aboveMaterial == MaterialType::Empty) {
                            m_grid[aboveIdx] = material;
                            m_grid[idx] = MaterialType::Empty;
                            anyMaterialMoved = true;
                        }
                        // Gases can rise through liquids (creating bubbles)
                        else {
                            const auto& aboveProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(aboveMaterial)];
                            if (aboveProps.isLiquid) {
                                // Swap positions - gas rises through liquid
                                m_grid[aboveIdx] = material;
                                m_grid[idx] = aboveMaterial;
                                anyMaterialMoved = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Handle material interactions (like fire spreading, water effects, etc.)
    handleMaterialInteractions(oldGrid, anyMaterialMoved);
    
    // Mark the chunk for update next frame if materials moved
    // This allows materials to continue being simulated even without player interaction
    if (anyMaterialMoved) {
        setShouldUpdateNextFrame(true);
        
        // Mark neighboring chunks as potentially needing updates too
        if (chunkBelow) chunkBelow->setShouldUpdateNextFrame(true);
        if (chunkLeft) chunkLeft->setShouldUpdateNextFrame(true);
        if (chunkRight) chunkRight->setShouldUpdateNextFrame(true);
    }
    
    // Update the pixel data to reflect the changes
    updatePixelData();
}

bool Chunk::canDisplace(MaterialType above, MaterialType below) const {
    // If below is empty, anything can fall into it
    if (below == MaterialType::Empty) {
        return true;
    }
    
    // Get properties of both materials
    const auto& aboveProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(above)];
    const auto& belowProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(below)];
    
    // Powders can displace liquids if they're dense enough
    if (aboveProps.isPowder && belowProps.isLiquid) {
        // Dense powders like sand/gravel sink in all liquids
        return true; // Sand/Gravel can fall through water/oil/etc.
    }
    
    // Liquids can't displace solids
    if (aboveProps.isLiquid && belowProps.isSolid) {
        return false;
    }
    
    // Liquids can flow through each other based on density
    if (aboveProps.isLiquid && belowProps.isLiquid) {
        // Density-based displacement between liquids
        if (above == MaterialType::Lava && below == MaterialType::Water) {
            // Lava is denser than water, so it sinks
            return true;
        }
        else if (above == MaterialType::Water && below == MaterialType::Oil) {
            // Water is denser than oil, so it sinks below oil
            return true;
        }
        else if (above == MaterialType::Lava && below == MaterialType::Oil) {
            // Lava is denser than oil, so it sinks
            return true;
        }
        // Same liquid types don't displace each other
        else if (above == below) {
            return false;
        }
    }
    
    // Gases can rise through liquids
    if (aboveProps.isGas && belowProps.isLiquid) {
        return false; // Gases don't displace liquids downward
    }
    if (aboveProps.isLiquid && belowProps.isGas) {
        return true; // Liquids fall through gases
    }
    
    // Default: no displacement
    return false;
}

void Chunk::handleMaterialInteractions(const std::vector<MaterialType>& oldGrid, bool& anyMaterialMoved) {
    // Process all cells in the grid for material interactions
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType current = m_grid[idx];
            
            // Skip empty cells
            if (current == MaterialType::Empty) {
                continue;
            }
            
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(current)];
            
            // Fire interactions with flammable materials
            if (current == MaterialType::Fire) {
                // Check surrounding cells for flammable materials
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        // Skip the fire itself
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Skip out of bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
                        
                        int neighborIdx = ny * WIDTH + nx;
                        if (neighborIdx < 0 || neighborIdx >= static_cast<int>(m_grid.size())) continue;
                        
                        MaterialType neighbor = m_grid[neighborIdx];
                        const auto& neighborProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(neighbor)];
                        
                        // If neighbor is flammable, chance to ignite it
                        if (neighborProps.isFlammable && (m_rng() % 20) == 0) {
                            m_grid[neighborIdx] = MaterialType::Fire;
                            anyMaterialMoved = true;
                        }
                    }
                }
                
                // Fire has chance to burn out
                if ((m_rng() % 100) < 2) {
                    m_grid[idx] = MaterialType::Empty;
                    anyMaterialMoved = true;
                }
            }
            
            // Water and lava interactions
            if (current == MaterialType::Water || current == MaterialType::Lava) {
                // Check surrounding cells for water-lava interactions
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        // Skip the current cell
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Skip out of bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
                        
                        int neighborIdx = ny * WIDTH + nx;
                        if (neighborIdx < 0 || neighborIdx >= static_cast<int>(m_grid.size())) continue;
                        
                        MaterialType neighbor = m_grid[neighborIdx];
                        
                        // Water + Lava = Stone (water cools lava)
                        if ((current == MaterialType::Water && neighbor == MaterialType::Lava) ||
                            (current == MaterialType::Lava && neighbor == MaterialType::Water)) {
                            // 50% chance for obsidian (Stone) where the water was
                            if (current == MaterialType::Water) {
                                m_grid[idx] = MaterialType::Stone;
                            } else {
                                m_grid[neighborIdx] = MaterialType::Stone;
                            }
                            anyMaterialMoved = true;
                        }
                    }
                }
            }
            
            // Oil can be ignited by nearby fire
            if (current == MaterialType::Oil) {
                // Check surrounding cells for fire
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        // Skip the oil itself
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Skip out of bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
                        
                        int neighborIdx = ny * WIDTH + nx;
                        if (neighborIdx < 0 || neighborIdx >= static_cast<int>(m_grid.size())) continue;
                        
                        MaterialType neighbor = m_grid[neighborIdx];
                        
                        // If neighbor is fire, oil catches fire
                        if (neighbor == MaterialType::Fire && (m_rng() % 10) == 0) {
                            m_grid[idx] = MaterialType::Fire;
                            anyMaterialMoved = true;
                            break;
                        }
                    }
                }
            }
            
            // Flammable gas behavior
            if (current == MaterialType::FlammableGas) {
                // Check surrounding cells for fire
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        // Skip the gas itself
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Skip out of bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
                        
                        int neighborIdx = ny * WIDTH + nx;
                        if (neighborIdx < 0 || neighborIdx >= static_cast<int>(m_grid.size())) continue;
                        
                        MaterialType neighbor = m_grid[neighborIdx];
                        
                        // If neighbor is fire, gas explodes
                        if (neighbor == MaterialType::Fire) {
                            // Set gas and surrounding area to fire
                            m_grid[idx] = MaterialType::Fire;
                            
                            // Create mini explosion
                            for (int ey = -2; ey <= 2; ++ey) {
                                for (int ex = -2; ex <= 2; ++ex) {
                                    int explosionX = x + ex;
                                    int explosionY = y + ey;
                                    
                                    if (explosionX < 0 || explosionX >= WIDTH || explosionY < 0 || explosionY >= HEIGHT) continue;
                                    
                                    int explosionIdx = explosionY * WIDTH + explosionX;
                                    if (explosionIdx < 0 || explosionIdx >= static_cast<int>(m_grid.size())) continue;
                                    
                                    // Don't affect solid blocks
                                    MaterialType targetMaterial = m_grid[explosionIdx];
                                    const auto& targetProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(targetMaterial)];
                                    
                                    if (!targetProps.isSolid || (m_rng() % 3) == 0) {
                                        m_grid[explosionIdx] = MaterialType::Fire;
                                    }
                                }
                            }
                            
                            anyMaterialMoved = true;
                            break;
                        }
                    }
                }
            }
        }
    }
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

bool Chunk::serialize(std::ostream& out) const {
    // Stub implementation - will be expanded in later phases
    
    // Write chunk position
    out.write(reinterpret_cast<const char*>(&m_posX), sizeof(m_posX));
    out.write(reinterpret_cast<const char*>(&m_posY), sizeof(m_posY));
    
    // Write chunk grid
    uint32_t gridSize = static_cast<uint32_t>(m_grid.size());
    out.write(reinterpret_cast<const char*>(&gridSize), sizeof(gridSize));
    out.write(reinterpret_cast<const char*>(m_grid.data()), gridSize * sizeof(MaterialType));
    
    // Reset modified flag after serialization
    const_cast<Chunk*>(this)->setModified(false);
    
    return out.good();
}

bool Chunk::deserialize(std::istream& in) {
    // Stub implementation - will be expanded in later phases
    
    // Read chunk position
    in.read(reinterpret_cast<char*>(&m_posX), sizeof(m_posX));
    in.read(reinterpret_cast<char*>(&m_posY), sizeof(m_posY));
    
    // Read chunk grid
    uint32_t gridSize;
    in.read(reinterpret_cast<char*>(&gridSize), sizeof(gridSize));
    m_grid.resize(gridSize);
    in.read(reinterpret_cast<char*>(m_grid.data()), gridSize * sizeof(MaterialType));
    
    // Update pixel data for rendering
    updatePixelData();
    
    // Mark as clean
    setModified(false);
    // But mark as dirty for physics update
    m_isDirty = true;
    m_shouldUpdateNextFrame = true;
    
    return in.good();
}

// World implementation

World::World(int width, int height) 
    : m_width(width), m_height(height), m_chunkManager(Chunk::WIDTH) {
    std::cout << "Creating world with chunk size: " << Chunk::WIDTH << "x" << Chunk::HEIGHT << std::endl;
    // Compute chunk dimensions
    m_chunksX = (width + Chunk::WIDTH - 1) / Chunk::WIDTH;
    m_chunksY = (height + Chunk::HEIGHT - 1) / Chunk::HEIGHT;
    
    // Create all chunks (legacy system)
    m_chunks.resize(m_chunksX * m_chunksY);
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            m_chunks[y * m_chunksX + x] = std::make_unique<Chunk>(x * Chunk::WIDTH, y * Chunk::HEIGHT);
            
            // Initially initialize chunk manager too
            // This will be replaced with dynamic loading/unloading later
            m_chunkManager.getChunk(x, y, true);
        }
    }
    
    // Initialize pixel data for rendering
    m_pixelData.resize(width * height * 4, 0);
    
    // Initialize RNG
    std::random_device rd;
    m_rng = std::mt19937(rd());
    
    std::cout << "Created world with dimensions: " << width << "x" << height << std::endl;
    std::cout << "Chunk grid size: " << m_chunksX << "x" << m_chunksY << std::endl;
    
    // TEST: Add some blocks to each chunk for debugging the streaming system
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            // Get chunk
            Chunk* chunk = getChunkAt(x, y);
            if (!chunk) continue;
            
            // Add a distinctive pattern to each chunk
            // Fill the entire chunk with different materials based on position
            for (int localX = 0; localX < Chunk::WIDTH; localX++) {
                for (int localY = 0; localY < Chunk::HEIGHT; localY++) {
                    // Add a solid pattern with some materials
                    MaterialType material;
                    if ((x + y) % 3 == 0) material = MaterialType::Sand;
                    else if ((x + y) % 3 == 1) material = MaterialType::Stone;
                    else material = MaterialType::Gravel;
                    
                    // Set the material and mark as modified to force pixel update
                    chunk->set(localX, localY, material);
                }
            }
            
            // Force pixel data update
            chunk->updatePixelData();
            
            // Draw a border around each chunk
            for (int i = 0; i < Chunk::WIDTH; i++) {
                chunk->set(i, 0, MaterialType::Bedrock);
                chunk->set(i, Chunk::HEIGHT-1, MaterialType::Bedrock);
            }
            for (int i = 0; i < Chunk::HEIGHT; i++) {
                chunk->set(0, i, MaterialType::Bedrock);
                chunk->set(Chunk::WIDTH-1, i, MaterialType::Bedrock);
            }
        }
    }
}

MaterialType World::get(int x, int y) const {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return MaterialType::Empty;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    // Try to get from ChunkManager first (for streaming system)
    ChunkCoord coord{chunkX, chunkY};
    
    // Need a non-const chunk to try loading from ChunkManager
    // This const_cast is necessary because get() is const but needs to potentially load chunks
    ChunkManager& manager = const_cast<ChunkManager&>(m_chunkManager);
    Chunk* chunk = manager.getChunk(chunkX, chunkY, false); // Don't try to load, just check if available
    
    if (chunk) {
        return chunk->get(localX, localY);
    }
    
    // Fall back to the regular chunks
    const Chunk* oldChunk = getChunkAt(chunkX, chunkY);
    if (!oldChunk) {
        return MaterialType::Empty;
    }
    
    // Get material at local coordinates
    return oldChunk->get(localX, localY);
}

void World::set(int x, int y, MaterialType material) {
    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    // Get the chunk
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    // Try to get from ChunkManager first (for streaming system)
    Chunk* chunk = m_chunkManager.getChunk(chunkX, chunkY, true); // Load if needed
    if (chunk) {
        // Set material at local coordinates in the ChunkManager's chunk
        chunk->set(localX, localY, material);
    }
    
    // Also set in the legacy system for now (for compatibility)
    Chunk* oldChunk = getChunkAt(chunkX, chunkY);
    if (oldChunk) {
        // Set material at local coordinates
        oldChunk->set(localX, localY, material);
    }
    
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

bool Chunk::isNotIsolatedLiquid(const std::vector<MaterialType>& grid, int x, int y) {
    // Skip bounds check for performance in internal use
    int idx = y * WIDTH + x;
    if (idx < 0 || idx >= static_cast<int>(grid.size())) {
        return false;
    }
    
    MaterialType material = grid[idx];
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    
    // If not a liquid, it's not an isolated liquid
    if (!props.isLiquid) {
        return false;
    }
    
    // Check surrounding cells for other liquids or empty spaces
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            // Skip the center cell
            if (dx == 0 && dy == 0) {
                continue;
            }
            
            int nx = x + dx;
            int ny = y + dy;
            
            // Skip out of bounds
            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) {
                continue;
            }
            
            int neighborIdx = ny * WIDTH + nx;
            if (neighborIdx < 0 || neighborIdx >= static_cast<int>(grid.size())) {
                continue;
            }
            
            MaterialType neighbor = grid[neighborIdx];
            
            // If neighbor is empty, the liquid can flow there
            if (neighbor == MaterialType::Empty) {
                return true;
            }
            
            // If neighbor is another liquid, they can interact
            const auto& neighborProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(neighbor)];
            if (neighborProps.isLiquid) {
                return true;
            }
        }
    }
    
    // If no empty spaces or other liquids found, it's isolated
    return false;
}

void World::update() {
    // Handle special edge-case cleanup for powder materials that may have gotten stuck
    // This periodic cleanup ensures nothing gets left behind
    static int cleanupCounter = 0;
    cleanupCounter++;
    
    // Every 30 frames, do a full check for stuck powders at chunk boundaries
    if (cleanupCounter >= 30) {
        cleanupCounter = 0;
        // Force chunks that have been inactive for a long time to update at least once
        for (auto& chunk : m_chunks) {
            if (chunk && chunk->getInactivityCounter() > 50) {
                std::cout << "Setting chunk to dirty for being inactive for a while" << std::endl;
                chunk->setDirty(true);
            }
        }
    }
    
    // At the start of the frame, transfer shouldUpdateNextFrame flags to dirty flags
    for (auto& chunk : m_chunks) {
        if (chunk) {
            if (chunk->shouldUpdateNextFrame()) {
                chunk->setDirty(true); // Set chunk to update this frame
                chunk->setShouldUpdateNextFrame(false); // Reset flag for next frame
            }
        }
    }
    
    // Update all chunks in the streaming system with proper physics
    const auto& activeChunks = m_chunkManager.getActiveChunks();
    
    // First, mark all active chunks as dirty
    for (const auto& coord : activeChunks) {
        Chunk* chunk = m_chunkManager.getChunk(coord.x, coord.y, false);
        if (chunk) {
            chunk->setDirty(true); // Force update for active chunks
        }
    }
    
    // Then properly update each chunk with physics from its neighbors
    for (const auto& coord : activeChunks) {
        Chunk* chunk = m_chunkManager.getChunk(coord.x, coord.y, false);
        if (!chunk) continue;
        
        // Get neighboring chunks from ChunkManager
        ChunkCoord belowCoord = {coord.x, coord.y + 1};
        ChunkCoord leftCoord = {coord.x - 1, coord.y};
        ChunkCoord rightCoord = {coord.x + 1, coord.y};
        
        Chunk* chunkBelow = m_chunkManager.getChunk(belowCoord.x, belowCoord.y, false);
        Chunk* chunkLeft = m_chunkManager.getChunk(leftCoord.x, leftCoord.y, false);
        Chunk* chunkRight = m_chunkManager.getChunk(rightCoord.x, rightCoord.y, false);
        
        // Update this chunk with its neighbors from the ChunkManager
        chunk->update(chunkBelow, chunkLeft, chunkRight);
    }
    
    // Special check for powders and liquids at chunk boundaries to prevent stuck particles
    for (int i = 0; i < (int)m_chunks.size(); ++i) {
        auto& chunk = m_chunks[i];
        if (chunk && chunk->isDirty()) {
            int y = i / m_chunksX;
            int x = i % m_chunksX;
            
            // Only check the bottom row if there's a chunk below
            if (y < m_chunksY - 1) {
                Chunk* chunkBelow = getChunkAt(x, y + 1);
                if (chunkBelow) {
                    // Check every cell in the bottom row of this chunk
                    for (int localX = 0; localX < Chunk::WIDTH; localX++) {
                        MaterialType material = chunk->get(localX, Chunk::HEIGHT - 1);
                        if (material != MaterialType::Empty) {
                            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                            
                            // If it's a powder or liquid, always check for falling
                            if (props.isPowder || props.isLiquid) {
                                // Special case: For materials at chunk boundaries, actively try to move them
                                // across the boundary rather than just marking for a later update
                                MaterialType belowMaterial = chunkBelow->get(localX, 0);
                                
                                // Check if material can displace what's below
                                bool canMove = false;
                                
                                if (belowMaterial == MaterialType::Empty) {
                                    canMove = true;
                                } else if (props.isLiquid) {
                                    // For liquids, check if they can displace the material below
                                    const auto& belowProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(belowMaterial)];
                                    
                                    // Handle density-based displacements between different liquids
                                    if (belowProps.isLiquid) {
                                        if ((material == MaterialType::Lava && belowMaterial == MaterialType::Water) ||
                                            (material == MaterialType::Water && belowMaterial == MaterialType::Oil) ||
                                            (material == MaterialType::Lava && belowMaterial == MaterialType::Oil)) {
                                            canMove = true;
                                        }
                                    }
                                }
                                
                                if (canMove) {
                                    // Directly move material down, displacing what's below if needed
                                    MaterialType originalBelowMaterial = belowMaterial;
                                    chunkBelow->set(localX, 0, material);
                                    chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                    
                                    // Set the element as free-falling in the chunk below
                                    chunkBelow->setFreeFalling(localX, true);
                                } else {
                                    // Try to move diagonally if directly below is blocked
                                    bool moved = false;
                                    
                                    // Randomly choose which side to try first
                                    bool tryLeftFirst = (rand() % 2 == 0);
                                    
                                    if (tryLeftFirst) {
                                        // Try down-left
                                        if (localX > 0) {
                                            MaterialType downLeftMaterial = chunkBelow->get(localX - 1, 0);
                                            if (downLeftMaterial == MaterialType::Empty) {
                                                chunkBelow->set(localX - 1, 0, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                chunkBelow->setFreeFalling(localX-1,true);
                                                moved = true;
                                            }
                                        }
                                        
                                        // Try down-right if didn't move down-left
                                        if (!moved && localX < Chunk::WIDTH - 1) {
                                            MaterialType downRightMaterial = chunkBelow->get(localX + 1, 0);
                                            if (downRightMaterial == MaterialType::Empty) {
                                                chunkBelow->set(localX + 1, 0, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                chunkBelow->setFreeFalling(localX+1,true);
                                                moved = true;
                                            }
                                        }
                                    } else {
                                        // Try down-right first
                                        if (localX < Chunk::WIDTH - 1) {
                                            MaterialType downRightMaterial = chunkBelow->get(localX + 1, 0);
                                            if (downRightMaterial == MaterialType::Empty) {
                                                chunkBelow->set(localX + 1, 0, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                chunkBelow->setFreeFalling(localX+1,true);
                                                moved = true;
                                            }
                                        }
                                        
                                        // Try down-left if didn't move down-right
                                        if (!moved && localX > 0) {
                                            MaterialType downLeftMaterial = chunkBelow->get(localX - 1, 0);
                                            if (downLeftMaterial == MaterialType::Empty) {
                                                chunkBelow->set(localX - 1, 0, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                chunkBelow->setFreeFalling(localX-1,true);
                                                moved = true;
                                            }
                                        }
                                    }
                                    
                                    // For liquids at chunk boundaries, also try horizontal spread
                                    // if vertical movement isn't possible
                                    if (!moved && props.isLiquid) {
                                        // Try to move left
                                        if (localX > 0) {
                                            MaterialType leftMaterial = chunk->get(localX - 1, Chunk::HEIGHT - 1);
                                            if (leftMaterial == MaterialType::Empty) {
                                                chunk->set(localX - 1, Chunk::HEIGHT - 1, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                moved = true;
                                            }
                                        }
                                        
                                        // Try to move right
                                        if (!moved && localX < Chunk::WIDTH - 1) {
                                            MaterialType rightMaterial = chunk->get(localX + 1, Chunk::HEIGHT - 1);
                                            if (rightMaterial == MaterialType::Empty) {
                                                chunk->set(localX + 1, Chunk::HEIGHT - 1, material);
                                                chunk->set(localX, Chunk::HEIGHT - 1, MaterialType::Empty);
                                                moved = true;
                                            }
                                        }
                                    }
                                }
                                
                                // Always mark both chunks for update next frame
                                chunk->setShouldUpdateNextFrame(true);
                                chunkBelow->setShouldUpdateNextFrame(true);
                            }
                        }
                    }
                }
            }
            
            // Check for liquids at horizontal chunk boundaries that might get stuck
            // Left edge
            if (x > 0) {
                Chunk* chunkLeft = getChunkAt(x - 1, y);
                if (chunkLeft) {
                    // Check leftmost column of this chunk and rightmost of left chunk
                    for (int localY = 0; localY < Chunk::HEIGHT; localY++) {
                        MaterialType material = chunk->get(0, localY);
                        if (material != MaterialType::Empty) {
                            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                            
                            // Only process liquids for horizontal edge cases
                            if (props.isLiquid) {
                                MaterialType leftMaterial = chunkLeft->get(Chunk::WIDTH - 1, localY);
                                if (leftMaterial == MaterialType::Empty) {
                                    // Liquid can flow left across chunk boundary
                                    // Always move all material to maintain volume
                                    chunkLeft->set(Chunk::WIDTH - 1, localY, material);
                                    chunk->set(0, localY, MaterialType::Empty);
                                    
                                    // Mark chunks for next frame update
                                    chunk->setShouldUpdateNextFrame(true);
                                    chunkLeft->setShouldUpdateNextFrame(true);
                                }
                            }
                        }
                    }
                }
            }
            
            // Right edge
            if (x < m_chunksX - 1) {
                Chunk* chunkRight = getChunkAt(x + 1, y);
                if (chunkRight) {
                    // Check rightmost column of this chunk and leftmost of right chunk
                    for (int localY = 0; localY < Chunk::HEIGHT; localY++) {
                        MaterialType material = chunk->get(Chunk::WIDTH - 1, localY);
                        if (material != MaterialType::Empty) {
                            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
                            
                            // Only process liquids for horizontal edge cases
                            if (props.isLiquid) {
                                MaterialType rightMaterial = chunkRight->get(0, localY);
                                if (rightMaterial == MaterialType::Empty) {
                                    // Liquid can flow right across chunk boundary
                                    // Always move all material to maintain volume
                                    chunkRight->set(0, localY, material);
                                    chunk->set(Chunk::WIDTH - 1, localY, MaterialType::Empty);
                                    
                                    // Mark chunks for next frame update
                                    chunk->setShouldUpdateNextFrame(true);
                                    chunkRight->setShouldUpdateNextFrame(true);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update all dirty chunks and propagate activity to neighbors
    for (int i = 0; i < (int)m_chunks.size(); ++i) {
        auto& chunk = m_chunks[i];
        if (chunk && chunk->isDirty()) {
            int y = i / m_chunksX;
            int x = i % m_chunksX;
            
            // Mark neighboring chunks for update next frame to ensure proper physics across chunk boundaries
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    // Skip self, already marked
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < m_chunksX && ny >= 0 && ny < m_chunksY) {
                        Chunk* neighbor = getChunkAt(nx, ny);
                        if (neighbor) {
                            neighbor->setShouldUpdateNextFrame(true);
                        }
                    }
                }
            }
            
            // Now update the chunk with references to its neighbors
            Chunk* chunkBelow = (y < m_chunksY - 1) ? getChunkAt(x, y + 1) : nullptr;
            Chunk* chunkLeft = (x > 0) ? getChunkAt(x - 1, y) : nullptr;
            Chunk* chunkRight = (x < m_chunksX - 1) ? getChunkAt(x + 1, y) : nullptr;
            
            chunk->update(chunkBelow, chunkLeft, chunkRight);
        }
    }
    
    // Update the combined pixel data from all chunks
    updateWorldPixelData();
}

void World::update(int startX, int startY, int endX, int endY) {
    // Bounds check
    startX = std::max(0, std::min(m_width - 1, startX));
    startY = std::max(0, std::min(m_height - 1, startY));
    endX = std::max(startX, std::min(m_width, endX));
    endY = std::max(startY, std::min(m_height, endY));
    
    // Convert world coordinates to chunk coordinates
    int startChunkX = startX / Chunk::WIDTH;
    int startChunkY = startY / Chunk::HEIGHT;
    int endChunkX = (endX + Chunk::WIDTH - 1) / Chunk::WIDTH;  // ceiling division
    int endChunkY = (endY + Chunk::HEIGHT - 1) / Chunk::HEIGHT;  // ceiling division
    
    // Bounds check for chunks
    startChunkX = std::max(0, std::min(m_chunksX - 1, startChunkX));
    startChunkY = std::max(0, std::min(m_chunksY - 1, startChunkY));
    endChunkX = std::max(startChunkX, std::min(m_chunksX, endChunkX));
    endChunkY = std::max(startChunkY, std::min(m_chunksY, endChunkY));
    
    // First pass: mark all chunks in the region and their neighbors as dirty
    for (int y = startChunkY; y < endChunkY; ++y) {
        for (int x = startChunkX; x < endChunkX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (chunk) {
                chunk->setDirty(true);
                
                // Mark neighboring chunks as dirty too for proper physics
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        // Skip self, already marked
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < m_chunksX && ny >= 0 && ny < m_chunksY) {
                            Chunk* neighbor = getChunkAt(nx, ny);
                            if (neighbor) {
                                neighbor->setDirty(true);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update chunks in the specified region with a bit of padding for proper physics at boundaries
    int paddedStartChunkX = std::max(0, startChunkX - 1);
    int paddedStartChunkY = std::max(0, startChunkY - 1);
    int paddedEndChunkX = std::min(m_chunksX, endChunkX + 1);
    int paddedEndChunkY = std::min(m_chunksY, endChunkY + 1);
    
    for (int y = paddedStartChunkY; y < paddedEndChunkY; ++y) {
        for (int x = paddedStartChunkX; x < paddedEndChunkX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (chunk && chunk->isDirty()) {
                Chunk* chunkBelow  = (y < m_chunksY - 1) ? getChunkAt(x, y + 1) : nullptr;
                Chunk* chunkLeft   = (x > 0)             ? getChunkAt(x - 1, y) : nullptr;
                Chunk* chunkRight  = (x < m_chunksX - 1) ? getChunkAt(x + 1, y) : nullptr;
                
                chunk->update(chunkBelow, chunkLeft, chunkRight);
            }
        }
    }
    
    // Update pixel data for the entire world
    // This is simpler than trying to update just a subset of the pixel data
    // updatePixelData(); // TEMP DISABLED FOR STREAMING SYSTEM MIGRATION
}

void World::levelLiquids() {
    // This function performs extra simulation steps to ensure liquids properly settle
    // It's useful after initial world generation or after a large change to the world
    
    // Mark all chunks as dirty to ensure everything gets processed
    for (int i = 0; i < (int)m_chunks.size(); ++i) {
        if (m_chunks[i]) {
            m_chunks[i]->setDirty(true);
        }
    }
    
    // Perform multiple update passes to allow liquids to flow and settle
    const int SETTLE_PASSES = 10;
    for (int pass = 0; pass < SETTLE_PASSES; ++pass) {
        update();
    }
}

void World::levelLiquids(int startX, int startY, int endX, int endY) {
    // Level liquids in a specific region of the world
    
    // Bounds check
    startX = std::max(0, std::min(m_width - 1, startX));
    startY = std::max(0, std::min(m_height - 1, startY));
    endX = std::max(startX, std::min(m_width, endX));
    endY = std::max(startY, std::min(m_height, endY));
    
    // Convert to chunk coordinates
    int startChunkX = startX / Chunk::WIDTH;
    int startChunkY = startY / Chunk::HEIGHT;
    int endChunkX = (endX + Chunk::WIDTH - 1) / Chunk::WIDTH;
    int endChunkY = (endY + Chunk::HEIGHT - 1) / Chunk::HEIGHT;
    
    // Add padding to ensure proper settling at region boundaries
    startChunkX = std::max(0, startChunkX - 1);
    startChunkY = std::max(0, startChunkY - 1);
    endChunkX = std::min(m_chunksX, endChunkX + 1);
    endChunkY = std::min(m_chunksY, endChunkY + 1);
    
    // Mark all chunks in region as dirty
    for (int y = startChunkY; y < endChunkY; ++y) {
        for (int x = startChunkX; x < endChunkX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (chunk) {
                chunk->setDirty(true);
            }
        }
    }
    
    // Perform multiple update passes on the region
    const int SETTLE_PASSES = 10;
    for (int pass = 0; pass < SETTLE_PASSES; ++pass) {
        update(startX - Chunk::WIDTH, startY - Chunk::HEIGHT, 
               endX + Chunk::WIDTH, endY + Chunk::HEIGHT);
    }
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
        // updatePixelData(); // TEMP DISABLED FOR STREAMING SYSTEM MIGRATION
        std::cout << "Terrain generated. Build and render to see the result before cave generation." << std::endl;
        std::cout << "Press any key to continue with cave generation..." << std::endl;
        // Here we'd pause in an interactive application
    }
    
    // ---- Cave Generation ----
    std::cout << "Generating cave system with cellular automata..." << std::endl;
    
    // Define the cave depth zones - Keep caves deeper underground
    const int upperCaveDepthStart = WORLD_HEIGHT / 2;     // Upper caves start at 1/2 depth
    const int midCaveDepthStart = 2 * WORLD_HEIGHT / 3;   // Mid caves start at 2/3 depth
    const int deepCaveDepthStart = 3 * WORLD_HEIGHT / 4;  // Deep caves start at 3/4 depth
    const int caveZoneHeight = WORLD_HEIGHT / 6;          // Each zone has a height of 1/6 world height
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    // Create cellular automata grid for each zone
    std::vector<std::vector<int>> upperCaveGrid(caveZoneHeight, std::vector<int>(WORLD_WIDTH, 0));
    std::vector<std::vector<int>> midCaveGrid(caveZoneHeight, std::vector<int>(WORLD_WIDTH, 0));
    std::vector<std::vector<int>> deepCaveGrid(caveZoneHeight, std::vector<int>(WORLD_WIDTH, 0));
    
    // Initialize the cave grids with random noise
    // Increased wall probability across all cave types to create more cohesive caves
    float upperWallProb = 0.65f; // More walls = smaller caves with more structure
    float midWallProb = 0.60f;   // More walls = more structured medium caves
    float deepWallProb = 0.55f;  // More walls = more defined large caves
    
    std::cout << "Initializing cave grids..." << std::endl;
    
    // Initialize upper cave grid (weak CA - smaller, isolated caves)
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            // Use Perlin noise for structured initial distribution - lower frequency for larger patterns
            float noiseFactor = perlinNoise2D(x * 0.03f, y * 0.03f, seed + 100);
            float probability = upperWallProb + (noiseFactor * 0.15f); // Stronger noise influence
            
            // Stronger edge bias to prevent caves at boundaries
            float edgeFactor = 1.0f;
            if (y < 10) edgeFactor = 0.9f + (y * 0.01f); // Top edge - stronger bias
            if (y > caveZoneHeight - 10) edgeFactor = 0.9f + ((caveZoneHeight - y) * 0.01f); // Bottom edge
            if (x < 10) edgeFactor = 0.9f + (x * 0.01f); // Left edge
            if (x > WORLD_WIDTH - 10) edgeFactor = 0.9f + ((WORLD_WIDTH - x) * 0.01f); // Right edge
            
            // Use pre-defined cave shapes as seeds for more natural formations
            float shapeFactor = 1.0f;
            // Create several oval-shaped cave seeds
            for (int seed = 0; seed < 4; seed++) {
                int centerX = 150 + (seed * WORLD_WIDTH / 4) + (m_rng() % (WORLD_WIDTH / 10));
                int centerY = caveZoneHeight / 2 + (m_rng() % (caveZoneHeight / 4) - caveZoneHeight / 8);
                int radiusX = 20 + (m_rng() % 20);
                int radiusY = 10 + (m_rng() % 10);
                
                float dx = (x - centerX) / static_cast<float>(radiusX);
                float dy = (y - centerY) / static_cast<float>(radiusY);
                float distSq = dx*dx + dy*dy;
                
                if (distSq < 1.0f) {
                    shapeFactor = 0.3f; // Lower probability inside cave seeds
                }
            }
            
            upperCaveGrid[y][x] = (dist(m_rng) < probability * edgeFactor * shapeFactor) ? 1 : 0;
        }
    }
    
    // Initialize mid cave grid (moderate CA - medium, connected caves)
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            // Use Perlin noise for more natural structure - lower frequency for larger patterns
            float noiseFactor = perlinNoise2D(x * 0.02f, y * 0.02f, seed + 200);
            float probability = midWallProb + (noiseFactor * 0.15f);
            
            // Stronger edge bias
            float edgeFactor = 1.0f;
            if (y < 10) edgeFactor = 0.9f + (y * 0.01f);
            if (y > caveZoneHeight - 10) edgeFactor = 0.9f + ((caveZoneHeight - y) * 0.01f);
            if (x < 10) edgeFactor = 0.9f + (x * 0.01f);
            if (x > WORLD_WIDTH - 10) edgeFactor = 0.9f + ((WORLD_WIDTH - x) * 0.01f);
            
            // Create cave seeds - larger for mid-level
            float shapeFactor = 1.0f;
            for (int seed = 0; seed < 3; seed++) {
                int centerX = 300 + (seed * WORLD_WIDTH / 3) + (m_rng() % (WORLD_WIDTH / 8));
                int centerY = caveZoneHeight / 2 + (m_rng() % (caveZoneHeight / 4) - caveZoneHeight / 8);
                int radiusX = 30 + (m_rng() % 25);
                int radiusY = 15 + (m_rng() % 15);
                
                float dx = (x - centerX) / static_cast<float>(radiusX);
                float dy = (y - centerY) / static_cast<float>(radiusY);
                float distSq = dx*dx + dy*dy;
                
                if (distSq < 1.0f) {
                    shapeFactor = 0.2f; // Lower probability inside cave seeds
                }
            }
            
            midCaveGrid[y][x] = (dist(m_rng) < probability * edgeFactor * shapeFactor) ? 1 : 0;
        }
    }
    
    // Initialize deep cave grid (strong CA - larger caves with natural feel)
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            // Use Perlin noise for more natural structure - lower frequency for larger patterns
            float noiseFactor = perlinNoise2D(x * 0.015f, y * 0.015f, seed + 300);
            float probability = deepWallProb + (noiseFactor * 0.15f);
            
            // Stronger edge bias
            float edgeFactor = 1.0f;
            if (y < 10) edgeFactor = 0.9f + (y * 0.01f);
            if (y > caveZoneHeight - 10) edgeFactor = 0.9f + ((caveZoneHeight - y) * 0.01f);
            if (x < 10) edgeFactor = 0.9f + (x * 0.01f);
            if (x > WORLD_WIDTH - 10) edgeFactor = 0.9f + ((WORLD_WIDTH - x) * 0.01f);
            
            // Create cave seeds - largest for deep level
            float shapeFactor = 1.0f;
            for (int seed = 0; seed < 2; seed++) {
                int centerX = WORLD_WIDTH / 4 + (seed * WORLD_WIDTH / 2) + (m_rng() % (WORLD_WIDTH / 6));
                int centerY = caveZoneHeight / 2 + (m_rng() % (caveZoneHeight / 4) - caveZoneHeight / 8);
                int radiusX = 40 + (m_rng() % 30);
                int radiusY = 20 + (m_rng() % 20);
                
                float dx = (x - centerX) / static_cast<float>(radiusX);
                float dy = (y - centerY) / static_cast<float>(radiusY);
                float distSq = dx*dx + dy*dy;
                
                if (distSq < 1.0f) {
                    shapeFactor = 0.15f; // Lower probability inside cave seeds
                }
            }
            
            deepCaveGrid[y][x] = (dist(m_rng) < probability * edgeFactor * shapeFactor) ? 1 : 0;
        }
    }
    
    // Function to count wall neighbors for a cell (including edge handling)
    auto countWallNeighbors = [](const std::vector<std::vector<int>>& grid, int x, int y) {
        int count = 0;
        int height = grid.size();
        int width = grid[0].size();
        
        for (int ny = y - 1; ny <= y + 1; ++ny) {
            for (int nx = x - 1; nx <= x + 1; ++nx) {
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    count++; // Treat out-of-bounds as walls
                } else if (!(nx == x && ny == y) && grid[ny][nx] == 1) {
                    count++;
                }
            }
        }
        return count;
    };
    
    std::cout << "Running weak cellular automata for upper caves..." << std::endl;
    
    // Run weak cellular automata for upper caves - conservative rules, fewer iterations
    int upperIterations = 3;
    for (int it = 0; it < upperIterations; ++it) {
        std::vector<std::vector<int>> newGrid = upperCaveGrid;
        for (int y = 0; y < caveZoneHeight; ++y) {
            for (int x = 0; x < WORLD_WIDTH; ++x) {
                int neighbors = countWallNeighbors(upperCaveGrid, x, y);
                
                if (upperCaveGrid[y][x] == 1) {
                    // Wall remains if 4 or more neighbors are walls
                    newGrid[y][x] = (neighbors >= 4) ? 1 : 0;
                } else {
                    // Empty becomes wall if 5 or more neighbors are walls
                    newGrid[y][x] = (neighbors >= 5) ? 1 : 0;
                }
            }
        }
        upperCaveGrid.swap(newGrid);
    }
    
    std::cout << "Running moderate cellular automata for mid caves..." << std::endl;
    
    // Run moderate cellular automata for mid caves - standard rules
    int midIterations = 4;
    for (int it = 0; it < midIterations; ++it) {
        std::vector<std::vector<int>> newGrid = midCaveGrid;
        for (int y = 0; y < caveZoneHeight; ++y) {
            for (int x = 0; x < WORLD_WIDTH; ++x) {
                int neighbors = countWallNeighbors(midCaveGrid, x, y);
                
                if (midCaveGrid[y][x] == 1) {
                    // Wall remains if 4 or more neighbors are walls
                    newGrid[y][x] = (neighbors >= 4) ? 1 : 0;
                } else {
                    // Empty becomes wall if 5 or more neighbors are walls
                    newGrid[y][x] = (neighbors >= 5) ? 1 : 0;
                }
            }
        }
        midCaveGrid.swap(newGrid);
    }
    
    std::cout << "Running strong cellular automata for deep caves..." << std::endl;
    
    // Run strong cellular automata for deep caves - more iterations, moderate rules
    int deepIterations = 5;
    for (int it = 0; it < deepIterations; ++it) {
        std::vector<std::vector<int>> newGrid = deepCaveGrid;
        for (int y = 0; y < caveZoneHeight; ++y) {
            for (int x = 0; x < WORLD_WIDTH; ++x) {
                int neighbors = countWallNeighbors(deepCaveGrid, x, y);
                
                if (deepCaveGrid[y][x] == 1) {
                    // Wall remains if 3 or more neighbors are walls (more caverns)
                    newGrid[y][x] = (neighbors >= 3) ? 1 : 0;
                } else {
                    // Empty becomes wall if 5 or more neighbors are walls (standard)
                    newGrid[y][x] = (neighbors >= 5) ? 1 : 0;
                }
            }
        }
        deepCaveGrid.swap(newGrid);
    }
    
    // Additional smoothing pass
    std::cout << "Running extra smoothing passes..." << std::endl;
    
    // Function for smoothing caves (removes isolated walls and fills small holes)
    auto smoothCaveGrid = [&countWallNeighbors](std::vector<std::vector<int>>& grid) {
        std::vector<std::vector<int>> newGrid = grid;
        for (size_t y = 0; y < grid.size(); ++y) {
            for (size_t x = 0; x < grid[0].size(); ++x) {
                int neighbors = countWallNeighbors(grid, x, y);
                
                // Remove single wall tiles surrounded by 6+ empty spaces
                if (grid[y][x] == 1 && neighbors <= 2) {
                    newGrid[y][x] = 0;
                }
                
                // Fill single empty tiles surrounded by 6+ walls
                if (grid[y][x] == 0 && neighbors >= 6) {
                    newGrid[y][x] = 1;
                }
            }
        }
        grid.swap(newGrid);
    };
    
    // Apply smoothing to all cave grids
    smoothCaveGrid(upperCaveGrid);
    smoothCaveGrid(midCaveGrid);
    smoothCaveGrid(deepCaveGrid);
    
    std::cout << "Applying cellular automata results to world..." << std::endl;
    
    // Apply the cave grids to the actual world
    // Upper caves
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            if (upperCaveGrid[y][x] == 0) { // 0 = cave (empty)
                int worldY = upperCaveDepthStart + y;
                if (worldY < WORLD_HEIGHT) {
                    MaterialType material = get(x, worldY);
                    if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                        set(x, worldY, MaterialType::Empty);
                    }
                }
            }
        }
    }
    
    // Mid caves
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            if (midCaveGrid[y][x] == 0) { // 0 = cave (empty)
                int worldY = midCaveDepthStart + y;
                if (worldY < WORLD_HEIGHT) {
                    MaterialType material = get(x, worldY);
                    if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                        set(x, worldY, MaterialType::Empty);
                    }
                }
            }
        }
    }
    
    // Deep caves
    for (int y = 0; y < caveZoneHeight; ++y) {
        for (int x = 0; x < WORLD_WIDTH; ++x) {
            if (deepCaveGrid[y][x] == 0) { // 0 = cave (empty)
                int worldY = deepCaveDepthStart + y;
                if (worldY < WORLD_HEIGHT) {
                    MaterialType material = get(x, worldY);
                    if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                        set(x, worldY, MaterialType::Empty);
                    }
                }
            }
        }
    }
    
    // Create more natural connections between cave systems
    std::cout << "Creating cave system connections..." << std::endl;
    
    // Use Perlin worms with variable thickness to connect between cave regions
    int numConnectors = 10 + (m_rng() % 6); // 10-15 connectors - more but more subtle
    
    // Function to create a natural-looking cave connection
    auto createCaveConnection = [&](int startX, int startY, int endX, int endY, int seed_offset) {
        // Get distance between points
        int dx = endX - startX;
        int dy = endY - startY;
        float distance = std::sqrt(dx*dx + dy*dy);
        
        // Start with angle toward destination
        float angle = std::atan2(dy, dx);
        
        // Current position
        float x = startX;
        float y = startY;
        
        // Randomized parameters for this connector
        float segmentLength = 3.0f + dist(m_rng) * 2.0f; // 3-5 segment length
        float angleVariation = 0.2f + dist(m_rng) * 0.3f; // 0.2-0.5 angle variation
        float widthVariation = 0.5f + dist(m_rng) * 0.5f; // 0.5-1.0 width variation
        int baseRadius = 1 + (m_rng() % 2); // 1-2 base radius
        float radiusScale = 0.1f + dist(m_rng) * 0.3f; // How much radius varies
        float noiseScale = 0.05f + dist(m_rng) * 0.05f;
        
        // Calculate number of steps needed (with some randomness)
        int steps = static_cast<int>(distance / segmentLength) + 5 + (m_rng() % 10);
        
        for (int step = 0; step < steps; step++) {
            // Calculate progress ratio (0 to 1)
            float progress = static_cast<float>(step) / steps;
            
            // Vary the direction more at the middle of the path, less at start/end
            float directionFreedom = angleVariation * (1.0f - std::abs(progress - 0.5f) * 2.0f);
            
            // Use Perlin noise for direction changes
            float noiseVal = perlinNoise2D(step * 0.1f, 0, seed + seed_offset);
            float angleChange = (noiseVal * 2.0f - 1.0f) * directionFreedom;
            
            // Gradually bend back toward destination as we get closer
            float targetAngle = std::atan2(endY - y, endX - x);
            float angleToTarget = targetAngle - angle;
            // Normalize angle difference to [-PI, PI]
            while (angleToTarget > M_PI) angleToTarget -= 2.0f * M_PI;
            while (angleToTarget < -M_PI) angleToTarget += 2.0f * M_PI;
            
            // Apply stronger correction as we get further through the path
            angle += angleChange + (angleToTarget * 0.1f * (step > steps / 2 ? progress * 2.0f : 0.1f));
            
            // Move forward
            x += std::cos(angle) * segmentLength;
            y += std::sin(angle) * segmentLength;
            
            // Bound check
            if (x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT) {
                break;
            }
            
            // Vary the tunnel width - wider in the middle, narrower at ends
            float widthFactor = widthVariation * (1.0f - std::abs(progress - 0.5f) * 1.8f);
            
            // Add noise to width
            float widthNoise = perlinNoise2D(x * noiseScale, y * noiseScale, seed + seed_offset + 500);
            float currentRadius = baseRadius + (widthNoise * radiusScale) + widthFactor;
            
            // Carve a small cave section here
            int radius = static_cast<int>(currentRadius) + 1;
            for (int cy = -radius; cy <= radius; cy++) {
                for (int cx = -radius; cx <= radius; cx++) {
                    // Oval shape - slightly wider horizontally
                    float distSq = (cx*cx) / (radius*radius * 1.2f) + (cy*cy) / (radius*radius);
                    if (distSq <= 1.0f) {
                        int px = static_cast<int>(x) + cx;
                        int py = static_cast<int>(y) + cy;
                        
                        if (px >= 0 && px < WORLD_WIDTH && py >= 0 && py < WORLD_HEIGHT) {
                            MaterialType material = get(px, py);
                            if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                                set(px, py, MaterialType::Empty);
                            }
                        }
                    }
                }
            }
            
            // Sometimes create a small cave pocket
            if (m_rng() % 15 == 0) {
                int pocketRadius = 2 + (m_rng() % 3);
                float pocketAngle = angle + ((dist(m_rng) * 2.0f - 1.0f) * M_PI * 0.5f);
                float pocketDist = 3.0f + dist(m_rng) * 4.0f;
                
                float px = x + std::cos(pocketAngle) * pocketDist;
                float py = y + std::sin(pocketAngle) * pocketDist;
                
                // Carve a small pocket
                for (int cy = -pocketRadius; cy <= pocketRadius; cy++) {
                    for (int cx = -pocketRadius; cx <= pocketRadius; cx++) {
                        float distSq = (cx*cx + cy*cy) / static_cast<float>(pocketRadius * pocketRadius);
                        if (distSq <= 1.0f) {
                            int wx = static_cast<int>(px) + cx;
                            int wy = static_cast<int>(py) + cy;
                            
                            if (wx >= 0 && wx < WORLD_WIDTH && wy >= 0 && wy < WORLD_HEIGHT) {
                                MaterialType material = get(wx, wy);
                                if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    
    // Connect between different cave layers
    for (int i = 0; i < numConnectors; ++i) {
        // Start and end points in actual caves, not just random points
        // Select a point in one system
        int startX, startY, endX, endY;
        
        // For vertical connections between layers
        int layerChoice = m_rng() % 2; // 0 = upper to mid, 1 = mid to deep
        
        if (layerChoice == 0) {
            // Connect upper to mid cave
            startX = 100 + (m_rng() % (WORLD_WIDTH - 200));
            startY = upperCaveDepthStart + (m_rng() % caveZoneHeight);
            endX = startX + (m_rng() % 200) - 100; // Slight horizontal offset
            endY = midCaveDepthStart + (m_rng() % caveZoneHeight); 
        } else {
            // Connect mid to deep cave 
            startX = 100 + (m_rng() % (WORLD_WIDTH - 200));
            startY = midCaveDepthStart + (m_rng() % caveZoneHeight);
            endX = startX + (m_rng() % 200) - 100; // Slight horizontal offset
            endY = deepCaveDepthStart + (m_rng() % caveZoneHeight);
        }
        
        // Create natural-looking connection
        createCaveConnection(startX, startY, endX, endY, i * 100);
    }
    
    // Create horizontal connections within each layer
    std::cout << "Enhancing cave network connectivity..." << std::endl;
    
    // For each layer, connect cave areas horizontally 
    for (int layer = 0; layer < 3; ++layer) {
        int baseY = 0;
        if (layer == 0) baseY = upperCaveDepthStart + (caveZoneHeight / 2);
        else if (layer == 1) baseY = midCaveDepthStart + (caveZoneHeight / 2);
        else baseY = deepCaveDepthStart + (caveZoneHeight / 2);
        
        // Create multiple connections per layer
        int connectionsPerLayer = 3 + (m_rng() % 3); // 3-5 connections
        
        for (int c = 0; c < connectionsPerLayer; ++c) {
            // Choose points with significant horizontal separation
            int startX = 50 + (m_rng() % (WORLD_WIDTH / 3));
            int endX = (2 * WORLD_WIDTH / 3) + (m_rng() % (WORLD_WIDTH / 3 - 50));
            
            int startY = baseY + (m_rng() % 30) - 15; // Vary around the middle
            int endY = baseY + (m_rng() % 30) - 15;
            
            // Create natural-looking connection
            createCaveConnection(startX, startY, endX, endY, layer * 1000 + c * 100);
        }
    }
    
    // Optional: Create a few very rare surface entrances - EXTREMELY RARE
    std::cout << "Creating rare surface entrances..." << std::endl;
    
    int numSurfaceEntrances = 1 + (m_rng() % 2); // 1-2 entrances only
    
    for (int i = 0; i < numSurfaceEntrances; ++i) {
        // Choose random X position far from edges
        int entranceX = 150 + (m_rng() % (WORLD_WIDTH - 300));
        
        // Find surface at this X
        int surfaceY = 0;
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            if (get(entranceX, y) != MaterialType::Empty) {
                surfaceY = y;
                break;
            }
        }
        
        if (surfaceY > 0) {
            // Target a point in the upper cave system
            int targetY = upperCaveDepthStart + (m_rng() % (caveZoneHeight / 2));
            
            // Create a narrow winding shaft downward
            float currentX = entranceX;
            int width = 2; // Fixed narrow width
            float wiggleAmount = 0.4f; // More winding
            float noiseScale = 0.03f;
            
            for (int y = surfaceY; y <= targetY; ++y) {
                // Add more pronounced wiggle
                float noise = perlinNoise2D(0, y * noiseScale, seed + i * 5000);
                currentX += (noise - 0.5f) * wiggleAmount;
                
                int centerX = static_cast<int>(currentX);
                
                // Make the entrance wider near the surface and narrow as it goes down
                int currentWidth = width;
                if (y < surfaceY + 5) {
                    currentWidth = 3; // Wider at entrance
                }
                
                // Carve the shaft at this Y level
                for (int x = centerX - currentWidth; x <= centerX + currentWidth; ++x) {
                    if (x >= 0 && x < WORLD_WIDTH) {
                        MaterialType material = get(x, y);
                        if (material != MaterialType::Empty && material != MaterialType::Bedrock) {
                            set(x, y, MaterialType::Empty);
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "Cellular automata cave generation complete!" << std::endl;

    if (renderStepByStep) {
        // Update pixel data after cave generation
        // updatePixelData(); // TEMP DISABLED FOR STREAMING SYSTEM MIGRATION
        std::cout << "Caves generated. Build and render to see the final cave system." << std::endl;
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
        // updatePixelData(); // TEMP DISABLED FOR STREAMING SYSTEM MIGRATION
        std::cout << "Ore deposits generated. Build and render to see the final world." << std::endl;
    } else {
        // Update pixel data
        // updatePixelData(); // TEMP DISABLED FOR STREAMING SYSTEM MIGRATION
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
        if (m_rng() % 25 == 0) {
            // Small turn up to 45 degrees
            angleChange = (m_rng() % 145) * 3.14159f / 180.0f;
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

void World::updateWorldPixelData() {
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
