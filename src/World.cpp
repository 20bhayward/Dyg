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
        
        m_pixelData[pixelIdx] = (std::max)(0, (std::min)(255, static_cast<int>(props.r) + variation));   // R
        m_pixelData[pixelIdx+1] = (std::max)(0, (std::min)(255, static_cast<int>(props.g) + variation)); // G
        m_pixelData[pixelIdx+2] = (std::max)(0, (std::min)(255, static_cast<int>(props.b) + variation)); // B
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
    // Process the entire world - this is just a wrapper for the regionalized version
    levelLiquids(0, 0, m_width - 1, m_height - 1);
}

// Regionalized version for better performance
void World::levelLiquids(int startX, int startY, int endX, int endY) {
    // Ensure coordinates are within world bounds
    startX = (std::max)(0, startX);
    startY = (std::max)(0, startY);
    endX = (std::min)(m_width - 1, endX);
    endY = (std::min)(m_height - 1, endY);
    
    // This function will look for liquid pixels in a specific region and enforce proper settling
    for (int y = endY - 1; y >= startY; y--) {  // Start from bottom-1, going up
        for (int x = startX; x <= endX; x++) {
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
    
    // Only update pixel data if any chunks were updated
    if (!chunksToUpdate.empty()) {
        updatePixelData();
    }
}

// Optimized version that only updates a specific region of the world
void World::update(int startX, int startY, int endX, int endY) {
    // Create a list of chunks to update in the specified region
    std::vector<std::pair<int, int>> chunksToUpdate;
    
    // Convert world coordinates to chunk coordinates
    int startChunkX = (std::max)(0, startX / Chunk::WIDTH);
    int startChunkY = (std::max)(0, startY / Chunk::HEIGHT);
    int endChunkX = (std::min)(m_chunksX - 1, endX / Chunk::WIDTH);
    int endChunkY = (std::min)(m_chunksY - 1, endY / Chunk::HEIGHT);
    
    // First pass: identify all chunks in the active region and mark them as dirty
    for (int y = startChunkY; y <= endChunkY; ++y) {
        for (int x = startChunkX; x <= endChunkX; ++x) {
            Chunk* chunk = getChunkAt(x, y);
            if (chunk) {
                // Force chunks in active region to be dirty for reliable updates
                chunk->setDirty(true);
                
                // Add this chunk
                chunksToUpdate.emplace_back(x, y);
                
                // Also add neighboring chunks since they might be affected
                // But only if they're outside our active region
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
    newX = (std::max)(playerWidth/2.0f, (std::min)(newX, m_width - playerWidth/2.0f));
    newY = (std::max)(playerHeight/2.0f, (std::min)(newY, m_height - playerHeight/2.0f));
    
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
    m_playerY = 30;  // Start higher since the ground is now near the top 1/3rd of the world
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
    // Disabled material deposits
    
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
    // Make ground at a moderate height in the world to create room for caves
    // Use top 1/3rd of the screen for the ground surface, leaving 2/3rds for underground
    int baseGroundLevel = m_height / 3;
    
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
        int startX = (std::max)(0, plateauCenter - plateauWidth/2);
        int endX = (std::min)(m_width - 1, plateauCenter + plateauWidth/2);
        
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
                
                // Gravel pockets in dirt have been disabled
                // Just keep dirt material
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
                
                // Disable special materials generation - use only stone for now
                bool createSpecialMaterial = false;
                MaterialType specialMaterial = MaterialType::Stone;
                
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
    // Define number of caves based on world size - fewer but higher quality caves
    int numCaveSystems = 4 + (m_width / 300); // Reduced number of cave systems
    
    // Distribution for cave type
    std::uniform_int_distribution<int> caveTypeDist(0, 100);
    std::uniform_int_distribution<int> materialDist(0, 100);
    
    // Common distributions for all cave types
    std::uniform_int_distribution<int> xDist(m_width / 10, m_width * 9 / 10);
    std::uniform_int_distribution<int> yDist(m_height / 3, m_height * 4 / 5); // Caves don't go as deep
    std::uniform_int_distribution<int> angleDist(0, 628); // 0 to 2π*100
    
    // We'll use a simplified approach to generate caves
    // Just make a few basic cave types to keep the codebase clean
    enum class CaveType {
        WindingCave,  // Winding cave paths with proper floors
        LargeCavern   // Open caverns with platforms
    };
    
    for (int cave = 0; cave < numCaveSystems; ++cave) {
        // Simplified cave type selection
        CaveType caveType;
        int typeRoll = caveTypeDist(m_rng);
        
        if (typeRoll < 60) { // 60% - Focus on winding caves
            caveType = CaveType::WindingCave;
        } else { // 40% - Larger caverns
            caveType = CaveType::LargeCavern;
        }
        
        // Common starting point for the cave
        int startX = xDist(m_rng);
        int startY = yDist(m_rng);
        
        // Different generation based on cave type
        if (caveType == CaveType::WindingCave) {
            // Winding tunnel with proper floor structure
            std::uniform_int_distribution<int> pathLengthDist(40, 100);
            std::uniform_int_distribution<int> radiusBaseDist(5, 10);
            std::uniform_int_distribution<int> radiusVarDist(-3, 3);
            std::uniform_int_distribution<int> branchCountDist(1, 3);
            
            int pathLength = pathLengthDist(m_rng);
            std::vector<std::pair<int, int>> pathPoints;
            
            // Starting position
            int x = startX;
            int y = startY;
            
            // Initial angle for the path
            double angle = angleDist(m_rng) / 100.0;
            
            // Store key points for potential branching
            pathPoints.push_back(std::make_pair(x, y));
            
            // Generate the main path
            for (int step = 0; step < pathLength; step++) {
                // Vary radius for more natural cave
                int radiusBase = radiusBaseDist(m_rng);
                int radiusVar = radiusVarDist(m_rng);
                int radius = radiusBase + radiusVar;
                
                // Ensure minimum size
                if (radius < 4) radius = 4;
                
                // Create the tunnel section with solid floors
                for (int cy = -radius; cy <= radius; cy++) {
                    for (int cx = -radius; cx <= radius; cx++) {
                        // Use elliptical shape for more natural tunnels
                        // Make horizontal radius slightly larger for walkability
                        float horizStretch = 1.3f;
                        float vertStretch = 1.0f;
                        
                        float distRatio = cx*cx/(radius*radius*horizStretch*horizStretch) + 
                                        cy*cy/(radius*radius*vertStretch*vertStretch);
                                        
                        if (distRatio <= 1.0f) {
                            int wx = x + cx;
                            int wy = y + cy;
                            
                            // Only carve out above a minimum depth
                            if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                                if (get(wx, wy) != MaterialType::Empty && wy > m_height/4) {
                                    set(wx, wy, MaterialType::Empty);
                                }
                            }
                        }
                    }
                }
                
                // Create a solid floor with Terraria-style walking surface
                int floorY = y + radius - 2; // Slightly above the bottom
                int floorWidth = (int)(radius * 1.8f); // Wider floor
                
                // Create a flat floor with slight undulation for natural look
                for (int fx = -floorWidth; fx <= floorWidth; fx++) {
                    int wx = x + fx;
                    // Add gentle elevation changes with sin wave
                    int floorOffset = (int)(sin(step * 0.05f + fx * 0.1f) * 2.0f);
                    int wy = floorY + floorOffset;
                    
                    if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height && wy < y + radius) {
                        // Ensure walkable floor
                        MaterialType aboveMaterial = wy > 0 ? get(wx, wy - 1) : MaterialType::Empty;
                        
                        // Clear space above the floor for player headroom
                        for (int clearY = 0; clearY <= 3; clearY++) {
                            if (wy - clearY >= 0) {
                                set(wx, wy - clearY, MaterialType::Empty);
                            }
                        }
                        
                        // Always ensure solid ground beneath the floor
                        for (int solidY = 1; solidY <= 3; solidY++) {
                            if (wy + solidY < m_height && get(wx, wy + solidY) == MaterialType::Empty) {
                                set(wx, wy + solidY, MaterialType::Stone);
                            }
                        }
                    }
                }
                
                // Save path points for potential branching
                if (step % 8 == 0) {
                    pathPoints.push_back(std::make_pair(x, y));
                }
                
                // Change direction gradually for smoother, more natural paths
                angle += (m_rng() % 100 - 50) / 800.0; // Gentle turns
                
                // Move in the current direction with variable step size
                float stepSize = 2.0f + sin(step * 0.05f) * 0.5f;
                x += static_cast<int>(cos(angle) * stepSize);
                y += static_cast<int>(sin(angle) * stepSize);
                
                // Bound check and reflect if needed
                if (x < 10) { x = 10; angle = 3.14159 - angle; }
                if (x >= m_width - 10) { x = m_width - 10; angle = 3.14159 - angle; }
                if (y < m_height / 4) { y = m_height / 4; angle = -angle; }
                if (y >= m_height - 10) { y = m_height - 10; angle = -angle; }
            }
            
            // Create branch tunnels for more interesting exploration
            int numBranches = branchCountDist(m_rng);
            
            for (int branch = 0; branch < numBranches; branch++) {
                // Only create branches if we have enough path points
                if (pathPoints.size() < 3) continue;
                
                // Select a random point to branch from
                std::uniform_int_distribution<int> pointDist(1, pathPoints.size() - 2);
                int pointIdx = pointDist(m_rng);
                
                int branchX = pathPoints[pointIdx].first;
                int branchY = pathPoints[pointIdx].second;
                
                // Create a shorter branch path
                int branchLength = pathLength / 3;
                double branchAngle = angleDist(m_rng) / 100.0;
                
                x = branchX;
                y = branchY;
                
                // Generate the branch tunnel (simplified version of main path)
                for (int step = 0; step < branchLength; step++) {
                    int radius = radiusBaseDist(m_rng) - 1; // Smaller radius for branches
                    
                    // Ensure minimum size
                    if (radius < 3) radius = 3;
                    
                    // Create tunnel cavity
                    for (int cy = -radius; cy <= radius; cy++) {
                        for (int cx = -radius; cx <= radius; cx++) {
                            // Elliptical shape
                            float distRatio = cx*cx/(radius*radius*1.2f*1.2f) + cy*cy/(radius*radius);
                            if (distRatio <= 1.0f) {
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
                    
                    // Always create solid floor in branches too
                    int floorY = y + radius - 2;
                    int floorWidth = radius + 2;
                    
                    for (int fx = -floorWidth; fx <= floorWidth; fx++) {
                        int wx = x + fx;
                        int wy = floorY;
                        
                        if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                            // Clear space for player
                            for (int clearY = 0; clearY <= 3; clearY++) {
                                if (wy - clearY >= 0) {
                                    set(wx, wy - clearY, MaterialType::Empty);
                                }
                            }
                            
                            // Create solid floor beneath
                            for (int solidY = 1; solidY <= 3; solidY++) {
                                if (wy + solidY < m_height && get(wx, wy + solidY) == MaterialType::Empty) {
                                    set(wx, wy + solidY, MaterialType::Stone);
                                }
                            }
                        }
                    }
                    
                    // Update branch position
                    branchAngle += (m_rng() % 100 - 50) / 500.0; // More variation in branches
                    x += static_cast<int>(cos(branchAngle) * 2.0f);
                    y += static_cast<int>(sin(branchAngle) * 2.0f);
                    
                    // Bound check
                    if (x < 10 || x >= m_width - 10 || y < m_height / 4 || y >= m_height - 10) {
                        break;  // End branch if hit boundary
                    }
                }
            }
        }
        else if (caveType == CaveType::LargeCavern) {
            // Large cavern with walkable floor and platforms
            std::uniform_int_distribution<int> cavernSizeXDist(30, 65);
            std::uniform_int_distribution<int> cavernSizeYDist(20, 40);
            std::uniform_int_distribution<int> platformCountDist(2, 5);
            std::uniform_int_distribution<int> platformWidthDist(8, 20);
            std::uniform_int_distribution<int> stalactiteCountDist(5, 15);
            
            // Create a larger cavern with more interesting features
            int cavernWidth = cavernSizeXDist(m_rng);
            int cavernHeight = cavernSizeYDist(m_rng);
            
            // Use noise-based approach for more natural-looking cavern shapes
            for (int cy = -cavernHeight; cy <= cavernHeight; cy++) {
                for (int cx = -cavernWidth; cx <= cavernWidth; cx++) {
                    // Create a base elliptical shape for the cavern
                    float xRatio = (float)cx / cavernWidth;
                    float yRatio = (float)cy / cavernHeight;
                    float distRatio = xRatio * xRatio + yRatio * yRatio;
                    
                    // Add noise to the edges for more natural look
                    float edgeNoise = sin(cx * 0.1f) * sin(cy * 0.15f) * 0.2f;
                    
                    // Create cavity
                    if (distRatio + edgeNoise <= 1.0f) {
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
            
            // Create a flatter floor for the cavern
            int floorY = startY + cavernHeight - cavernHeight/4;
            int floorWidth = cavernWidth * 4/5;
            
            // Generate natural-looking floor with gentle slopes
            for (int fx = -floorWidth; fx <= floorWidth; fx++) {
                // Base floor height with slight undulations
                int floorOffset = (int)(sin(fx * 0.05f) * 3.0f + cos(fx * 0.02f) * 2.0f);
                int wy = floorY + floorOffset;
                int wx = startX + fx;
                
                if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                    // Clear space above the floor for player movement
                    for (int clearY = -6; clearY <= 0; clearY++) {
                        if (wy + clearY >= 0 && wy + clearY < m_height) {
                            set(wx, wy + clearY, MaterialType::Empty);
                        }
                    }
                    
                    // Ensure solid ground beneath
                    for (int groundY = 1; groundY <= 3; groundY++) {
                        if (wy + groundY < m_height) {
                            set(wx, wy + groundY, MaterialType::Stone);
                        }
                    }
                }
            }
            
            // Add floating platforms for vertical traversal
            int numPlatforms = platformCountDist(m_rng);
            
            for (int p = 0; p < numPlatforms; p++) {
                // Create platforms at different heights in the cavern
                int platformY = startY - cavernHeight/2 + (p * cavernHeight) / (numPlatforms + 1);
                int platformWidth = platformWidthDist(m_rng);
                
                // Offset platforms horizontally for more natural distribution
                int platformX = startX + (m_rng() % (cavernWidth) - cavernWidth/2);
                
                // Create the platform with slight curve
                for (int fx = -platformWidth; fx <= platformWidth; fx++) {
                    // Add gentle curve to platform
                    float normalizedX = (float)fx / platformWidth * 2.0f - 1.0f;
                    int heightOffset = (int)(sin(normalizedX * 3.14159f) * 2.0f);
                    
                    int wx = platformX + fx;
                    int wy = platformY + heightOffset;
                    
                    if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                        // Clear space above the platform for player
                        for (int clearY = -4; clearY <= -1; clearY++) {
                            if (wy + clearY >= 0) {
                                set(wx, wy + clearY, MaterialType::Empty);
                            }
                        }
                        
                        // Create a solid platform
                        set(wx, wy, MaterialType::Stone);
                        set(wx, wy + 1, MaterialType::Stone);
                    }
                }
            }
            
            // Add stalactites and stalagmites for realistic cave appearance
            int numFormations = stalactiteCountDist(m_rng);
            
            for (int i = 0; i < numFormations; i++) {
                // Distribute formations throughout the cavern
                int formationX = startX + (m_rng() % cavernWidth * 2) - cavernWidth;
                
                // 0-1 for ceiling, 2-3 for floor
                int locationType = m_rng() % 4;
                int formationY;
                
                if (locationType <= 1) {
                    // Ceiling formations (stalactites)
                    formationY = startY - cavernHeight + (m_rng() % (cavernHeight/3));
                } else {
                    // Floor formations (stalagmites)
                    formationY = floorY - (m_rng() % 4);
                }
                
                // Vary formation sizes
                int size = 4 + (m_rng() % 8);
                bool isCeiling = locationType <= 1;
                
                // Create tapered formation (narrower at tip)
                for (int h = 0; h < size; h++) {
                    // Calculate width based on height (tapered shape)
                    float taperRatio = (float)h / size;
                    int width = (int)(size * 0.8f * (1.0f - taperRatio)) + 1;
                    
                    for (int w = -width; w <= width; w++) {
                        int wx = formationX + w;
                        int wy;
                        
                        if (isCeiling) {
                            // Stalactites grow downward
                            wy = formationY + h;
                        } else {
                            // Stalagmites grow upward
                            wy = formationY - h;
                        }
                        
                        if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                            // Only place if the formation is inside the cavern
                            if (isCeiling) {
                                float xr = (float)(wx - startX) / cavernWidth;
                                float yr = (float)(wy - startY) / cavernHeight;
                                if (xr*xr + yr*yr <= 0.9f) {
                                    set(wx, wy, MaterialType::Stone);
                                }
                            } else {
                                // Make sure we don't build into the floor
                                if (wy < floorY) {
                                    set(wx, wy, MaterialType::Stone);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// Helper methods for terrain generation
void World::generateWaterPools() {
    // Add a moderate number of water pools
    int numPools = m_width / 100 + 4; // Reduced number of pools
    
    std::uniform_int_distribution<int> poolDist(0, m_width - 1);
    std::uniform_int_distribution<int> poolSizeDist(10, 30);
    std::uniform_int_distribution<int> poolTypeDist(0, 100);
    
    for (int i = 0; i < numPools; ++i) {
        int poolX = poolDist(m_rng);
        int poolSize = poolSizeDist(m_rng);
        bool isOil = poolTypeDist(m_rng) > 80; // 20% chance for oil
        
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

// Simplified ore generation methods
void World::generateMaterialDeposits() {
    // Simplified version - just add some basic ore deposits
    int numDeposits = m_width / 100 + 10;
    
    std::uniform_int_distribution<int> xDist(0, m_width - 1);
    std::uniform_int_distribution<int> yDist(m_height / 2, m_height - 20);
    std::uniform_int_distribution<int> sizeDist(5, 15);
    std::uniform_int_distribution<int> typeDist(0, 100);
    
    for (int i = 0; i < numDeposits; i++) {
        int depositX = xDist(m_rng);
        int depositY = yDist(m_rng);
        int depositSize = sizeDist(m_rng);
        
        // Pick material type
        MaterialType material;
        if (typeDist(m_rng) < 40) {
            material = MaterialType::Sand;   // "Gold"
        } else if (typeDist(m_rng) < 70) {
            material = MaterialType::Gravel; // "Silver"
        } else {
            material = MaterialType::Coal;   // "Coal"
        }
        
        // Create a simple circular deposit
        for (int y = -depositSize; y <= depositSize; y++) {
            for (int x = -depositSize; x <= depositSize; x++) {
                float distance = sqrt(x*x + y*y);
                if (distance <= depositSize) {
                    int wx = depositX + x;
                    int wy = depositY + y;
                    
                    if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                        if (get(wx, wy) == MaterialType::Stone) {
                            set(wx, wy, material);
                        }
                    }
                }
            }
        }
    }
}

void World::generateTerrariaStyleOreVeins() {
    // Simplified stub
    generateMaterialDeposits();
}

void World::generateSpecialDeposits() {
    // Simplified stub - no special deposits
}

// Helper functions for chunk management
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
            
            // Copy chunk's pixel data to the world's pixel data
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

// Player helper methods
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
