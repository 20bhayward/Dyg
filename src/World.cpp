#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring> // For memcpy

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
        
        // Use deterministic position-based variation for consistent textures
        // This ensures aligned materials have consistent textures rather than random
        int posHash = ((x * 13) + (y * 7)) % 32;
        int variation = (posHash % 7) - 3;
        
        m_pixelData[pixelIdx] = std::max(0, std::min(255, static_cast<int>(props.r) + variation));   // R
        m_pixelData[pixelIdx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + variation)); // G
        m_pixelData[pixelIdx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + variation)); // B
        m_pixelData[pixelIdx+3] = props.transparency;  // A (use material transparency)
    }
}

void Chunk::update(Chunk* chunkBelow, Chunk* chunkLeft, Chunk* chunkRight) {
    if (!m_isDirty) {
        return;
    }
    
    // Always start dirty - will be reset at end if nothing moved
    m_isDirty = true;
    
    // We need to avoid updating a cell twice in the same frame
    // So we create a temporary copy of the grid to read from
    std::vector<MaterialType> oldGrid = m_grid;
    
    // Handle material interactions (fire spreading, etc.)
    // Add this before the normal updates to prioritize reactive interactions
    handleMaterialInteractions(oldGrid);
    
    // Use a checkerboard update pattern (even cells first, then odd cells)
    // This helps prevent artifacts when multiple cells want to move to the same location
    
    // First pass: update cells where (x+y) is even
    for (int y = HEIGHT-1; y >= 0; --y) {
        for (int x = (y % 2); x < WIDTH; x += 2) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            // Skip empty cells and static solids like stone
            if (material == MaterialType::Empty || 
                (MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isSolid &&
                 !MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isFlammable)) {
                continue;
            }
            
            // Handle different materials
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Sand and other powders
            if (props.isPowder) {
                // Check if material is enclosed (trapped on all sides)
                bool isEnclosed = true;
                
                // Check surrounding cells
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        // Skip self
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // For boundary cells, check neighboring chunks
                        MaterialType neighborMaterial = MaterialType::Empty;
                        
                        if (nx < 0) {
                            // Left chunk boundary
                            if (chunkLeft) {
                                neighborMaterial = chunkLeft->get(WIDTH-1, ny);
                            }
                        } 
                        else if (nx >= WIDTH) {
                            // Right chunk boundary
                            if (chunkRight) {
                                neighborMaterial = chunkRight->get(0, ny);
                            }
                        }
                        else if (ny >= HEIGHT) {
                            // Bottom chunk boundary
                            if (chunkBelow) {
                                neighborMaterial = chunkBelow->get(nx, 0);
                            }
                        }
                        else if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                            // Within this chunk
                            neighborMaterial = oldGrid[ny * WIDTH + nx];
                        }
                        
                        // If there's an empty space, the material is not enclosed
                        if (neighborMaterial == MaterialType::Empty) {
                            isEnclosed = false;
                            break;
                        }
                    }
                    if (!isEnclosed) break;
                }
                
                // If the material is enclosed, treat it as a solid (skip physics)
                if (isEnclosed) {
                    continue;
                }
                
                // Check if at bottom of chunk and can fall to chunk below
                if (y+1 >= HEIGHT) {
                    if (chunkBelow && chunkBelow->get(x, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x, 0, material);
                        chunkBelow->setDirty(true);
                        continue;
                    }
                }
                // Try to fall straight down (within this chunk)
                else if (y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y+1) * WIDTH + x] = material;
                    continue;
                } 
                // If there's a particle below that's also a powder, check if it's settled
                else if (y+1 < HEIGHT) {
                    MaterialType belowMaterial = oldGrid[(y+1) * WIDTH + x];
                    if (belowMaterial != MaterialType::Empty && 
                        MATERIAL_PROPERTIES[static_cast<std::size_t>(belowMaterial)].isPowder) {
                        // Check if powder below has support
                        bool hasSupport = false;
                        if (y+2 < HEIGHT) {
                            if (oldGrid[(y+2) * WIDTH + x] != MaterialType::Empty) {
                                hasSupport = true;
                            }
                            if (x > 0 && oldGrid[(y+2) * WIDTH + (x-1)] != MaterialType::Empty) {
                                hasSupport = true;
                            }
                            if (x+1 < WIDTH && oldGrid[(y+2) * WIDTH + (x+1)] != MaterialType::Empty) {
                                hasSupport = true;
                            }
                        }
                        
                        // If the powder below doesn't have support, mark this chunk as dirty
                        // to ensure we'll continue updating it
                        if (!hasSupport) {
                            m_isDirty = true;
                        }
                    }
                }
                
                // Try to fall diagonally
                bool fellLeft = false;
                bool fellRight = false;
                
                // Check if we're at the bottom of chunk
                if (y+1 >= HEIGHT) {
                    // Try left diagonal to chunk below
                    if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x-1, 0, material);
                        chunkBelow->setDirty(true);
                        fellLeft = true;
                    }
                    // Try right diagonal to chunk below
                    else if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x+1, 0, material);
                        chunkBelow->setDirty(true);
                        fellRight = true;
                    }
                }
                else if (y+1 < HEIGHT) {
                    // Try left diagonal within this chunk
                    if (x > 0 && oldGrid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y+1) * WIDTH + (x-1)] = material;
                        fellLeft = true;
                    }
                    // Try right diagonal within this chunk
                    else if (x+1 < WIDTH && oldGrid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y+1) * WIDTH + (x+1)] = material;
                        fellRight = true;
                    }
                }
                
                // If we fell, we're done with this cell
                if (fellLeft || fellRight) {
                    continue;
                }
            }
            
            // Water and other liquids
            else if (props.isLiquid) {
                // Check if material is enclosed (trapped on all sides)
                bool isEnclosed = true;
                
                // Check surrounding cells
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        // Skip self
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // For boundary cells, check neighboring chunks
                        MaterialType neighborMaterial = MaterialType::Empty;
                        
                        if (nx < 0) {
                            // Left chunk boundary
                            if (chunkLeft) {
                                neighborMaterial = chunkLeft->get(WIDTH-1, ny);
                            }
                        } 
                        else if (nx >= WIDTH) {
                            // Right chunk boundary
                            if (chunkRight) {
                                neighborMaterial = chunkRight->get(0, ny);
                            }
                        }
                        else if (ny >= HEIGHT) {
                            // Bottom chunk boundary
                            if (chunkBelow) {
                                neighborMaterial = chunkBelow->get(nx, 0);
                            }
                        }
                        else if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                            // Within this chunk
                            neighborMaterial = oldGrid[ny * WIDTH + nx];
                        }
                        
                        // If there's an empty space, the material is not enclosed
                        if (neighborMaterial == MaterialType::Empty) {
                            isEnclosed = false;
                            break;
                        }
                    }
                    if (!isEnclosed) break;
                }
                
                // If the material is enclosed, treat it as a solid (skip physics)
                if (isEnclosed) {
                    continue;
                }
                
                // Check if at bottom of chunk and can fall to chunk below
                if (y+1 >= HEIGHT) {
                    if (chunkBelow && chunkBelow->get(x, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x, 0, material);
                        chunkBelow->setDirty(true);
                        continue;
                    }
                }
                // Try to fall straight down (within this chunk)
                else if (y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y+1) * WIDTH + x] = material;
                    continue;
                }
                
                // Try diagonal down movement
                bool movedDiagonal = false;
                
                // Check if we're at the bottom of chunk
                if (y+1 >= HEIGHT) {
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try left-down first
                        if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x-1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        } 
                        // Then right-down
                        else if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x+1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                    } else {
                        // Try right-down first
                        if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x+1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                        // Then left-down 
                        else if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x-1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                    }
                }
                // Within this chunk
                else {
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try left-down first
                        if (x > 0 && y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x-1)] = material;
                            movedDiagonal = true;
                        } 
                        // Then right-down
                        else if (x+1 < WIDTH && y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x+1)] = material;
                            movedDiagonal = true;
                        }
                    } else {
                        // Try right-down first
                        if (x+1 < WIDTH && y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x+1)] = material;
                            movedDiagonal = true;
                        }
                        // Then left-down 
                        else if (x > 0 && y+1 < HEIGHT && oldGrid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x-1)] = material;
                            movedDiagonal = true;
                        }
                    }
                }
                
                if (movedDiagonal) {
                    continue;
                }

                // If couldn't move down, try to spread horizontally
                // Look up to 3 cells in each direction to make liquid flow better
                const int MAX_FLOW_DISTANCE = 3;
                bool flowed = false;
                
                // Deterministic direction choice to avoid bias
                if ((x + y) % 2 == 0) {
                    // Try to flow left
                    for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                        int nx = x - d;
                        
                        // If we hit the left edge, check chunk to the left
                        if (nx < 0) {
                            if (chunkLeft) {
                                int leftX = WIDTH + nx; // Convert negative nx to position in left chunk
                                if (chunkLeft->get(leftX, y) == MaterialType::Empty) {
                                    // Make sure there's no hole underneath in left chunk
                                    if (y+1 >= HEIGHT || 
                                        chunkLeft->get(leftX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        chunkLeft->set(leftX, y, material);
                                        chunkLeft->setDirty(true);
                                        flowed = true;
                                        break;
                                    }
                                }
                            }
                            break; // Stop looking left
                        }
                        
                        // Normal flow within chunk
                        if (oldGrid[y * WIDTH + nx] == MaterialType::Empty) {
                            // Make sure there's no hole under this position
                            if (y+1 >= HEIGHT) {
                                if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else if (oldGrid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                m_grid[idx] = MaterialType::Empty;
                                m_grid[y * WIDTH + nx] = material;
                                flowed = true;
                                break;
                            }
                        } else {
                            // Hit an obstacle, stop looking in this direction
                            break;
                        }
                    }
                    
                    // Try to flow right if didn't flow left
                    if (!flowed) {
                        for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                            int nx = x + d;
                            
                            // If we hit the right edge, check chunk to the right
                            if (nx >= WIDTH) {
                                if (chunkRight) {
                                    int rightX = nx - WIDTH; // Convert to position in right chunk
                                    if (chunkRight->get(rightX, y) == MaterialType::Empty) {
                                        // Make sure there's no hole underneath in right chunk
                                        if (y+1 >= HEIGHT || 
                                            chunkRight->get(rightX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                            m_grid[idx] = MaterialType::Empty;
                                            chunkRight->set(rightX, y, material);
                                            chunkRight->setDirty(true);
                                            flowed = true;
                                            break;
                                        }
                                    }
                                }
                                break; // Stop looking right
                            }
                            
                            // Normal flow within chunk
                            if (oldGrid[y * WIDTH + nx] == MaterialType::Empty) {
                                // Make sure there's no hole under this position
                                if (y+1 >= HEIGHT) {
                                    if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        m_grid[y * WIDTH + nx] = material;
                                        flowed = true;
                                        break;
                                    }
                                } else if (oldGrid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else {
                                // Hit an obstacle, stop looking in this direction
                                break;
                            }
                        }
                    }
                } else {
                    // Try to flow right first
                    for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                        int nx = x + d;
                        
                        // If we hit the right edge, check chunk to the right
                        if (nx >= WIDTH) {
                            if (chunkRight) {
                                int rightX = nx - WIDTH; // Convert to position in right chunk
                                if (chunkRight->get(rightX, y) == MaterialType::Empty) {
                                    // Make sure there's no hole underneath in right chunk
                                    if (y+1 >= HEIGHT || 
                                        chunkRight->get(rightX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        chunkRight->set(rightX, y, material);
                                        chunkRight->setDirty(true);
                                        flowed = true;
                                        break;
                                    }
                                }
                            }
                            break; // Stop looking right
                        }
                        
                        // Normal flow within chunk
                        if (oldGrid[y * WIDTH + nx] == MaterialType::Empty) {
                            // Make sure there's no hole under this position
                            if (y+1 >= HEIGHT) {
                                if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else if (oldGrid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                m_grid[idx] = MaterialType::Empty;
                                m_grid[y * WIDTH + nx] = material;
                                flowed = true;
                                break;
                            }
                        } else {
                            // Hit an obstacle, stop looking in this direction
                            break;
                        }
                    }
                    
                    // Try to flow left if didn't flow right
                    if (!flowed) {
                        for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                            int nx = x - d;
                            
                            // If we hit the left edge, check chunk to the left
                            if (nx < 0) {
                                if (chunkLeft) {
                                    int leftX = WIDTH + nx; // Convert negative nx to position in left chunk
                                    if (chunkLeft->get(leftX, y) == MaterialType::Empty) {
                                        // Make sure there's no hole underneath in left chunk
                                        if (y+1 >= HEIGHT || 
                                            chunkLeft->get(leftX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                            m_grid[idx] = MaterialType::Empty;
                                            chunkLeft->set(leftX, y, material);
                                            chunkLeft->setDirty(true);
                                            flowed = true;
                                            break;
                                        }
                                    }
                                }
                                break; // Stop looking left
                            }
                            
                            // Normal flow within chunk
                            if (oldGrid[y * WIDTH + nx] == MaterialType::Empty) {
                                // Make sure there's no hole under this position
                                if (y+1 >= HEIGHT) {
                                    if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        m_grid[y * WIDTH + nx] = material;
                                        flowed = true;
                                        break;
                                    }
                                } else if (oldGrid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else {
                                // Hit an obstacle, stop looking in this direction
                                break;
                            }
                        }
                    }
                }
                
                if (flowed) {
                    continue;
                }
            }
            
            // Gases (rise up) - Improved gas physics
            else if (props.isGas) {
                // Different behavior for different gases
                bool isSteam = (material == MaterialType::Steam);
                bool isSmoke = (material == MaterialType::Smoke);
                
                // Gases can sometimes dissipate (especially steam)
                int dissipationChance = isSteam ? 5 : (isSmoke ? 10 : 50);
                if (static_cast<int>(m_rng() % 100) < dissipationChance) {
                    // The gas dissipates
                    m_grid[idx] = MaterialType::Empty;
                    continue;
                }
                
                // Gases try to rise quickly for steam, medium for smoke, slower for others
                int riseChance = isSteam ? 95 : (isSmoke ? 90 : 80);
                
                // Try to rise upward with appropriate speed
                if (y > 0 && static_cast<int>(m_rng() % 100) < riseChance) {
                    // Try to rise directly upward
                    if (oldGrid[(y-1) * WIDTH + x] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y-1) * WIDTH + x] = material;
                        continue;
                    }
                    
                    // If upward is blocked, check diagonally up
                    bool diagonalMoved = false;
                    
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try up-left first
                        if (x > 0 && oldGrid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            diagonalMoved = true;
                        }
                        // Then up-right
                        else if (x+1 < WIDTH && oldGrid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
                            diagonalMoved = true;
                        }
                    } else {
                        // Try up-right first
                        if (x+1 < WIDTH && oldGrid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
                            diagonalMoved = true;
                        }
                        // Then up-left
                        else if (x > 0 && oldGrid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            diagonalMoved = true;
                        }
                    }
                    
                    if (diagonalMoved) {
                        continue;
                    }
                }
                
                // If rise failed, try horizontal spread (gases like to fill available space)
                // Chance of spreading is different for each gas
                int spreadChance = isSteam ? 40 : (isSmoke ? 60 : 50);
                
                if (static_cast<int>(m_rng() % 100) < spreadChance) {
                    bool spreadLeft = false;
                    bool spreadRight = false;
                    
                    // Deterministic direction choice to avoid bias
                    if ((x + y) % 2 == 0) {
                        // Try left
                        if (x > 0 && oldGrid[y * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x-1)] = material;
                            spreadLeft = true;
                        }
                        // Try right
                        else if (x+1 < WIDTH && oldGrid[y * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x+1)] = material;
                            spreadRight = true;
                        }
                    } else {
                        // Try right
                        if (x+1 < WIDTH && oldGrid[y * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x+1)] = material;
                            spreadRight = true;
                        }
                        // Try left
                        else if (x > 0 && oldGrid[y * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x-1)] = material;
                            spreadLeft = true;
                        }
                    }
                    
                    if (spreadLeft || spreadRight) {
                        continue;
                    }
                }
                
                // Special case: if steam touches water, convert to water
                if (isSteam) {
                    // Check all surrounding cells for water
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                                if (oldGrid[ny * WIDTH + nx] == MaterialType::Water) {
                                    // Contact with water causes steam to condensate
                                    if (static_cast<int>(m_rng() % 100) < 30) { // 30% chance to condensate
                                        m_grid[idx] = MaterialType::Empty;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                // Special case: smoke near fire
                else if (isSmoke) {
                    // Check if near fire - if so, smoke rises faster
                    bool nearFire = false;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                                if (oldGrid[ny * WIDTH + nx] == MaterialType::Fire) {
                                    nearFire = true;
                                    break;
                                }
                            }
                        }
                        if (nearFire) break;
                    }
                    
                    // Smoke near fire has a chance to move upward even if no empty space
                    if (nearFire && y > 0 && static_cast<int>(m_rng() % 100) < 30) {
                        // Enhanced rising due to heat - can displace air
                        MaterialType aboveMaterial = oldGrid[(y-1) * WIDTH + x];
                        if (aboveMaterial == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + x] = material;
                            continue;
                        }
                    }
                }
            }
        }
    }
    
    // Second pass: update cells where (x+y) is odd
    for (int y = HEIGHT-1; y >= 0; --y) {
        for (int x = ((y % 2) == 0 ? 1 : 0); x < WIDTH; x += 2) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            // Skip empty cells and static solids
            if (material == MaterialType::Empty || 
                (MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isSolid &&
                 !MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isFlammable)) {
                continue;
            }
            
            // Handle different materials
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Sand and other powders
            if (props.isPowder) {
                // Check if at bottom of chunk and can fall to chunk below
                if (y+1 >= HEIGHT) {
                    if (chunkBelow && chunkBelow->get(x, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x, 0, material);
                        chunkBelow->setDirty(true);
                        continue;
                    }
                }
                // Try to fall straight down (within this chunk)
                else if (y+1 < HEIGHT && m_grid[(y+1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y+1) * WIDTH + x] = material;
                    continue;
                }
                
                // Try to fall diagonally
                bool fellLeft = false;
                bool fellRight = false;
                
                // Check if we're at the bottom of chunk
                if (y+1 >= HEIGHT) {
                    // Try left diagonal to chunk below
                    if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x-1, 0, material);
                        chunkBelow->setDirty(true);
                        fellLeft = true;
                    }
                    // Try right diagonal to chunk below
                    else if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x+1, 0, material);
                        chunkBelow->setDirty(true);
                        fellRight = true;
                    }
                }
                else if (y+1 < HEIGHT) {
                    // Try left diagonal within this chunk
                    if (x > 0 && m_grid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y+1) * WIDTH + (x-1)] = material;
                        fellLeft = true;
                    }
                    // Try right diagonal within this chunk
                    else if (x+1 < WIDTH && m_grid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y+1) * WIDTH + (x+1)] = material;
                        fellRight = true;
                    }
                }
                
                // If we fell, we're done with this cell
                if (fellLeft || fellRight) {
                    continue;
                }
            }
            
            // Water and other liquids
            else if (props.isLiquid) {
                // Check if at bottom of chunk and can fall to chunk below
                if (y+1 >= HEIGHT) {
                    if (chunkBelow && chunkBelow->get(x, 0) == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        chunkBelow->set(x, 0, material);
                        chunkBelow->setDirty(true);
                        continue;
                    }
                }
                // Try to fall straight down (within this chunk)
                else if (y+1 < HEIGHT && m_grid[(y+1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y+1) * WIDTH + x] = material;
                    continue;
                }
                
                // Try diagonal down movement
                bool movedDiagonal = false;
                
                // Check if we're at the bottom of chunk
                if (y+1 >= HEIGHT) {
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try left-down first
                        if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x-1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        } 
                        // Then right-down
                        else if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x+1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                    } else {
                        // Try right-down first
                        if (x+1 < WIDTH && chunkBelow && chunkBelow->get(x+1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x+1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                        // Then left-down 
                        else if (x > 0 && chunkBelow && chunkBelow->get(x-1, 0) == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            chunkBelow->set(x-1, 0, material);
                            chunkBelow->setDirty(true);
                            movedDiagonal = true;
                        }
                    }
                }
                // Within this chunk
                else {
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try left-down first
                        if (x > 0 && y+1 < HEIGHT && m_grid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x-1)] = material;
                            movedDiagonal = true;
                        } 
                        // Then right-down
                        else if (x+1 < WIDTH && y+1 < HEIGHT && m_grid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x+1)] = material;
                            movedDiagonal = true;
                        }
                    } else {
                        // Try right-down first
                        if (x+1 < WIDTH && y+1 < HEIGHT && m_grid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x+1)] = material;
                            movedDiagonal = true;
                        }
                        // Then left-down 
                        else if (x > 0 && y+1 < HEIGHT && m_grid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y+1) * WIDTH + (x-1)] = material;
                            movedDiagonal = true;
                        }
                    }
                }
                
                if (movedDiagonal) {
                    continue;
                }

                // If couldn't move down, try to spread horizontally
                // Look up to 3 cells in each direction to make liquid flow better
                const int MAX_FLOW_DISTANCE = 3;
                bool flowed = false;
                
                // Deterministic direction choice to avoid bias
                if ((x + y) % 2 == 0) {
                    // Try to flow left
                    for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                        int nx = x - d;
                        
                        // If we hit the left edge, check chunk to the left
                        if (nx < 0) {
                            if (chunkLeft) {
                                int leftX = WIDTH + nx; // Convert negative nx to position in left chunk
                                if (chunkLeft->get(leftX, y) == MaterialType::Empty) {
                                    // Make sure there's no hole underneath in left chunk
                                    if (y+1 >= HEIGHT || 
                                        chunkLeft->get(leftX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        chunkLeft->set(leftX, y, material);
                                        chunkLeft->setDirty(true);
                                        flowed = true;
                                        break;
                                    }
                                }
                            }
                            break; // Stop looking left
                        }
                        
                        // Normal flow within chunk
                        if (m_grid[y * WIDTH + nx] == MaterialType::Empty) {
                            // Make sure there's no hole under this position
                            if (y+1 >= HEIGHT) {
                                if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else if (m_grid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                m_grid[idx] = MaterialType::Empty;
                                m_grid[y * WIDTH + nx] = material;
                                flowed = true;
                                break;
                            }
                        } else {
                            // Hit an obstacle, stop looking in this direction
                            break;
                        }
                    }
                    
                    // Try to flow right if didn't flow left
                    if (!flowed) {
                        for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                            int nx = x + d;
                            
                            // If we hit the right edge, check chunk to the right
                            if (nx >= WIDTH) {
                                if (chunkRight) {
                                    int rightX = nx - WIDTH; // Convert to position in right chunk
                                    if (chunkRight->get(rightX, y) == MaterialType::Empty) {
                                        // Make sure there's no hole underneath in right chunk
                                        if (y+1 >= HEIGHT || 
                                            chunkRight->get(rightX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                            m_grid[idx] = MaterialType::Empty;
                                            chunkRight->set(rightX, y, material);
                                            chunkRight->setDirty(true);
                                            flowed = true;
                                            break;
                                        }
                                    }
                                }
                                break; // Stop looking right
                            }
                            
                            // Normal flow within chunk
                            if (m_grid[y * WIDTH + nx] == MaterialType::Empty) {
                                // Make sure there's no hole under this position
                                if (y+1 >= HEIGHT) {
                                    if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        m_grid[y * WIDTH + nx] = material;
                                        flowed = true;
                                        break;
                                    }
                                } else if (m_grid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else {
                                // Hit an obstacle, stop looking in this direction
                                break;
                            }
                        }
                    }
                } else {
                    // Try to flow right first
                    for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                        int nx = x + d;
                        
                        // If we hit the right edge, check chunk to the right
                        if (nx >= WIDTH) {
                            if (chunkRight) {
                                int rightX = nx - WIDTH; // Convert to position in right chunk
                                if (chunkRight->get(rightX, y) == MaterialType::Empty) {
                                    // Make sure there's no hole underneath in right chunk
                                    if (y+1 >= HEIGHT || 
                                        chunkRight->get(rightX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        chunkRight->set(rightX, y, material);
                                        chunkRight->setDirty(true);
                                        flowed = true;
                                        break;
                                    }
                                }
                            }
                            break; // Stop looking right
                        }
                        
                        // Normal flow within chunk
                        if (m_grid[y * WIDTH + nx] == MaterialType::Empty) {
                            // Make sure there's no hole under this position
                            if (y+1 >= HEIGHT) {
                                if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else if (m_grid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                m_grid[idx] = MaterialType::Empty;
                                m_grid[y * WIDTH + nx] = material;
                                flowed = true;
                                break;
                            }
                        } else {
                            // Hit an obstacle, stop looking in this direction
                            break;
                        }
                    }
                    
                    // Try to flow left if didn't flow right
                    if (!flowed) {
                        for (int d = 1; d <= MAX_FLOW_DISTANCE && !flowed; d++) {
                            int nx = x - d;
                            
                            // If we hit the left edge, check chunk to the left
                            if (nx < 0) {
                                if (chunkLeft) {
                                    int leftX = WIDTH + nx; // Convert negative nx to position in left chunk
                                    if (chunkLeft->get(leftX, y) == MaterialType::Empty) {
                                        // Make sure there's no hole underneath in left chunk
                                        if (y+1 >= HEIGHT || 
                                            chunkLeft->get(leftX, (y+1 >= HEIGHT) ? 0 : y+1) != MaterialType::Empty) {
                                            m_grid[idx] = MaterialType::Empty;
                                            chunkLeft->set(leftX, y, material);
                                            chunkLeft->setDirty(true);
                                            flowed = true;
                                            break;
                                        }
                                    }
                                }
                                break; // Stop looking left
                            }
                            
                            // Normal flow within chunk
                            if (m_grid[y * WIDTH + nx] == MaterialType::Empty) {
                                // Make sure there's no hole under this position
                                if (y+1 >= HEIGHT) {
                                    if (!chunkBelow || chunkBelow->get(nx, 0) != MaterialType::Empty) {
                                        m_grid[idx] = MaterialType::Empty;
                                        m_grid[y * WIDTH + nx] = material;
                                        flowed = true;
                                        break;
                                    }
                                } else if (m_grid[(y+1) * WIDTH + nx] != MaterialType::Empty) {
                                    m_grid[idx] = MaterialType::Empty;
                                    m_grid[y * WIDTH + nx] = material;
                                    flowed = true;
                                    break;
                                }
                            } else {
                                // Hit an obstacle, stop looking in this direction
                                break;
                            }
                        }
                    }
                }
                
                if (flowed) {
                    continue;
                }
            }
            
            // Gases (rise up) - Improved gas physics (second pass)
            else if (props.isGas) {
                // Check if material is enclosed (trapped on all sides)
                bool isEnclosed = true;
                
                // Check surrounding cells
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        // Skip self
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // For boundary cells, check neighboring chunks
                        MaterialType neighborMaterial = MaterialType::Empty;
                        
                        if (nx < 0) {
                            // Left chunk boundary
                            if (chunkLeft) {
                                neighborMaterial = chunkLeft->get(WIDTH-1, ny);
                            }
                        } 
                        else if (nx >= WIDTH) {
                            // Right chunk boundary
                            if (chunkRight) {
                                neighborMaterial = chunkRight->get(0, ny);
                            }
                        }
                        else if (ny >= HEIGHT) {
                            // Bottom chunk boundary
                            if (chunkBelow) {
                                neighborMaterial = chunkBelow->get(nx, 0);
                            }
                        }
                        else if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                            // Within this chunk
                            neighborMaterial = oldGrid[ny * WIDTH + nx];
                        }
                        
                        // If there's an empty space, the material is not enclosed
                        if (neighborMaterial == MaterialType::Empty) {
                            isEnclosed = false;
                            break;
                        }
                    }
                    if (!isEnclosed) break;
                }
                
                // If the material is enclosed, treat it as a solid (skip physics)
                if (isEnclosed) {
                    continue;
                }
                
                // Different behavior for different gases
                bool isSteam = (material == MaterialType::Steam);
                bool isSmoke = (material == MaterialType::Smoke);
                
                // Gases can sometimes dissipate (especially steam)
                int dissipationChance = isSteam ? 5 : (isSmoke ? 10 : 50);
                if (static_cast<int>(m_rng() % 100) < dissipationChance) {
                    // The gas dissipates
                    m_grid[idx] = MaterialType::Empty;
                    continue;
                }
                
                // Gases try to rise quickly for steam, medium for smoke, slower for others
                int riseChance = isSteam ? 95 : (isSmoke ? 90 : 80);
                
                // Try to rise upward with appropriate speed
                if (y > 0 && static_cast<int>(m_rng() % 100) < riseChance) {
                    // Try to rise directly upward
                    if (m_grid[(y-1) * WIDTH + x] == MaterialType::Empty) {
                        m_grid[idx] = MaterialType::Empty;
                        m_grid[(y-1) * WIDTH + x] = material;
                        continue;
                    }
                    
                    // If upward is blocked, check diagonally up
                    bool diagonalMoved = false;
                    
                    // Deterministic pattern but seems random
                    if ((x + y) % 2 == 0) {
                        // Try up-left first
                        if (x > 0 && m_grid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            diagonalMoved = true;
                        }
                        // Then up-right
                        else if (x+1 < WIDTH && m_grid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
                            diagonalMoved = true;
                        }
                    } else {
                        // Try up-right first
                        if (x+1 < WIDTH && m_grid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
                            diagonalMoved = true;
                        }
                        // Then up-left
                        else if (x > 0 && m_grid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            diagonalMoved = true;
                        }
                    }
                    
                    if (diagonalMoved) {
                        continue;
                    }
                }
                
                // If rise failed, try horizontal spread (gases like to fill available space)
                // Chance of spreading is different for each gas
                int spreadChance = isSteam ? 40 : (isSmoke ? 60 : 50);
                
                if (static_cast<int>(m_rng() % 100) < spreadChance) {
                    bool spreadLeft = false;
                    bool spreadRight = false;
                    
                    // Deterministic direction choice to avoid bias
                    if ((x + y) % 2 == 0) {
                        // Try left
                        if (x > 0 && m_grid[y * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x-1)] = material;
                            spreadLeft = true;
                        }
                        // Try right
                        else if (x+1 < WIDTH && m_grid[y * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x+1)] = material;
                            spreadRight = true;
                        }
                    } else {
                        // Try right
                        if (x+1 < WIDTH && m_grid[y * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x+1)] = material;
                            spreadRight = true;
                        }
                        // Try left
                        else if (x > 0 && m_grid[y * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[y * WIDTH + (x-1)] = material;
                            spreadLeft = true;
                        }
                    }
                    
                    if (spreadLeft || spreadRight) {
                        continue;
                    }
                }
                
                // Special case: if steam touches water, convert to water
                if (isSteam) {
                    // Check all surrounding cells for water
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                                if (m_grid[ny * WIDTH + nx] == MaterialType::Water) {
                                    // Contact with water causes steam to condensate
                                    if (static_cast<int>(m_rng() % 100) < 30) { // 30% chance to condensate
                                        m_grid[idx] = MaterialType::Empty;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                // Special case: smoke near fire
                else if (isSmoke) {
                    // Check if near fire - if so, smoke rises faster
                    bool nearFire = false;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                                if (m_grid[ny * WIDTH + nx] == MaterialType::Fire) {
                                    nearFire = true;
                                    break;
                                }
                            }
                        }
                        if (nearFire) break;
                    }
                    
                    // Smoke near fire has a chance to move upward even if no empty space
                    if (nearFire && y > 0 && static_cast<int>(m_rng() % 100) < 30) {
                        // Enhanced rising due to heat - can displace air
                        MaterialType aboveMaterial = m_grid[(y-1) * WIDTH + x];
                        if (aboveMaterial == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + x] = material;
                            continue;
                        }
                    }
                }
            }
        }
    }
    
    // Update the pixel data for rendering
    updatePixelData();
    
    // Track if any materials actually moved during this update
    bool anyMovement = false;
    
    // We'll iterate over the grid to see if any cell is still active
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            MaterialType material = m_grid[y * WIDTH + x];
            
            // Skip empty cells and static solids
            if (material == MaterialType::Empty || 
                (MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isSolid &&
                 !MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isFlammable)) {
                continue;
            }
            
            // Check if this material can still move (e.g., sand with empty space below)
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Optimization: Check if this is a trapped particle that can't move anymore
            // This avoids unnecessary checks in future updates
            bool trapped = false;
            
            if (props.isPowder || props.isLiquid) {
                // Check if powder/liquid is trapped (solid on all sides)
                if (y+1 >= HEIGHT || m_grid[(y+1) * WIDTH + x] != MaterialType::Empty) {
                    // Can't fall straight down, check if it's fully trapped
                    bool blockedLeft = (x <= 0 || m_grid[y * WIDTH + (x-1)] != MaterialType::Empty);
                    bool blockedRight = (x+1 >= WIDTH || m_grid[y * WIDTH + (x+1)] != MaterialType::Empty);
                    bool blockedDownLeft = (x <= 0 || y+1 >= HEIGHT || m_grid[(y+1) * WIDTH + (x-1)] != MaterialType::Empty);
                    bool blockedDownRight = (x+1 >= WIDTH || y+1 >= HEIGHT || m_grid[(y+1) * WIDTH + (x+1)] != MaterialType::Empty);
                    
                    if (props.isPowder) {
                        // Powders only move down and diagonally down, so if blocked in these directions, it's trapped
                        trapped = blockedDownLeft && blockedDownRight;
                    } else {
                        // Liquids can move horizontally too
                        trapped = blockedLeft && blockedRight && blockedDownLeft && blockedDownRight;
                    }
                }
                
                // If not trapped, check if it can actually move
                if (!trapped) {
                    // Check below
                    if (y+1 < HEIGHT && m_grid[(y+1) * WIDTH + x] == MaterialType::Empty) {
                        anyMovement = true;
                        break;
                    }
                    
                    // Check diagonal below
                    if (y+1 < HEIGHT) {
                        if ((x > 0 && m_grid[(y+1) * WIDTH + (x-1)] == MaterialType::Empty) ||
                            (x+1 < WIDTH && m_grid[(y+1) * WIDTH + (x+1)] == MaterialType::Empty)) {
                            anyMovement = true;
                            break;
                        }
                    }
                    
                    // For liquids, check horizontal flow
                    if (props.isLiquid) {
                        if ((x > 0 && m_grid[y * WIDTH + (x-1)] == MaterialType::Empty) ||
                            (x+1 < WIDTH && m_grid[y * WIDTH + (x+1)] == MaterialType::Empty)) {
                            anyMovement = true;
                            break;
                        }
                    }
                }
            }
            else if (props.isGas) {
                // Check if gas is trapped (solid on all sides)
                if (y <= 0 || m_grid[(y-1) * WIDTH + x] != MaterialType::Empty) {
                    // Can't rise straight up, check if it's fully trapped
                    bool blockedLeft = (x <= 0 || m_grid[y * WIDTH + (x-1)] != MaterialType::Empty);
                    bool blockedRight = (x+1 >= WIDTH || m_grid[y * WIDTH + (x+1)] != MaterialType::Empty);
                    bool blockedUpLeft = (x <= 0 || y <= 0 || m_grid[(y-1) * WIDTH + (x-1)] != MaterialType::Empty);
                    bool blockedUpRight = (x+1 >= WIDTH || y <= 0 || m_grid[(y-1) * WIDTH + (x+1)] != MaterialType::Empty);
                    
                    trapped = blockedLeft && blockedRight && blockedUpLeft && blockedUpRight;
                }
                
                // If not trapped, check if it can actually move
                if (!trapped) {
                    // Check above for gases
                    if (y > 0 && m_grid[(y-1) * WIDTH + x] == MaterialType::Empty) {
                        anyMovement = true;
                        break;
                    }
                    
                    // Check horizontal for gases
                    if ((x > 0 && m_grid[y * WIDTH + (x-1)] == MaterialType::Empty) ||
                        (x+1 < WIDTH && m_grid[y * WIDTH + (x+1)] == MaterialType::Empty)) {
                        anyMovement = true;
                        break;
                    }
                }
            }
        }
        if (anyMovement) break;
    }
    
    // If nothing moved, mark the chunk as not dirty
    m_isDirty = anyMovement;
}

void Chunk::handleMaterialInteractions(const std::vector<MaterialType>& oldGrid) {
    // Distribution for random checks
    std::uniform_int_distribution<int> chanceDist(0, 99);
    
    // Iterate through the grid to find interactions
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            MaterialType material = oldGrid[idx];
            
            // Skip empty cells
            if (material == MaterialType::Empty) {
                continue;
            }
            
            // Fire interactions
            if (material == MaterialType::Fire) {
                // 1. Fire has a chance to go out
                if (chanceDist(m_rng) < 5) {  // 5% chance to go out
                    // Leave charred material or smoke
                    if (chanceDist(m_rng) < 30) {
                        m_grid[idx] = MaterialType::Smoke;
                    } else {
                        m_grid[idx] = MaterialType::Empty;
                    }
                    continue;
                }
                
                // 2. Fire can spread to nearby flammable materials
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        // Skip self
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Check bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) {
                            continue;
                        }
                        
                        int nIdx = ny * WIDTH + nx;
                        MaterialType nearbyMaterial = oldGrid[nIdx];
                        
                        // Check if target material is flammable
                        if (nearbyMaterial != MaterialType::Empty) {
                            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(nearbyMaterial)];
                            
                            if (props.isFlammable) {
                                // Different ignition chances for different materials
                                int ignitionChance = 0;
                                
                                if (nearbyMaterial == MaterialType::Wood) {
                                    ignitionChance = 15; // 15% chance to ignite wood
                                } else if (nearbyMaterial == MaterialType::Oil) {
                                    ignitionChance = 40; // 40% chance to ignite oil
                                } else if (nearbyMaterial == MaterialType::Grass) {
                                    ignitionChance = 25; // 25% chance to ignite grass
                                }
                                
                                // Roll for ignition
                                if (chanceDist(m_rng) < ignitionChance) {
                                    m_grid[nIdx] = MaterialType::Fire;
                                }
                            }
                            
                            // Special case: fire creates steam when touching water
                            if (nearbyMaterial == MaterialType::Water && chanceDist(m_rng) < 20) {
                                m_grid[nIdx] = MaterialType::Steam;
                            }
                        }
                    }
                }
            }
            
            // Oil + Fire interactions (already handled in fire spreading)
            
            // Water + Fire interactions
            if (material == MaterialType::Water) {
                // Water can extinguish nearby fire
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        // Skip self
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        // Check bounds
                        if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) {
                            continue;
                        }
                        
                        int nIdx = ny * WIDTH + nx;
                        MaterialType nearbyMaterial = oldGrid[nIdx];
                        
                        // Water extinguishes fire with high probability
                        if (nearbyMaterial == MaterialType::Fire) {
                            if (chanceDist(m_rng) < 70) {  // 70% chance to extinguish
                                if (chanceDist(m_rng) < 40) {  // 40% chance to create steam
                                    m_grid[nIdx] = MaterialType::Steam;
                                } else {
                                    m_grid[nIdx] = MaterialType::Empty;
                                }
                            }
                        }
                    }
                }
            }
            
            // Smoke + Water interactions
            if (material == MaterialType::Smoke) {
                // Check for water nearby - smoke dissipates faster near water
                bool nearWater = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                            if (oldGrid[ny * WIDTH + nx] == MaterialType::Water) {
                                nearWater = true;
                                break;
                            }
                        }
                    }
                    if (nearWater) break;
                }
                
                // Smoke dissipates faster near water
                if (nearWater && chanceDist(m_rng) < 20) {
                    m_grid[idx] = MaterialType::Empty;
                }
            }
            
            // Fix oil disappearing bug - reduce evaporation chances
            if (material == MaterialType::Oil) {
                // Make sure oil doesn't disappear randomly
                // Check if this oil is part of a larger body to make it more stable
                bool hasNeighboringOil = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue; // Skip self
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                            if (oldGrid[ny * WIDTH + nx] == MaterialType::Oil) {
                                hasNeighboringOil = true;
                                break;
                            }
                        }
                    }
                    if (hasNeighboringOil) break;
                }
                
                // Make isolated oil droplets more stable
                if (!hasNeighboringOil) {
                    // Create a stable film of oil (only happens in rare cases)
                    bool hasGroundBelow = false;
                    if (y + 1 < HEIGHT) {
                        MaterialType belowMaterial = oldGrid[(y+1) * WIDTH + x];
                        if (belowMaterial != MaterialType::Empty && 
                            belowMaterial != MaterialType::Water &&
                            belowMaterial != MaterialType::Oil) {
                            hasGroundBelow = true;
                        }
                    }
                    
                    // If we have ground below, make it more sticky (stable)
                    if (hasGroundBelow) {
                        // Oil remains in place - do nothing special
                    }
                }
            }
        }
    }
}

void Chunk::updatePixelData() {
    // Setup random number generator for color variations
    static std::mt19937 colorGen(std::random_device{}());
    static std::uniform_int_distribution<int> colorVar(-20, 20);
    static std::uniform_real_distribution<float> fireIntensity(0.7f, 1.3f);
    
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            MaterialType material = m_grid[y * WIDTH + x];
            int idx = (y * WIDTH + x) * 4;
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Apply color variation based on material type
            if (material == MaterialType::Empty) {
                // Empty space is transparent
                m_pixelData[idx] = 0;
                m_pixelData[idx+1] = 0;
                m_pixelData[idx+2] = 0;
                m_pixelData[idx+3] = 0;
            }
            else if (material == MaterialType::Fire) {
                // Fire has more dramatic color variation
                float intensity = fireIntensity(colorGen);
                m_pixelData[idx] = std::min(255, static_cast<int>(props.r * intensity));
                m_pixelData[idx+1] = std::min(255, static_cast<int>(props.g * (intensity * 0.8f)));
                m_pixelData[idx+2] = std::min(255, static_cast<int>(props.b * (intensity * 0.6f)));
                m_pixelData[idx+3] = props.transparency;
            }
            else if (material == MaterialType::Smoke || material == MaterialType::Steam) {
                // Smoke and steam have variable opacity
                int alpha = 120 + colorVar(colorGen) % 40;
                m_pixelData[idx] = props.r;
                m_pixelData[idx+1] = props.g;
                m_pixelData[idx+2] = props.b;
                m_pixelData[idx+3] = std::max(80, std::min(200, alpha));
            }
            else if (material == MaterialType::Grass) {
                // Create extremely detailed Noita-like grass with individual pixel-perfect blades
                
                // Position-based deterministic pattern for grass blade shapes
                // Use absolute position for consistent patterns
                int worldX = x; // Just use absolute coordinates
                int worldY = y;
                
                // Create various grass blade patterns
                bool tallBlade = (worldX % 5 == 0 && worldY % 4 == 0);  // Tall sparse blades
                bool mediumBlade = ((worldX + 2) % 5 == 0 && worldY % 3 == 0); // Medium blades
                bool shortBlade = ((worldX + 1) % 3 == 0 && worldY % 2 == 0);  // Common short blades
                bool tinyBlade = ((worldX * worldY) % 7 == 0);  // Tiny grass details
                
                // Check neighbors to ensure grass blades are connected to ground
                bool isTopOfGrass = false;
                if (y < HEIGHT-1) {
                    MaterialType below = m_grid[(y+1) * WIDTH + x];
                    if (below == MaterialType::Grass || below == MaterialType::TopSoil || below == MaterialType::Dirt) {
                        isTopOfGrass = true;
                    }
                }
                
                int rVar, gVar, bVar;
                
                if (tallBlade && isTopOfGrass) {
                    // Tall grass blade tips (darker green)
                    rVar = -20;
                    gVar = 20; // Very vibrant green
                    bVar = -15;
                } else if (mediumBlade && isTopOfGrass) {
                    // Medium grass blades (medium green)
                    rVar = -15;
                    gVar = 10;
                    bVar = -10;
                } else if (shortBlade) {
                    // Short grass blades (lighter green)
                    rVar = -10;
                    gVar = 5;
                    bVar = -5;
                } else if (tinyBlade) {
                    // Tiny details (yellowish-green)
                    rVar = 5;
                    gVar = 5;
                    bVar = -15;
                } else {
                    // Base grass variation with position-based consistency
                    int grassHash = (worldX * 41 + worldY * 23) % 20;
                    rVar = (grassHash % 10) - 5;
                    gVar = (grassHash % 12) - 2; // Bias toward more green
                    bVar = (grassHash % 8) - 4;
                }
                
                m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + rVar));
                m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + gVar));
                m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + bVar));
                m_pixelData[idx+3] = props.transparency;
            }
            else if (material == MaterialType::Stone || material == MaterialType::Gravel) {
                // Stone and gravel - create pixel-perfect pattern like Noita
                // Hash based on position to create consistent patterns
                int posHash = (x * 263 + y * 71) % 100;
                
                // Get pattern variation based on position
                bool patternDot = ((x + y) % 3 == 0);
                bool patternStripe = (x % 4 == 0 || y % 4 == 0);
                
                // Apply more distinct variations to create noticeable patterns
                int redVar, greenVar, blueVar;
                
                if (patternDot) {
                    // Create darker spots
                    redVar = -15;
                    greenVar = -15;
                    blueVar = -15;
                } else if (patternStripe) {
                    // Create light stripes
                    redVar = 10;
                    greenVar = 10;
                    blueVar = 10;
                } else {
                    // Normal subtle variation
                    redVar = (posHash % 15) - 7;
                    greenVar = ((posHash * 3) % 15) - 7;
                    blueVar = ((posHash * 7) % 15) - 7;
                }
                
                m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + redVar));
                m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + greenVar));
                m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + blueVar));
                m_pixelData[idx+3] = props.transparency;
            }
            else if (material == MaterialType::Dirt || material == MaterialType::TopSoil) {
                // Dirt and topsoil - create more Noita-like granular texture
                int posHash = (x * 131 + y * 37) % 100;
                
                // Create granular patterns
                bool isDarkGrain = ((x*x + y*y) % 5 == 0);
                bool isLightGrain = ((x+y) % 7 == 0);
                
                int rVar, gVar, bVar;
                
                if (isDarkGrain) {
                    // Dark grains scattered throughout
                    rVar = -20;
                    gVar = -15;
                    bVar = -10;
                } else if (isLightGrain) {
                    // Light sandy specks
                    rVar = 15;
                    gVar = 10;
                    bVar = 5;
                } else {
                    // Normal dirt variation
                    rVar = (posHash % 20) - 10;
                    gVar = (posHash % 15) - 7;
                    bVar = (posHash % 10) - 5;
                }
                
                m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + rVar));
                m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + gVar));
                m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + bVar));
                m_pixelData[idx+3] = props.transparency;
            }
            else if (material == MaterialType::Water || material == MaterialType::Oil) {
                // Check if liquid is settled (surrounded by other materials)
                bool isSettled = true;
                
                // Check below and sides to determine if settled
                if (y+1 < HEIGHT && m_grid[(y+1) * WIDTH + x] == MaterialType::Empty)
                    isSettled = false;
                if (x > 0 && m_grid[y * WIDTH + (x-1)] == MaterialType::Empty)
                    isSettled = false;
                if (x+1 < WIDTH && m_grid[y * WIDTH + (x+1)] == MaterialType::Empty)
                    isSettled = false;
                
                if (isSettled) {
                    // Fixed color for settled liquid - no variation
                    m_pixelData[idx] = props.r;
                    m_pixelData[idx+1] = props.g;
                    m_pixelData[idx+2] = props.b;
                    m_pixelData[idx+3] = props.transparency;
                } else {
                    // Moving liquid has color variations
                    int waterVar = colorVar(colorGen) / 3;
                    m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + waterVar / 2));
                    m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + waterVar));
                    m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + waterVar * 2));
                    m_pixelData[idx+3] = props.transparency;
                }
            }
            else {
                // Default material with some variation
                int var = colorVar(colorGen) / 2;
                m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + var));
                m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + var));
                m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + var));
                m_pixelData[idx+3] = props.transparency;
            }
        }
    }
}

// World implementation

World::World(int width, int height) : 
    m_width(width), 
    m_height(height),
    m_chunksX(std::ceil(static_cast<float>(width) / Chunk::WIDTH)),
    m_chunksY(std::ceil(static_cast<float>(height) / Chunk::HEIGHT))
{
    // Create the world's chunks with their proper positions for pixel-perfect alignment
    m_chunks.resize(m_chunksX * m_chunksY);
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            int idx = y * m_chunksX + x;
            int chunkPosX = x * Chunk::WIDTH;
            int chunkPosY = y * Chunk::HEIGHT;
            m_chunks[idx] = std::make_unique<Chunk>(chunkPosX, chunkPosY);
        }
    }
    
    // Initialize pixel data for the whole world
    m_pixelData.resize(width * height * 4, 0);
    
    std::cout << "Created world with dimensions: " << width << "x" << height << std::endl;
    std::cout << "Chunk grid size: " << m_chunksX << "x" << m_chunksY << std::endl;
}

MaterialType World::get(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return MaterialType::Empty;
    }
    
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    const Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return MaterialType::Empty;
    }
    
    return chunk->get(localX, localY);
}

void World::set(int x, int y, MaterialType material) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(x, y, chunkX, chunkY, localX, localY);
    
    Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (!chunk) {
        return;
    }
    
    chunk->set(localX, localY, material);
    
    // Update pixel data for this position
    int idx = (y * m_width + x) * 4;
    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
    m_pixelData[idx] = props.r;     // R
    m_pixelData[idx+1] = props.g;   // G
    m_pixelData[idx+2] = props.b;   // B
    m_pixelData[idx+3] = props.transparency; // Use material's transparency value
}

// Helper function to ensure liquids settle properly with flat surface
void World::levelLiquids() {
    // This function will look for liquid pixels and enforce proper settling
    for (int y = m_height - 2; y >= 0; y--) {  // Start from bottom-1, going up
        for (int x = 0; x < m_width; x++) {
            MaterialType material = get(x, y);
            
            // Only process liquids
            if (!MATERIAL_PROPERTIES[static_cast<std::size_t>(material)].isLiquid) {
                continue;
            }
            
            // Check if there's an empty space below
            if (y < m_height - 1 && get(x, y + 1) == MaterialType::Empty) {
                // Make it fall one tile down
                set(x, y, MaterialType::Empty);
                set(x, y + 1, material);
            } 
            // If can't fall, check for uneven surface and equalize it
            else {
                bool moved = false;
                
                // Check the nearby 5 cells on each side for even distribution
                for (int step = 1; step <= 5 && !moved; step++) {
                    // Check if we have a valid column to the left
                    if (x - step >= 0) {
                        // If column to the left has space and current column has higher liquid level
                        int leftLiquidHeight = 0;
                        int curLiquidHeight = 0;
                        
                        // Count down from current position to find liquid heights
                        for (int checkY = y; checkY < m_height; checkY++) {
                            if (MATERIAL_PROPERTIES[static_cast<std::size_t>(get(x, checkY))].isLiquid) {
                                curLiquidHeight++;
                            } else {
                                break;
                            }
                        }
                        
                        for (int checkY = y; checkY < m_height; checkY++) {
                            if (MATERIAL_PROPERTIES[static_cast<std::size_t>(get(x - step, checkY))].isLiquid) {
                                leftLiquidHeight++;
                            } else {
                                break;
                            }
                        }
                        
                        // If this column has more liquid than left column, move a cell to equalize
                        if (curLiquidHeight > leftLiquidHeight + 1 && get(x - step, y) == MaterialType::Empty) {
                            set(x, y, MaterialType::Empty);
                            set(x - step, y, material);
                            moved = true;
                            break;
                        }
                    }
                    
                    // Check if we have a valid column to the right
                    if (x + step < m_width && !moved) {
                        // If column to the right has space and current column has higher liquid level
                        int rightLiquidHeight = 0;
                        int curLiquidHeight = 0;
                        
                        // Count down from current position to find liquid heights
                        for (int checkY = y; checkY < m_height; checkY++) {
                            if (MATERIAL_PROPERTIES[static_cast<std::size_t>(get(x, checkY))].isLiquid) {
                                curLiquidHeight++;
                            } else {
                                break;
                            }
                        }
                        
                        for (int checkY = y; checkY < m_height; checkY++) {
                            if (MATERIAL_PROPERTIES[static_cast<std::size_t>(get(x + step, checkY))].isLiquid) {
                                rightLiquidHeight++;
                            } else {
                                break;
                            }
                        }
                        
                        // If this column has more liquid than right column, move a cell to equalize
                        if (curLiquidHeight > rightLiquidHeight + 1 && get(x + step, y) == MaterialType::Empty) {
                            set(x, y, MaterialType::Empty);
                            set(x + step, y, material);
                            moved = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void World::update() {
    // Create a list of chunks to update (dirty chunks and their neighbors)
    std::vector<std::pair<int, int>> chunksToUpdate;
    
    // First pass: identify all dirty chunks
    for (int y = 0; y < m_chunksY; ++y) {
        for (int x = 0; x < m_chunksX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (chunk && chunk->isDirty()) {
                // Add this chunk
                chunksToUpdate.emplace_back(x, y);
                
                // Also add neighboring chunks since they might be affected
                if (y > 0) chunksToUpdate.emplace_back(x, y-1);  // Above
                if (y < m_chunksY - 1) chunksToUpdate.emplace_back(x, y+1);  // Below
                if (x > 0) chunksToUpdate.emplace_back(x-1, y);  // Left
                if (x < m_chunksX - 1) chunksToUpdate.emplace_back(x+1, y);  // Right
            }
        }
    }
    
    // Remove duplicates from the update list
    std::sort(chunksToUpdate.begin(), chunksToUpdate.end());
    chunksToUpdate.erase(std::unique(chunksToUpdate.begin(), chunksToUpdate.end()), chunksToUpdate.end());
    
    // Second pass: update only the necessary chunks
    for (const auto& [x, y] : chunksToUpdate) {
        Chunk* chunk = getChunkAt(x, y);
        if (chunk) {
            // Get neighboring chunks
            Chunk* chunkBelow = (y < m_chunksY - 1) ? getChunkAt(x, y + 1) : nullptr;
            Chunk* chunkLeft = (x > 0) ? getChunkAt(x - 1, y) : nullptr;
            Chunk* chunkRight = (x < m_chunksX - 1) ? getChunkAt(x + 1, y) : nullptr;
            
            // Update with neighbors for boundary physics
            chunk->update(chunkBelow, chunkLeft, chunkRight);
        }
    }
    
    // Level liquids for smoother fluid surfaces - do this after chunks are updated
    // This prevents wave effects and ensures liquids form flat surfaces
    levelLiquids();
    
    // Only update pixel data if any chunks were updated
    if (!chunksToUpdate.empty()) {
        updatePixelData();
    }
}

// Improved liquid flow algorithm
bool Chunk::isNotIsolatedLiquid(const std::vector<MaterialType>& grid, int x, int y) {
    // Count how many liquid cells are connected to this one
    int liquidCount = 0;
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            
            MaterialType cellMaterial = grid[ny * WIDTH + nx];
            if (MATERIAL_PROPERTIES[static_cast<std::size_t>(cellMaterial)].isLiquid) {
                liquidCount++;
            }
        }
    }
    
    return liquidCount > 0;
}

void World::updatePlayer(float dt) {
    // Constants for player physics
    const float GRAVITY = 400.0f;    // Gravity force
    // Warning fix: these are now set directly in respective functions
    // const float PLAYER_SPEED = 200.0f; // Moved to movePlayerLeft/movePlayerRight
    // const float JUMP_FORCE = 350.0f;  // Moved to playerJump
    const float FRICTION = 0.8f;      // Horizontal friction
    
    // Apply gravity
    m_playerVelY += GRAVITY * dt;
    
    // Limit fall speed
    if (m_playerVelY > 500.0f) {
        m_playerVelY = 500.0f;
    }
    
    // Update position based on velocity
    float newX = m_playerX + m_playerVelX * dt;
    float newY = m_playerY + m_playerVelY * dt;
    
    // Check for collision with world
    int playerWidth = 10;   // Player is 10x20 pixels
    int playerHeight = 20;
    
    // Check if we're on ground
    m_playerOnGround = false;
    for (int x = -playerWidth/2; x <= playerWidth/2; x++) {
        int checkX = static_cast<int>(newX) + x;
        int checkY = static_cast<int>(newY) + playerHeight/2 + 1;
        
        if (checkX >= 0 && checkX < m_width && checkY >= 0 && checkY < m_height) {
            MaterialType material = get(checkX, checkY);
            if (material != MaterialType::Empty && material != MaterialType::Grass) {
                // We hit ground - exclude grass so player can move through it
                newY = checkY - playerHeight/2 - 1;
                m_playerVelY = 0;
                m_playerOnGround = true;
                break;
            }
        }
    }
    
    // Check for head collision
    for (int x = -playerWidth/2; x <= playerWidth/2; x++) {
        int checkX = static_cast<int>(newX) + x;
        int checkY = static_cast<int>(newY) - playerHeight/2 - 1;
        
        if (checkX >= 0 && checkX < m_width && checkY >= 0 && checkY < m_height) {
            MaterialType material = get(checkX, checkY);
            if (material != MaterialType::Empty && material != MaterialType::Grass) {
                // We hit ceiling - exclude grass so player can move through it
                newY = checkY + playerHeight/2 + 2;
                m_playerVelY = 0;
                break;
            }
        }
    }
    
    // Check for horizontal collision
    for (int y = -playerHeight/2; y <= playerHeight/2; y++) {
        // Check right side
        int checkX = static_cast<int>(newX) + playerWidth/2 + 1;
        int checkY = static_cast<int>(newY) + y;
        
        if (checkX >= 0 && checkX < m_width && checkY >= 0 && checkY < m_height) {
            MaterialType material = get(checkX, checkY);
            if (material != MaterialType::Empty && material != MaterialType::Grass) {
                // We hit right wall - exclude grass so player can move through it
                newX = checkX - playerWidth/2 - 1;
                m_playerVelX = 0;
                break;
            }
        }
        
        // Check left side
        checkX = static_cast<int>(newX) - playerWidth/2 - 1;
        if (checkX >= 0 && checkX < m_width && checkY >= 0 && checkY < m_height) {
            MaterialType material = get(checkX, checkY);
            if (material != MaterialType::Empty && material != MaterialType::Grass) {
                // We hit left wall - exclude grass so player can move through it
                newX = checkX + playerWidth/2 + 2;
                m_playerVelX = 0;
                break;
            }
        }
    }
    
    // Apply friction when on ground
    if (m_playerOnGround) {
        m_playerVelX *= FRICTION;
    }
    
    // Clamp position to world bounds
    newX = std::max(playerWidth/2.0f, std::min(newX, m_width - playerWidth/2.0f));
    newY = std::max(playerHeight/2.0f, std::min(newY, m_height - playerHeight/2.0f));
    
    // Update position
    m_playerX = static_cast<int>(newX);
    m_playerY = static_cast<int>(newY);
    
    // Make surrounding chunks dirty for player area
    int chunkX, chunkY, localX, localY;
    worldToChunkCoords(m_playerX, m_playerY, chunkX, chunkY, localX, localY);
    
    // Mark player's chunk as dirty
    Chunk* chunk = getChunkAt(chunkX, chunkY);
    if (chunk) {
        chunk->setDirty(true);
    }
}

void World::generate(unsigned int seed) {
    // Set up the random number generator with the seed
    m_rng.seed(seed);
    
    // Clear the world
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            set(x, y, MaterialType::Empty);
        }
    }
    
    // Reset player position to above the terrain near the top middle of the world
    m_playerX = m_width / 2;
    m_playerY = 50;  // Start higher since the ground is now near the top 1/6th of the world
    m_playerVelX = 0.0f;
    m_playerVelY = 0.0f;
    
    // Generate terrain using perlin-like noise for smoother results
    // We'll use a simple approximation of noise here
    generateTerrain();
    
    // Add caves
    generateCaves();
    
    // Add water pools
    generateWaterPools();
    
    // Add some underground pockets of materials
    generateMaterialDeposits();
    
    // All chunks are dirty after generation
    for (auto& chunk : m_chunks) {
        if (chunk) {
            chunk->setDirty(true);
        }
    }
    
    // Update pixel data
    updatePixelData();
}

void World::generateTerrain() {
    // Make ground much higher in the world to create room for deep cave networks
    // Only use top 1/6th of the screen for the ground surface, leaving 5/6ths for underground
    int baseGroundLevel = m_height / 6;
    
    // Generate smoother height variations for natural terrain
    std::vector<int> heightMap(m_width, 0);
    
    // Use lower frequencies with moderate amplitudes for gentle surface terrain
    // Large scale features (primary terrain shape)
    std::uniform_int_distribution<int> freqDist1(3, 5);  // Lower frequency for larger hills
    std::uniform_int_distribution<int> ampDist1(10, 20); // Moderate amplitude for rolling hills
    int freq1 = freqDist1(m_rng);
    int amp1 = ampDist1(m_rng);
    
    // Medium scale features (smaller hills)
    std::uniform_int_distribution<int> freqDist2(8, 12);
    std::uniform_int_distribution<int> ampDist2(4, 8);
    int freq2 = freqDist2(m_rng);
    int amp2 = ampDist2(m_rng);
    
    // Small scale features (tiny details)
    std::uniform_int_distribution<int> freqDist3(18, 25);
    std::uniform_int_distribution<int> ampDist3(1, 3);
    int freq3 = freqDist3(m_rng);
    int amp3 = ampDist3(m_rng);
    
    // Starting height offset for variation - prevent identical shape patterns
    std::uniform_real_distribution<double> phaseOffsetDist(0, 6.28318530718);
    double phaseOffset1 = phaseOffsetDist(m_rng);
    double phaseOffset2 = phaseOffsetDist(m_rng);
    double phaseOffset3 = phaseOffsetDist(m_rng);
    
    // Generate the height map by combining different frequencies
    for (int x = 0; x < m_width; ++x) {
        // Create gentle biome variations
        double biomeFactor = sin(static_cast<double>(x) / m_width * 1.2) * 0.3 + 0.7; // 0.4-1.0 range
        int biomeAdjustedAmp = static_cast<int>(amp1 * biomeFactor);
        
        // Primary terrain shape (smoother, gentler)
        double angle1 = static_cast<double>(x) / m_width * freq1 * 6.28318530718 + phaseOffset1;
        int height1 = static_cast<int>(sin(angle1) * biomeAdjustedAmp);
        
        // Medium details
        double angle2 = static_cast<double>(x) / m_width * freq2 * 6.28318530718 + phaseOffset2;
        int height2 = static_cast<int>(sin(angle2) * amp2);
        
        // Small details
        double angle3 = static_cast<double>(x) / m_width * freq3 * 6.28318530718 + phaseOffset3;
        int height3 = static_cast<int>(sin(angle3) * amp3);
        
        // Combined terrain height - keep near the top of the world
        heightMap[x] = baseGroundLevel + height1 + height2 + height3;
    }
    
    // Apply moderate smoothing to keep some variation but remove sharp edges
    for (int i = 0; i < 3; ++i) {
        std::vector<int> smoothedMap = heightMap;
        for (int x = 2; x < m_width - 2; ++x) {
            // Use a 5-cell window for smoothing
            smoothedMap[x] = (heightMap[x-2] + heightMap[x-1] + heightMap[x] * 2 + heightMap[x+1] + heightMap[x+2]) / 6;
        }
        heightMap = smoothedMap;
    }
    
    // Add plateaus - flat areas for interesting terrain features
    int numPlateaus = m_width / 250 + 2; // Add more plateaus 
    std::uniform_int_distribution<int> plateauPosDist(0, m_width - 1);
    std::uniform_int_distribution<int> plateauWidthDist(30, 80); // Wider plateaus
    
    for (int i = 0; i < numPlateaus; ++i) {
        int plateauCenter = plateauPosDist(m_rng);
        int plateauWidth = plateauWidthDist(m_rng);
        int startX = std::max(0, plateauCenter - plateauWidth/2);
        int endX = std::min(m_width - 1, plateauCenter + plateauWidth/2);
        
        // Set plateau height to average height in this region
        int plateauHeight = 0;
        for (int x = startX; x <= endX; ++x) {
            plateauHeight += heightMap[x];
        }
        plateauHeight /= (endX - startX + 1);
        
        // Occasionally lower the plateau a bit to create distinct areas
        if (m_rng() % 3 == 0) {
            plateauHeight += 5 + (m_rng() % 10);
        }
        
        // Apply plateau with smooth transitions at edges
        for (int x = startX; x <= endX; ++x) {
            double edgeFactor = 1.0;
            // Smooth the edges of the plateau
            if (x < startX + 12) {
                edgeFactor = (x - startX) / 12.0;
            } else if (x > endX - 12) {
                edgeFactor = (endX - x) / 12.0;
            }
            
            heightMap[x] = heightMap[x] * (1.0 - edgeFactor) + plateauHeight * edgeFactor;
        }
    }
    
    // Apply a final gentle smoothing pass
    for (int i = 0; i < 2; ++i) {
        std::vector<int> smoothedMap = heightMap;
        for (int x = 1; x < m_width - 1; ++x) {
            smoothedMap[x] = (heightMap[x-1] + heightMap[x] * 2 + heightMap[x+1]) / 4;
        }
        heightMap = smoothedMap;
    }
    
    // Create distributions for material depth variations
    std::uniform_int_distribution<int> grassVariation(0, 2);        // Thin grass layer
    std::uniform_int_distribution<int> topsoilVariation(2, 6);      // Thin topsoil layer
    std::uniform_int_distribution<int> dirtDepthDist(15, 35);       // Medium dirt layer
    std::uniform_int_distribution<int> stoneStartVariation(-8, 8);  // More varied stone transition
    
    // Create distributions for color variation
    std::uniform_int_distribution<int> colorVariation(-15, 15);
    
    // Layers - the world is now much deeper, with most of it being underground
    // Keep all the ground near the top 1/6th of the world
    for (int x = 0; x < m_width; ++x) {
        int groundLevel = heightMap[x];
        
        // For realistic ground layering with dynamic depths
        int grassDepth = grassVariation(m_rng);
        int topsoilDepth = topsoilVariation(m_rng);
        int dirtDepth = dirtDepthDist(m_rng);
        int stoneVariation = stoneStartVariation(m_rng);
        
        // Calculate layer positions
        int grassTop = groundLevel - grassDepth;
        int topsoilTop = groundLevel;
        int dirtTop = groundLevel + topsoilDepth;
        int stoneTop = groundLevel + topsoilDepth + dirtDepth + stoneVariation;
        
        // Fill in each layer with varying colors - fill entire height
        for (int y = 0; y < m_height; ++y) {
            if (y < 0 || y >= m_height) continue;
            
            // For color variation per pixel
            int rVar = colorVariation(m_rng);
            int gVar = colorVariation(m_rng);
            int bVar = colorVariation(m_rng);
            
            MaterialType material;
            
            // Determine material type based on depth
            if (y < grassTop) {
                // Above ground (air)
                continue; // Skip - already Empty
            } else if (y < topsoilTop) {
                // Grass layer
                material = MaterialType::Grass;
            } else if (y < dirtTop) {
                // Topsoil layer
                material = MaterialType::TopSoil;
            } else if (y < stoneTop) {
                // Dirt layer
                material = MaterialType::Dirt;
                
                // Create occasional gravel pockets in dirt
                int wx = x;
                int wy = y;
                
                // Use lower frequencies for larger patterns
                float primaryNoise = sin(wx * 0.008f) * sin(wy * 0.01f);
                float secondaryNoise = sin((wx * 0.015f + wy * 0.013f) * 0.8f);
                
                // Combine noise patterns for organic shapes
                float combinedNoise = primaryNoise * 0.6f + secondaryNoise * 0.4f;
                
                // Scale to 0-1 range
                combinedNoise = (combinedNoise + 1.0f) / 2.0f;
                
                // Smaller, less common gravel pockets
                if (combinedNoise > 0.72f && combinedNoise < 0.76f) {
                    // Check for boundary pixels
                    bool isOnBoundary = false;
                    
                    // Examine neighboring pixels
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            // Calculate noise for neighbors
                            float nPrimary = sin((wx+dx) * 0.008f) * sin((wy+dy) * 0.01f);
                            float nSecondary = sin(((wx+dx) * 0.015f + (wy+dy) * 0.013f) * 0.8f);
                            float nCombined = nPrimary * 0.6f + nSecondary * 0.4f;
                            nCombined = (nCombined + 1.0f) / 2.0f;
                            
                            // Check if neighbor is outside gravel pocket
                            if (nCombined <= 0.68f || nCombined >= 0.78f) {
                                isOnBoundary = true;
                                break;
                            }
                        }
                        if (isOnBoundary) break;
                    }
                    
                    // Set to gravel for non-boundary pixels
                    material = MaterialType::Gravel;
                    
                    // Smooth transitions at boundaries
                    if (isOnBoundary) {
                        int edgePattern = (wx * 3 + wy * 5) % 7;
                        if (edgePattern < 3) { // ~43% chance to revert to dirt
                            material = MaterialType::Dirt;
                        }
                    }
                }
            } else {
                // Stone layer covers the vast majority of the world
                material = MaterialType::Stone;
                
                // Underground material veins
                int wx = x;
                int wy = y;
                
                // Use different noise frequencies at different depths to create distinct layers
                float depthFactor = (float)(wy - stoneTop) / (m_height - stoneTop);
                float noiseFreq = 0.01f - 0.005f * depthFactor; // Higher frequencies deeper down
                
                // Different noise functions for different depth zones
                float noiseValue;
                if (wy < stoneTop + (m_height - stoneTop) / 3) {
                    // Upper stone layer - longer horizontal veins
                    noiseValue = sin(wx * 0.007f + wy * 0.013f) * sin(wy * 0.009f + wx * 0.005f);
                } else if (wy < stoneTop + 2 * (m_height - stoneTop) / 3) {
                    // Middle stone layer - more circular/blobby patterns
                    noiseValue = sin(wx * 0.009f + wy * 0.011f) * cos(wx * 0.012f + wy * 0.008f);
                } else {
                    // Deep stone layer - larger, more ore-rich deposits
                    noiseValue = sin(wx * 0.006f + wy * 0.004f) * sin(wy * 0.007f + wx * 0.009f);
                }
                
                // Normalize to 0-1 range
                noiseValue = (noiseValue + 1.0f) / 2.0f;
                
                // Create different material types based on depth and noise value
                bool createSpecialMaterial = false;
                MaterialType specialMaterial = MaterialType::Stone;
                
                // Upper layer materials
                if (wy < stoneTop + (m_height - stoneTop) / 3) {
                    if (noiseValue > 0.75f && noiseValue < 0.79f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Gravel; // Common in upper layer
                    } else if (noiseValue > 0.88f && noiseValue < 0.90f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Sand; // Uncommon in upper layer
                    }
                }
                // Middle layer materials
                else if (wy < stoneTop + 2 * (m_height - stoneTop) / 3) {
                    if (noiseValue > 0.72f && noiseValue < 0.75f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Gravel; // Less common in middle
                    } else if (noiseValue > 0.86f && noiseValue < 0.89f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Sand; // More common in middle
                    } else if (noiseValue > 0.94f && noiseValue < 0.96f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Coal; // Only in middle layer
                    }
                }
                // Deep layer materials
                else {
                    if (noiseValue > 0.80f && noiseValue < 0.82f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Gravel; // Rare in deep layer
                    } else if (noiseValue > 0.84f && noiseValue < 0.88f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Sand; // More common in deep layer
                    } else if (noiseValue > 0.93f && noiseValue < 0.95f) {
                        createSpecialMaterial = true;
                        specialMaterial = MaterialType::Coal; // More common in deep layer
                    }
                }
                
                // If we're creating a special material, check for boundary smoothing
                if (createSpecialMaterial) {
                    // Check if we're on the boundary of the vein
                    bool isOnBoundary = false;
                    for (int dy = -1; dy <= 1 && !isOnBoundary; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            int checkX = wx + dx;
                            int checkY = wy + dy;
                            
                            // Skip out-of-bounds checks
                            if (checkX < 0 || checkX >= m_width || checkY < 0 || checkY >= m_height) {
                                continue;
                            }
                            
                            // Check noise value of neighbor
                            float neighborNoise;
                            if (checkY < stoneTop + (m_height - stoneTop) / 3) {
                                neighborNoise = sin(checkX * 0.007f + checkY * 0.013f) * sin(checkY * 0.009f + checkX * 0.005f);
                            } else if (checkY < stoneTop + 2 * (m_height - stoneTop) / 3) {
                                neighborNoise = sin(checkX * 0.009f + checkY * 0.011f) * cos(checkX * 0.012f + checkY * 0.008f);
                            } else {
                                neighborNoise = sin(checkX * 0.006f + checkY * 0.004f) * sin(checkY * 0.007f + checkX * 0.009f);
                            }
                            neighborNoise = (neighborNoise + 1.0f) / 2.0f;
                            
                            // Different threshold checks based on material and layer
                            bool inVeinRange = false;
                            if (specialMaterial == MaterialType::Gravel) {
                                if (checkY < stoneTop + (m_height - stoneTop) / 3) {
                                    inVeinRange = (neighborNoise > 0.75f && neighborNoise < 0.79f);
                                } else if (checkY < stoneTop + 2 * (m_height - stoneTop) / 3) {
                                    inVeinRange = (neighborNoise > 0.72f && neighborNoise < 0.75f);
                                } else {
                                    inVeinRange = (neighborNoise > 0.80f && neighborNoise < 0.82f);
                                }
                            } else if (specialMaterial == MaterialType::Sand) {
                                if (checkY < stoneTop + (m_height - stoneTop) / 3) {
                                    inVeinRange = (neighborNoise > 0.88f && neighborNoise < 0.90f);
                                } else if (checkY < stoneTop + 2 * (m_height - stoneTop) / 3) {
                                    inVeinRange = (neighborNoise > 0.86f && neighborNoise < 0.89f);
                                } else {
                                    inVeinRange = (neighborNoise > 0.84f && neighborNoise < 0.88f);
                                }
                            } else if (specialMaterial == MaterialType::Coal) {
                                if (checkY < stoneTop + 2 * (m_height - stoneTop) / 3) {
                                    inVeinRange = (neighborNoise > 0.94f && neighborNoise < 0.96f);
                                } else {
                                    inVeinRange = (neighborNoise > 0.93f && neighborNoise < 0.95f);
                                }
                            }
                            
                            // If neighboring pixel is outside the vein range, we're on a boundary
                            if (!inVeinRange) {
                                isOnBoundary = true;
                                break;
                            }
                        }
                    }
                    
                    // Apply the special material
                    material = specialMaterial;
                    
                    // For smoother edges, occasionally revert boundary pixels to stone
                    if (isOnBoundary) {
                        int edgePattern;
                        if (specialMaterial == MaterialType::Gravel) {
                            edgePattern = (wx + wy) % 3;
                            if (edgePattern == 0) material = MaterialType::Stone;
                        } else if (specialMaterial == MaterialType::Sand) {
                            edgePattern = (wx * wy) % 4;
                            if (edgePattern == 0) material = MaterialType::Stone;
                        } else if (specialMaterial == MaterialType::Coal) {
                            edgePattern = (wx * 2 + wy * 3) % 5;
                            if (edgePattern == 0) material = MaterialType::Stone;
                        }
                    }
                }
            }
            
            // Set the material
            set(x, y, material);
            
            // Apply color variation
            int idx = (y * m_width + x) * 4;
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            // Apply subtle variation to create more realistic appearance
            m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) + rVar));      // R
            m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + gVar));    // G
            m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) + bVar));    // B
            m_pixelData[idx+3] = 255;                                                             // A
        }
    }
    
    // Add uneven ground details on the surface
    std::uniform_int_distribution<int> grassExtrusionDist(0, 30); // Chance to add grass variance
    
    for (int x = 1; x < m_width-1; ++x) {
        // Find the top grass pixel for this column
        int grassTop = -1;
        for (int y = 0; y < m_height; ++y) {
            if (get(x, y) == MaterialType::Grass) {
                grassTop = y;
                break;
            }
        }
        
        if (grassTop < 0) continue; // No grass found
        
        // Sometimes add grass tufts that extend 1-2 pixels higher
        if (grassExtrusionDist(m_rng) < 10) { // 33% chance
            int tufts = m_rng() % 2 + 1; // 1-2 pixels higher
            for (int t = 1; t <= tufts; ++t) {
                int ty = grassTop - t;
                if (ty >= 0) {
                    set(x, ty, MaterialType::Grass);
                    
                    // Apply a slightly different color to the tufts
                    int idx = (ty * m_width + x) * 4;
                    const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(MaterialType::Grass)];
                    m_pixelData[idx] = std::max(0, std::min(255, static_cast<int>(props.r) - 10 - t*5));  // Darker R 
                    m_pixelData[idx+1] = std::max(0, std::min(255, static_cast<int>(props.g) + 15));      // Brighter G for grass
                    m_pixelData[idx+2] = std::max(0, std::min(255, static_cast<int>(props.b) - 10));      // Darker B
                    m_pixelData[idx+3] = 255;
                }
            }
        }
    }
}

void World::generateCaves() {
    // Define number of caves based on world size
    int numCaveSystems = 15 + (m_width / 150); 
    
    // Distribution for cave type
    std::uniform_int_distribution<int> caveTypeDist(0, 100);
    std::uniform_int_distribution<int> materialDist(0, 100);
    
    // Common distributions for all cave types
    std::uniform_int_distribution<int> xDist(m_width / 10, m_width * 9 / 10);
    std::uniform_int_distribution<int> yDist(m_height / 3, m_height * 9 / 10); // Allow caves to be higher
    std::uniform_int_distribution<int> angleDist(0, 628); // 0 to 2π*100
    
    // Various types of cave systems
    enum class CaveType {
        SmallHoles,  // Small isolated holes and minor passages
        WindingCave, // Traditional winding cave paths
        LargeCavern, // Large open caverns
        NarrowPassage, // Extremely narrow network of tunnels
        FloodedCave,  // Cave with water/oil
        MaterialFilled, // Cave filled with specific materials (sand, gravel)
        MassiveComplex // Huge interconnected network
    };
    
    for (int cave = 0; cave < numCaveSystems; ++cave) {
        // Determine cave type based on weighted probability
        CaveType caveType;
        int typeRoll = caveTypeDist(m_rng);
        
        if (typeRoll < 20) { // 20%
            caveType = CaveType::SmallHoles;
        } else if (typeRoll < 45) { // 25%
            caveType = CaveType::WindingCave;
        } else if (typeRoll < 65) { // 20%
            caveType = CaveType::LargeCavern;
        } else if (typeRoll < 75) { // 10%
            caveType = CaveType::NarrowPassage;
        } else if (typeRoll < 85) { // 10%
            caveType = CaveType::FloodedCave;
        } else if (typeRoll < 95) { // 10%
            caveType = CaveType::MaterialFilled;
        } else { // 5%
            caveType = CaveType::MassiveComplex;
        }
        
        // Common starting point for the cave
        int startX = xDist(m_rng);
        int startY = yDist(m_rng);
        
        // Different generation based on cave type
        switch(caveType) {
            case CaveType::SmallHoles: {
                // Create a cluster of small isolated holes
                std::uniform_int_distribution<int> clusterSizeDist(3, 8);
                std::uniform_int_distribution<int> holeRadiusDist(2, 6);
                std::uniform_int_distribution<int> offsetDist(-30, 30);
                
                int numHoles = clusterSizeDist(m_rng);
                
                for (int hole = 0; hole < numHoles; hole++) {
                    int holeX = startX + offsetDist(m_rng);
                    int holeY = startY + offsetDist(m_rng);
                    int radius = holeRadiusDist(m_rng);
                    
                    // Make some holes slightly elliptical
                    float xStretch = 1.0f + (m_rng() % 50) / 100.0f;
                    float yStretch = 1.0f + (m_rng() % 50) / 100.0f;
                    
                    for (int cy = -radius; cy <= radius; cy++) {
                        for (int cx = -radius; cx <= radius; cx++) {
                            float scaledX = cx / xStretch;
                            float scaledY = cy / yStretch;
                            
                            if ((scaledX*scaledX + scaledY*scaledY) <= radius*radius) {
                                int wx = holeX + cx;
                                int wy = holeY + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                        set(wx, wy, MaterialType::Empty);
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
            
            case CaveType::WindingCave: {
                // Traditional winding cave paths with variable width
                std::uniform_int_distribution<int> lengthDist(50, 150);
                std::uniform_int_distribution<int> radiusDist(3, 8); // Slimmer caves
                std::uniform_int_distribution<int> branchChanceDist(0, 100);
                
                int caveLength = lengthDist(m_rng);
                double angle = angleDist(m_rng) / 100.0; // Convert to radians
                
                int x = startX;
                int y = startY;
                
                // Track carved points to allow branching
                std::vector<std::pair<int, int>> pathPoints;
                
                for (int step = 0; step < caveLength; ++step) {
                    // Occasionally vary the radius to create chambers and narrow passages
                    int baseRadius = radiusDist(m_rng);
                    int radius = baseRadius;
                    
                    // Create chamber at the start, middle or end
                    if (step < 5 || (step > caveLength/2-5 && step < caveLength/2+5) || step > caveLength-10) {
                        radius = baseRadius * 2;
                    }
                    
                    // Carve out this section
                    for (int cy = -radius; cy <= radius; ++cy) {
                        for (int cx = -radius; cx <= radius; ++cx) {
                            if (cx*cx + cy*cy <= radius*radius) {
                                int wx = x + cx;
                                int wy = y + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                        set(wx, wy, MaterialType::Empty);
                                    }
                                }
                            }
                        }
                    }
                    
                    // Save this point for potential branching
                    if (step % 10 == 0) {
                        pathPoints.push_back(std::make_pair(x, y));
                    }
                    
                    // Change direction for a winding path - more variation
                    angle += (m_rng() % 100 - 50) / 300.0;
                    
                    // Move in the current direction
                    x += static_cast<int>(cos(angle) * 2);
                    y += static_cast<int>(sin(angle) * 2);
                    
                    // Bound check and reflect if needed
                    if (x < 5) { x = 5; angle = 3.14159 - angle; }
                    if (x >= m_width - 5) { x = m_width - 5; angle = 3.14159 - angle; }
                    if (y < m_height / 4) { y = m_height / 4; angle = -angle; }
                    if (y >= m_height - 5) { y = m_height - 5; angle = -angle; }
                }
                
                // Create branches with probability
                if (!pathPoints.empty() && branchChanceDist(m_rng) < 70) { // 70% chance for branches
                    std::uniform_int_distribution<int> branchLengthDist(15, 40);
                    std::uniform_int_distribution<int> pointSelectDist(0, pathPoints.size() - 1);
                    
                    int numBranches = 1 + (m_rng() % 3); // 1-3 branches
                    
                    for (int branch = 0; branch < numBranches; branch++) {
                        if (pathPoints.empty()) break;
                        
                        int pointIdx = pointSelectDist(m_rng);
                        if (pointIdx >= pathPoints.size()) pointIdx = pathPoints.size() - 1;
                        
                        int branchX = pathPoints[pointIdx].first;
                        int branchY = pathPoints[pointIdx].second;
                        
                        int branchLength = branchLengthDist(m_rng);
                        double branchAngle = angleDist(m_rng) / 100.0;
                        
                        // Create the branch
                        for (int step = 0; step < branchLength; ++step) {
                            int radius = radiusDist(m_rng) - 1; // Branches are slightly slimmer
                            
                            for (int cy = -radius; cy <= radius; ++cy) {
                                for (int cx = -radius; cx <= radius; ++cx) {
                                    if (cx*cx + cy*cy <= radius*radius) {
                                        int wx = branchX + cx;
                                        int wy = branchY + cy;
                                        
                                        if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                            if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                                set(wx, wy, MaterialType::Empty);
                                            }
                                        }
                                    }
                                }
                            }
                            
                            branchAngle += (m_rng() % 100 - 50) / 400.0;
                            branchX += static_cast<int>(cos(branchAngle) * 2);
                            branchY += static_cast<int>(sin(branchAngle) * 2);
                            
                            // Bound check for branch
                            if (branchX < 5 || branchX >= m_width - 5 || 
                                branchY < m_height / 4 || branchY >= m_height - 5) {
                                break;
                            }
                        }
                    }
                }
                break;
            }
            
            case CaveType::LargeCavern: {
                // Large open caverns with more irregular shapes
                std::uniform_int_distribution<int> cavernSizeXDist(30, 80);
                std::uniform_int_distribution<int> cavernSizeYDist(20, 50);
                std::uniform_int_distribution<int> noiseScaleDist(10, 30);
                
                int cavernWidth = cavernSizeXDist(m_rng);
                int cavernHeight = cavernSizeYDist(m_rng);
                int noiseScale = noiseScaleDist(m_rng);
                
                // Generate a large, irregular cavern using noise
                for (int cy = -cavernHeight/2; cy <= cavernHeight/2; cy++) {
                    for (int cx = -cavernWidth/2; cx <= cavernWidth/2; cx++) {
                        // Elliptical base shape
                        float normalizedX = (float)cx / (cavernWidth/2);
                        float normalizedY = (float)cy / (cavernHeight/2);
                        float ellipseDist = normalizedX*normalizedX + normalizedY*normalizedY;
                        
                        if (ellipseDist <= 1.0f) {
                            // Apply noise to make edges irregular
                            float noiseVal = sin((startX + cx) * 0.1f) * cos((startY + cy) * 0.1f);
                            noiseVal = (noiseVal + 1.0f) / 2.0f; // Normalize to 0-1
                            
                            // More irregular edges near the perimeter
                            float edgeFactor = std::max(0.0f, 1.0f - ellipseDist);
                            
                            if (noiseVal > 0.3f || edgeFactor > 0.7f) {
                                int wx = startX + cx;
                                int wy = startY + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                        set(wx, wy, MaterialType::Empty);
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Add some stalactites/stalagmites for visual interest
                std::uniform_int_distribution<int> formationsDist(4, 12);
                std::uniform_int_distribution<int> formationSizeDist(3, 8);
                
                int numFormations = formationsDist(m_rng);
                
                for (int i = 0; i < numFormations; i++) {
                    int formX = startX + (m_rng() % cavernWidth - cavernWidth/2);
                    int formY = startY + (m_rng() % cavernHeight - cavernHeight/2);
                    int size = formationSizeDist(m_rng);
                    
                    // Choose whether it's stalactite (ceiling) or stalagmite (floor)
                    bool isCeiling = m_rng() % 2 == 0;
                    
                    for (int h = 0; h < size; h++) {
                        int width = size - h; // Narrower as it extends
                        
                        for (int w = -width/2; w <= width/2; w++) {
                            int wx = formX + w;
                            int wy = isCeiling ? formY - h : formY + h;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                set(wx, wy, MaterialType::Stone);
                            }
                        }
                    }
                }
                break;
            }
            
            case CaveType::NarrowPassage: {
                // Very narrow, long winding passages
                std::uniform_int_distribution<int> lengthDist(100, 300);
                std::uniform_int_distribution<int> radiusDist(1, 3); // Very narrow
                
                int caveLength = lengthDist(m_rng);
                double angle = angleDist(m_rng) / 100.0;
                
                int x = startX;
                int y = startY;
                
                for (int step = 0; step < caveLength; ++step) {
                    // Ultra-narrow passages with occasional small chambers
                    int radius = radiusDist(m_rng);
                    
                    // Small chambers every so often
                    if (step % 25 == 0) {
                        radius += 2;
                    }
                    
                    for (int cy = -radius; cy <= radius; ++cy) {
                        for (int cx = -radius; cx <= radius; ++cx) {
                            if (cx*cx + cy*cy <= radius*radius) {
                                int wx = x + cx;
                                int wy = y + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                        set(wx, wy, MaterialType::Empty);
                                    }
                                }
                            }
                        }
                    }
                    
                    // More winding for narrow passages - frequently change direction
                    angle += (m_rng() % 100 - 50) / 250.0;
                    
                    // Move in smaller increments for more detail
                    x += static_cast<int>(cos(angle) * 1.5);
                    y += static_cast<int>(sin(angle) * 1.5);
                    
                    // Bound check and reflect 
                    if (x < 5) { x = 5; angle = 3.14159 - angle; }
                    if (x >= m_width - 5) { x = m_width - 5; angle = 3.14159 - angle; }
                    if (y < m_height / 4) { y = m_height / 4; angle = -angle; }
                    if (y >= m_height - 5) { y = m_height - 5; angle = -angle; }
                }
                break;
            }
            
            case CaveType::FloodedCave: {
                // Cave with water/liquid pools
                std::uniform_int_distribution<int> cavernSizeDist(30, 70);
                std::uniform_int_distribution<int> floodLevelDist(30, 70);
                
                int cavernSize = cavernSizeDist(m_rng);
                int floodPercent = floodLevelDist(m_rng); // Percentage of height to flood
                
                // Determine liquid type
                MaterialType liquidType;
                int liquidRoll = materialDist(m_rng);
                
                if (liquidRoll < 60) {  // 60%
                    liquidType = MaterialType::Water;
                } else if (liquidRoll < 85) {  // 25%
                    liquidType = MaterialType::Oil;
                } else {  // 15%
                    liquidType = MaterialType::ToxicSludge;
                }
                
                // First create a cavity
                for (int cy = -cavernSize/2; cy <= cavernSize/2; cy++) {
                    for (int cx = -cavernSize/2; cx <= cavernSize/2; cx++) {
                        // Circular/elliptical shape with noise
                        float dist = sqrt(cx*cx + cy*cy);
                        
                        // Add some noise to the edges
                        float noiseOffset = sin(cx * 0.2f) * cos(cy * 0.2f) * 5.0f;
                        
                        if (dist + noiseOffset < cavernSize/2) {
                            int wx = startX + cx;
                            int wy = startY + cy;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                    // Mark as empty first
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
                
                // Now add liquid at the bottom portion
                int floodHeight = (cavernSize * floodPercent) / 100;
                int floodTop = startY + cavernSize/2 - floodHeight;
                
                for (int cy = 0; cy < floodHeight; cy++) {
                    for (int cx = -cavernSize/2; cx <= cavernSize/2; cx++) {
                        float dist = sqrt(cx*cx + (cy - floodHeight/2)*(cy - floodHeight/2));
                        float noiseOffset = sin(cx * 0.2f) * cos(cy * 0.2f) * 5.0f;
                        
                        if (dist + noiseOffset < cavernSize/2) {
                            int wx = startX + cx;
                            int wy = floodTop + cy;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                if (get(wx, wy) == MaterialType::Empty) {
                                    set(wx, wy, liquidType);
                                }
                            }
                        }
                    }
                }
                
                // Add some dripping stalactites with the liquid
                std::uniform_int_distribution<int> numDripsDist(2, 6);
                int numDrips = numDripsDist(m_rng);
                
                for (int i = 0; i < numDrips; i++) {
                    int dripX = startX - cavernSize/2 + (m_rng() % cavernSize);
                    int ceilingY = startY - cavernSize/2;
                    
                    // Create a stalactite
                    int stalactiteHeight = 2 + (m_rng() % 5);
                    
                    for (int h = 0; h < stalactiteHeight; h++) {
                        int width = 2 - h / 2;
                        if (width < 1) width = 1;
                        
                        for (int w = -width; w <= width; w++) {
                            if (dripX + w >= 0 && dripX + w < m_width && 
                                ceilingY + h >= 0 && ceilingY + h < m_height) {
                                set(dripX + w, ceilingY + h, MaterialType::Stone);
                            }
                        }
                    }
                    
                    // Add a few drops of liquid below the stalactite
                    int numDrops = 1 + (m_rng() % 3);
                    for (int d = 0; d < numDrops; d++) {
                        int dropY = ceilingY + stalactiteHeight + d;
                        
                        if (dripX >= 0 && dripX < m_width && dropY >= 0 && dropY < m_height) {
                            if (get(dripX, dropY) == MaterialType::Empty) {
                                set(dripX, dropY, liquidType);
                            }
                        }
                    }
                }
                
                // Sometimes add a small air pocket trapped at the top
                if (m_rng() % 2 == 0) {
                    int pocketSize = 3 + (m_rng() % 6);
                    int pocketX = startX;
                    int pocketY = startY - cavernSize/2 + pocketSize/2;
                    
                    for (int cy = -pocketSize/2; cy <= pocketSize/2; cy++) {
                        for (int cx = -pocketSize/2; cx <= pocketSize/2; cx++) {
                            if (cx*cx + cy*cy <= (pocketSize/2)*(pocketSize/2)) {
                                int wx = pocketX + cx;
                                int wy = pocketY + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
                break;
            }
            
            case CaveType::MaterialFilled: {
                // Create caves filled with specific materials
                std::uniform_int_distribution<int> cavernSizeDist(25, 60);
                
                int cavernSize = cavernSizeDist(m_rng);
                
                // Choose fill material
                MaterialType fillMaterial;
                int fillRoll = materialDist(m_rng);
                
                if (fillRoll < 40) {  // 40%
                    fillMaterial = MaterialType::Sand;
                } else if (fillRoll < 70) {  // 30%
                    fillMaterial = MaterialType::Gravel;
                } else if (fillRoll < 90) {  // 20%
                    fillMaterial = MaterialType::Mud;
                } else {  // 10%
                    fillMaterial = MaterialType::Coal;
                }
                
                // First create the cavity
                for (int cy = -cavernSize/2; cy <= cavernSize/2; cy++) {
                    for (int cx = -cavernSize/2; cx <= cavernSize/2; cx++) {
                        float dist = sqrt(cx*cx + cy*cy);
                        
                        // Add some noise to the edges
                        float noiseOffset = sin(cx * 0.2f) * cos(cy * 0.2f) * 4.0f;
                        
                        if (dist + noiseOffset < cavernSize/2) {
                            int wx = startX + cx;
                            int wy = startY + cy;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                    // First make it empty
                                    set(wx, wy, MaterialType::Empty);
                                    
                                    // Then immediately fill most of it (leaving some air pockets)
                                    if ((cx+cy) % 6 != 0) { // Occasional air pockets
                                        set(wx, wy, fillMaterial);
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Add a few air pockets
                std::uniform_int_distribution<int> pocketCountDist(3, 8);
                std::uniform_int_distribution<int> pocketSizeDist(3, 8);
                
                int numPockets = pocketCountDist(m_rng);
                
                for (int p = 0; p < numPockets; p++) {
                    int pocketX = startX - cavernSize/2 + (m_rng() % cavernSize);
                    int pocketY = startY - cavernSize/2 + (m_rng() % cavernSize);
                    int pocketSize = pocketSizeDist(m_rng);
                    
                    for (int cy = -pocketSize/2; cy <= pocketSize/2; cy++) {
                        for (int cx = -pocketSize/2; cx <= pocketSize/2; cx++) {
                            if (cx*cx + cy*cy <= (pocketSize/2)*(pocketSize/2)) {
                                int wx = pocketX + cx;
                                int wy = pocketY + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
                break;
            }
            
            case CaveType::MassiveComplex: {
                // Huge interconnected cave network
                std::uniform_int_distribution<int> radiusDist(5, 12);
                std::uniform_int_distribution<int> lengthDist(120, 250);
                std::uniform_int_distribution<int> branchCountDist(3, 8);
                
                // Create a main tunnel
                int mainLength = lengthDist(m_rng);
                double mainAngle = angleDist(m_rng) / 100.0;
                
                int x = startX;
                int y = startY;
                
                // Store points for branching
                std::vector<std::tuple<int, int, double>> junctionPoints;
                
                // Create the main passage
                for (int step = 0; step < mainLength; ++step) {
                    int radius = radiusDist(m_rng);
                    
                    // Create wider chambers at key points
                    if (step % 40 < 5) {
                        radius += 5;
                        
                        // Save this as a junction point
                        if (step % 40 == 2 && junctionPoints.size() < 8) {
                            junctionPoints.push_back(std::make_tuple(x, y, mainAngle + 3.14159/2.0));
                        }
                    }
                    
                    // Carve the main tunnel
                    for (int cy = -radius; cy <= radius; ++cy) {
                        for (int cx = -radius; cx <= radius; ++cx) {
                            if (cx*cx + cy*cy <= radius*radius) {
                                int wx = x + cx;
                                int wy = y + cy;
                                
                                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                    if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                        set(wx, wy, MaterialType::Empty);
                                    }
                                }
                            }
                        }
                    }
                    
                    // Change direction gradually
                    mainAngle += (m_rng() % 100 - 50) / 500.0;
                    
                    // Move to next point
                    x += static_cast<int>(cos(mainAngle) * 2);
                    y += static_cast<int>(sin(mainAngle) * 2);
                    
                    // Bounds checking and reflection
                    if (x < 10) { x = 10; mainAngle = 3.14159 - mainAngle; }
                    if (x >= m_width - 10) { x = m_width - 10; mainAngle = 3.14159 - mainAngle; }
                    if (y < m_height / 4) { y = m_height / 4; mainAngle = -mainAngle; }
                    if (y >= m_height - 10) { y = m_height - 10; mainAngle = -mainAngle; }
                }
                
                // Create branches from the saved junction points
                int numBranches = branchCountDist(m_rng);
                numBranches = std::min(numBranches, (int)junctionPoints.size());
                
                for (int b = 0; b < numBranches; b++) {
                    if (junctionPoints.empty()) break;
                    
                    // Pick a junction point
                    int jIndex = m_rng() % junctionPoints.size();
                    int branchX = std::get<0>(junctionPoints[jIndex]);
                    int branchY = std::get<1>(junctionPoints[jIndex]);
                    double branchAngle = std::get<2>(junctionPoints[jIndex]);
                    
                    // Remove used junction point
                    junctionPoints.erase(junctionPoints.begin() + jIndex);
                    
                    // Create the branch
                    int branchLength = lengthDist(m_rng) / 2;
                    
                    for (int step = 0; step < branchLength; ++step) {
                        int radius = radiusDist(m_rng) - 2; // Slightly smaller than main tunnel
                        
                        // Create smaller chambers along branches
                        if (step % 25 < 3) {
                            radius += 3;
                        }
                        
                        // Carve the branch tunnel
                        for (int cy = -radius; cy <= radius; ++cy) {
                            for (int cx = -radius; cx <= radius; ++cx) {
                                if (cx*cx + cy*cy <= radius*radius) {
                                    int wx = branchX + cx;
                                    int wy = branchY + cy;
                                    
                                    if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                        if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                            set(wx, wy, MaterialType::Empty);
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Change direction gradually but more erratically for branches
                        branchAngle += (m_rng() % 100 - 50) / 400.0;
                        
                        // Move to next point in branch
                        branchX += static_cast<int>(cos(branchAngle) * 2);
                        branchY += static_cast<int>(sin(branchAngle) * 2);
                        
                        // Bounds checking and end branch if it hits boundaries
                        if (branchX < 5 || branchX >= m_width - 5 || 
                            branchY < m_height / 4 || branchY >= m_height - 5) {
                            break;
                        }
                    }
                }
                
                // Add some decorative features - stalactites, stalagmites
                std::uniform_int_distribution<int> featureCountDist(5, 15);
                int numFeatures = featureCountDist(m_rng);
                
                for (int f = 0; f < numFeatures; f++) {
                    // Randomly place within the massive complex area
                    int featureX = startX + (m_rng() % 100 - 50);
                    int featureY = startY + (m_rng() % 100 - 50);
                    
                    // Bounds check
                    if (featureX < 10 || featureX >= m_width - 10 || 
                        featureY < m_height / 4 || featureY >= m_height - 10) {
                        continue;
                    }
                    
                    // Only place if there's empty space
                    if (get(featureX, featureY) != MaterialType::Empty) {
                        continue;
                    }
                    
                    // Decide formation type and size
                    bool isStalactite = m_rng() % 2 == 0;
                    int size = 3 + (m_rng() % 6);
                    
                    // Create the formation
                    for (int h = 0; h < size; h++) {
                        int width = size - h;
                        
                        for (int w = -width/2; w <= width/2; w++) {
                            int wx = featureX + w;
                            int wy = isStalactite ? featureY - h : featureY + h;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                // Use stone or other material types for variety
                                MaterialType formationMaterial = MaterialType::Stone;
                                if (m_rng() % 10 == 0) {
                                    formationMaterial = MaterialType::Gravel;
                                }
                                
                                set(wx, wy, formationMaterial);
                            }
                        }
                    }
                }
                break;
            }
        }
    }
    
    // Second pass - add interconnections between nearby caves
    std::uniform_int_distribution<int> connectChanceDist(0, 100);
    
    if (connectChanceDist(m_rng) < 70) { // 70% chance to add some connections
        std::uniform_int_distribution<int> numConnectionsDist(2, 5);
        int numConnections = numConnectionsDist(m_rng);
        
        for (int c = 0; c < numConnections; c++) {
            // Find two random empty spaces that might be caves
            int x1 = m_width / 4 + (m_rng() % (m_width / 2));
            int y1 = m_height / 3 + (m_rng() % (m_height / 2));
            
            // Search for an actual empty space (cave)
            bool foundFirst = false;
            for (int searchY = y1; searchY < y1 + 50 && !foundFirst; searchY++) {
                for (int searchX = x1; searchX < x1 + 50 && !foundFirst; searchX++) {
                    if (searchX >= 0 && searchX < m_width && searchY >= 0 && searchY < m_height) {
                        if (get(searchX, searchY) == MaterialType::Empty) {
                            x1 = searchX;
                            y1 = searchY;
                            foundFirst = true;
                        }
                    }
                }
            }
            
            if (!foundFirst) continue;
            
            // Find a second point in another direction
            int x2 = m_width / 4 + (m_rng() % (m_width / 2));
            int y2 = m_height / 3 + (m_rng() % (m_height / 2));
            
            bool foundSecond = false;
            for (int searchY = y2; searchY < y2 + 50 && !foundSecond; searchY++) {
                for (int searchX = x2; searchX < x2 + 50 && !foundSecond; searchX++) {
                    if (searchX >= 0 && searchX < m_width && searchY >= 0 && searchY < m_height) {
                        if (get(searchX, searchY) == MaterialType::Empty && 
                            (abs(searchX - x1) > 50 || abs(searchY - y1) > 50)) {
                            x2 = searchX;
                            y2 = searchY;
                            foundSecond = true;
                        }
                    }
                }
            }
            
            if (!foundSecond) continue;
            
            // Create a tunnel between them
            int steps = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
            
            for (int i = 0; i <= steps; i++) {
                int x = x1 + (x2 - x1) * i / steps;
                int y = y1 + (y2 - y1) * i / steps;
                
                // Create a small tunnel
                int radius = 2 + (m_rng() % 2);  // 2-3 radius tunnel
                
                for (int cy = -radius; cy <= radius; cy++) {
                    for (int cx = -radius; cx <= radius; cx++) {
                        if (cx*cx + cy*cy <= radius*radius) {
                            int wx = x + cx;
                            int wy = y + cy;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void World::generateWaterPools() {
    // Add many more water pools for the expanded world
    int numPools = m_width / 60 + 8;
    
    std::uniform_int_distribution<int> poolDist(0, m_width - 1);
    std::uniform_int_distribution<int> poolSizeDist(10, 30);
    std::uniform_int_distribution<int> poolTypeDist(0, 100);
    
    for (int i = 0; i < numPools; ++i) {
        int poolX = poolDist(m_rng);
        int poolSize = poolSizeDist(m_rng);
        bool isOil = poolTypeDist(m_rng) < 30; // 30% chance of oil instead of water
        
        // Find the ground level at this position by scanning down
        int groundY = 0;
        bool foundGround = false;
        for (int y = 0; y < m_height; ++y) {
            if (get(poolX, y) != MaterialType::Empty) {
                groundY = y;
                foundGround = true;
                break;
            }
        }
        
        // If we didn't find ground, try scanning up (for cave ceilings)
        if (!foundGround) {
            for (int y = m_height - 1; y >= 0; --y) {
                if (get(poolX, y) == MaterialType::Empty) {
                    groundY = y;
                    foundGround = true;
                    break;
                }
            }
        }
        
        if (foundGround) {
            // Create a depression for the pool
            int poolDepth = poolSize / 3;
            
            for (int x = poolX - poolSize/2; x < poolX + poolSize/2; ++x) {
                if (x < 0 || x >= m_width) continue;
                
                // Elliptical shape for the pool
                double distRatio = static_cast<double>(x - poolX) / (poolSize/2);
                int depth = static_cast<int>(poolDepth * (1.0 - distRatio*distRatio));
                
                if (depth < 2) depth = 2;
                
                // Apply the pool at the detected ground level
                for (int y = groundY - depth; y < groundY; ++y) {
                    if (y >= 0 && y < m_height) {
                        // First empty the space, then fill with the liquid
                        set(x, y, MaterialType::Empty);
                        set(x, y, isOil ? MaterialType::Oil : MaterialType::Water);
                    }
                }
            }
        }
    }
}

void World::generateMaterialDeposits() {
    // Generate Terraria/Noita-style ore veins and material deposits
    
    // 1. Generate large, coherent ore veins that follow specific patterns
    generateTerrariaStyleOreVeins();
    
    // 2. Generate special structures like oil pockets
    generateSpecialDeposits();
}

// Helper function to create Terraria-style ore veins
void World::generateTerrariaStyleOreVeins() {
    // Generate fewer, smaller ore veins to make them less prominent
    int numVeinSets = m_width / 200 + 5;  // Reduce the number of ore clusters
    
    // Distributions for positioning and sizing
    std::uniform_int_distribution<int> xDist(0, m_width - 1);
    std::uniform_int_distribution<int> yDeepDist(m_height * 2/3, m_height - 20);  // Deep ore veins
    std::uniform_int_distribution<int> yMidDist(m_height / 2, m_height * 2/3);    // Mid-level deposits
    std::uniform_int_distribution<int> branchCountDist(2, 5);   // Fewer branches per vein
    std::uniform_int_distribution<int> branchLengthDist(3, 8);  // Shorter branches
    std::uniform_int_distribution<int> branchWidthDist(1, 2);   // Thinner branches
    std::uniform_int_distribution<int> materialDist(0, 100);    // Material type distribution
    std::uniform_int_distribution<int> angleDist(0, 628);       // Random angle (0-2π × 100)
    
    // Generate distinct ore vein sets
    for (int veinSet = 0; veinSet < numVeinSets; veinSet++) {
        // Pick starting point for this vein cluster
        int startX = xDist(m_rng);
        int startY = yDeepDist(m_rng);
        
        // Determine material type (like gold, silver, copper in Terraria)
        MaterialType material;
        int materialType = materialDist(m_rng);
        
        if (materialType < 40) {
            material = MaterialType::Sand;   // "Gold" (yellow minerals)
        } else if (materialType < 70) {
            material = MaterialType::Gravel; // "Silver" (grey minerals)
        } else {
            material = MaterialType::Wood;   // "Copper" (brownish minerals)
        }
        
        // Create multiple branches from the starting point
        int numBranches = branchCountDist(m_rng);
        
        for (int branch = 0; branch < numBranches; branch++) {
            // Each branch starts at the vein center but goes in different direction
            int branchLength = branchLengthDist(m_rng);
            int branchWidth = branchWidthDist(m_rng);
            double angle = angleDist(m_rng) / 100.0; // Convert to radians
            
            // Create the branch with Terraria-style blob shapes
            for (int step = 0; step < branchLength; step++) {
                // Calculate current position along the branch
                int currentX = startX + static_cast<int>(cos(angle) * step * 1.5);
                int currentY = startY + static_cast<int>(sin(angle) * step * 1.5);
                
                // Slightly vary the angle for a natural winding appearance
                angle += (m_rng() % 100 - 50) / 500.0;
                
                // Generate a Terraria-style blob at this point
                int blobSize = branchWidth - step / 5; // Branches get thinner toward the end
                if (blobSize < 1) blobSize = 1;
                
                // Create a blob of ore
                for (int y = -blobSize; y <= blobSize; y++) {
                    for (int x = -blobSize; x <= blobSize; x++) {
                        // Create pixel-perfect, Terraria-style ore shapes with solid edges
                        float distSq = x*x + y*y;
                        if (distSq <= blobSize * blobSize) {
                            int wx = currentX + x;
                            int wy = currentY + y;
                            
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                // Only replace stone to maintain structure integrity
                                if (get(wx, wy) == MaterialType::Stone) {
                                    // Determine if this is an edge pixel
                                    bool isEdge = false;
                                    
                                    // Check all neighbors to see if we're at a boundary
                                    for (int ny = -1; ny <= 1 && !isEdge; ny++) {
                                        for (int nx = -1; nx <= 1; nx++) {
                                            if (nx == 0 && ny == 0) continue;
                                            
                                            int checkX = wx + nx;
                                            int checkY = wy + ny;
                                            
                                            // If neighbor is outside our blob or outside the world
                                            if (checkX < 0 || checkX >= m_width || 
                                                checkY < 0 || checkY >= m_height ||
                                                get(checkX, checkY) != MaterialType::Stone) {
                                                isEdge = true;
                                                break;
                                            }
                                        }
                                    }
                                    
                                    // Place the ore, with special handling for edges
                                    if (!isEdge || ((wx * 3 + wy * 5) % 7 != 0)) {
                                        set(wx, wy, material);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// Helper function to create special material deposits
void World::generateSpecialDeposits() {
    // Create many more special pockets for the expanded world
    int numSpecialDeposits = m_width / 120 + 10;
    
    std::uniform_int_distribution<int> xDist(0, m_width - 1);
    std::uniform_int_distribution<int> yDist(m_height * 3/5, m_height - 15);
    std::uniform_int_distribution<int> depositSizeDist(8, 16);
    std::uniform_int_distribution<int> typeDist(0, 100);
    
    for (int i = 0; i < numSpecialDeposits; i++) {
        int depositX = xDist(m_rng);
        int depositY = yDist(m_rng);
        int depositSize = depositSizeDist(m_rng);
        
        // Determine what type of special deposit this is
        MaterialType material;
        int type = typeDist(m_rng);
        
        if (type < 50) {
            material = MaterialType::Oil;  // 50% chance for oil reservoirs
        } else if (type < 80) {
            material = MaterialType::Sand; // 30% chance for sand deposits
        } else {
            material = MaterialType::Water; // 20% chance for underground water pocket
        }
        
        // Create a more natural, irregular pocket shape
        // Use Perlin-like noise to create irregular but coherent shapes
        for (int y = -depositSize-2; y <= depositSize+2; y++) {
            for (int x = -depositSize-2; x <= depositSize+2; x++) {
                int wx = depositX + x;
                int wy = depositY + y;
                
                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                    // Base shape is elliptical
                    float distRatio = (float)(x*x)/(depositSize*depositSize) + 
                                     (float)(y*y)/((depositSize*0.8f)*(depositSize*0.8f));
                    
                    // Add noise to create irregular edges
                    float noise = sin(wx * 0.1f + wy * 0.13f) * sin(wy * 0.07f + wx * 0.08f) * 0.2f;
                    
                    // Only place within the irregular shape and only in stone
                    if (distRatio + noise < 1.0f && get(wx, wy) == MaterialType::Stone) {
                        set(wx, wy, material);
                    }
                }
            }
        }
    }
}

Chunk* World::getChunkAt(int chunkX, int chunkY) {
    if (chunkX < 0 || chunkX >= m_chunksX || chunkY < 0 || chunkY >= m_chunksY) {
        return nullptr;
    }
    return m_chunks[chunkY * m_chunksX + chunkX].get();
}

const Chunk* World::getChunkAt(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= m_chunksX || chunkY < 0 || chunkY >= m_chunksY) {
        return nullptr;
    }
    return m_chunks[chunkY * m_chunksX + chunkX].get();
}

void World::worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const {
    chunkX = worldX / Chunk::WIDTH;
    chunkY = worldY / Chunk::HEIGHT;
    localX = worldX % Chunk::WIDTH;
    localY = worldY % Chunk::HEIGHT;
}

void World::updatePixelData() {
    // Only update the dirty chunks' pixel data
    for (int chunkY = 0; chunkY < m_chunksY; ++chunkY) {
        for (int chunkX = 0; chunkX < m_chunksX; ++chunkX) {
            const Chunk* chunk = getChunkAt(chunkX, chunkY);
            if (!chunk || !chunk->isDirty()) continue;
            
            const uint8_t* chunkPixels = chunk->getPixelData();
            
            // Copy chunk's pixel data to the world's pixel data - use memcpy for rows when possible
            for (int y = 0; y < Chunk::HEIGHT; ++y) {
                int worldY = chunkY * Chunk::HEIGHT + y;
                if (worldY >= m_height) continue;
                
                // If the chunk is fully within width boundaries, we can copy the entire row at once
                if ((chunkX + 1) * Chunk::WIDTH <= m_width) {
                    int worldIdx = (worldY * m_width + chunkX * Chunk::WIDTH) * 4;
                    int chunkIdx = y * Chunk::WIDTH * 4;
                    // Optimize with memcpy for entire row
                    std::memcpy(&m_pixelData[worldIdx], &chunkPixels[chunkIdx], Chunk::WIDTH * 4);
                } else {
                    // Handle edge case where chunk extends beyond world width
                    for (int x = 0; x < Chunk::WIDTH; ++x) {
                        int worldX = chunkX * Chunk::WIDTH + x;
                        if (worldX >= m_width) break;
                        
                        int chunkIdx = (y * Chunk::WIDTH + x) * 4;
                        int worldIdx = (worldY * m_width + worldX) * 4;
                        
                        m_pixelData[worldIdx] = chunkPixels[chunkIdx];       // R
                        m_pixelData[worldIdx+1] = chunkPixels[chunkIdx+1];   // G
                        m_pixelData[worldIdx+2] = chunkPixels[chunkIdx+2];   // B
                        m_pixelData[worldIdx+3] = chunkPixels[chunkIdx+3];   // A
                    }
                }
            }
        }
    }
}

// Helper to count water pixels below current position (for depth-based effects)
int Chunk::countWaterBelow(int x, int y) const {
    // Count consecutive water pixels below this position (for depth-based effects)
    int count = 0;
    for (int checkY = y + 1; checkY < HEIGHT && count < 10; checkY++) {
        if (m_grid[checkY * WIDTH + x] == MaterialType::Water) {
            count++;
        } else {
            break; // Stop counting when hit non-water
        }
    }
    return count;
}

// Renderer helper functions for player
bool World::isPlayerDigging() const {
    // For now, the player is not digging
    return false;
}

bool World::performPlayerDigging(int /*mouseX*/, int /*mouseY*/, MaterialType& /*material*/) {
    // For now, the player is not digging
    return false;
}

void World::renderPlayer(float /*scale*/) const {
    // STUB: This is a placeholder for a more advanced player rendering system
    // Currently, the player is handled by the UI overlay in the renderer
    // This method is kept for compatibility with potential future direct world rendering
}

} // namespace PixelPhys