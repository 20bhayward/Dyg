#include "../include/World.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring> // For memcpy

namespace PixelPhys {

// Chunk implementation

Chunk::Chunk() : m_isDirty(true) {
    // Initialize chunk with empty cells
    m_grid.resize(WIDTH * HEIGHT, MaterialType::Empty);
    
    // Initialize pixel data for rendering (RGBA for each cell)
    m_pixelData.resize(WIDTH * HEIGHT * 4, 0);
}

MaterialType Chunk::get(int x, int y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return MaterialType::Empty;
    }
    return m_grid[y * WIDTH + x];
}

void Chunk::set(int x, int y, MaterialType material) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }
    
    MaterialType oldMaterial = m_grid[y * WIDTH + x];
    if (oldMaterial != material) {
        m_grid[y * WIDTH + x] = material;
        m_isDirty = true;
        
        // Update pixel data for this cell
        int idx = (y * WIDTH + x) * 4;
        const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
        m_pixelData[idx] = props.r;     // R
        m_pixelData[idx+1] = props.g;   // G
        m_pixelData[idx+2] = props.b;   // B
        m_pixelData[idx+3] = 255;       // A (fully opaque)
    }
}

void Chunk::update(Chunk* chunkBelow, Chunk* chunkLeft, Chunk* chunkRight) {
    if (!m_isDirty) {
        return;
    }
    
    // We need to avoid updating a cell twice in the same frame
    // So we create a temporary copy of the grid to read from
    std::vector<MaterialType> oldGrid = m_grid;
    
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
            
            // Gases (rise up)
            else if (props.isGas) {
                // Try to rise straight up
                if (y > 0 && oldGrid[(y-1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y-1) * WIDTH + x] = material;
                }
                // Try to spread left or right
                else {
                    bool spreadLeft = false;
                    bool spreadRight = false;
                    
                    // Randomly choose direction
                    if ((x % 2) == 0) {
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
                    
                    // If we spread, we're done with this cell
                    if (spreadLeft || spreadRight) {
                        continue;
                    }
                    
                    // Try diagonal spreading
                    if (y > 0) {
                        // Try left-up diagonal
                        if (x > 0 && oldGrid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            continue;
                        }
                        // Try right-up diagonal
                        else if (x+1 < WIDTH && oldGrid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
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
            
            // Gases (rise up)
            else if (props.isGas) {
                // Try to rise straight up
                if (y > 0 && m_grid[(y-1) * WIDTH + x] == MaterialType::Empty) {
                    m_grid[idx] = MaterialType::Empty;
                    m_grid[(y-1) * WIDTH + x] = material;
                }
                // Try to spread left or right
                else {
                    bool spreadLeft = false;
                    bool spreadRight = false;
                    
                    // Randomly choose direction
                    if ((x % 2) == 0) {
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
                    
                    // If we spread, we're done with this cell
                    if (spreadLeft || spreadRight) {
                        continue;
                    }
                    
                    // Try diagonal spreading
                    if (y > 0) {
                        // Try left-up diagonal
                        if (x > 0 && m_grid[(y-1) * WIDTH + (x-1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x-1)] = material;
                            continue;
                        }
                        // Try right-up diagonal
                        else if (x+1 < WIDTH && m_grid[(y-1) * WIDTH + (x+1)] == MaterialType::Empty) {
                            m_grid[idx] = MaterialType::Empty;
                            m_grid[(y-1) * WIDTH + (x+1)] = material;
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

void Chunk::updatePixelData() {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            MaterialType material = m_grid[y * WIDTH + x];
            int idx = (y * WIDTH + x) * 4;
            const auto& props = MATERIAL_PROPERTIES[static_cast<std::size_t>(material)];
            
            m_pixelData[idx] = props.r;     // R
            m_pixelData[idx+1] = props.g;   // G
            m_pixelData[idx+2] = props.b;   // B
            m_pixelData[idx+3] = 255;       // A (fully opaque)
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
    // Create the world's chunks
    m_chunks.resize(m_chunksX * m_chunksY);
    for (int i = 0; i < m_chunksX * m_chunksY; ++i) {
        m_chunks[i] = std::make_unique<Chunk>();
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
    m_pixelData[idx+3] = 255;       // A (fully opaque)
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

void World::generate(unsigned int seed) {
    // Set up the random number generator with the seed
    m_rng.seed(seed);
    
    // Clear the world
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            set(x, y, MaterialType::Empty);
        }
    }
    
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
    // Base ground level
    int baseGroundLevel = m_height * 2 / 3;
    
    // Generate smoother height variations for more natural terrain
    std::vector<int> heightMap(m_width, 0);
    
    // Use fewer frequencies with smaller amplitudes for less waviness
    // Large scale features (primary terrain shape) - lower frequency, lower amplitude
    std::uniform_int_distribution<int> freqDist1(4, 6);
    std::uniform_int_distribution<int> ampDist1(8, 18); // Reduced amplitude range
    int freq1 = freqDist1(m_rng);
    int amp1 = ampDist1(m_rng);
    
    // Medium scale features (smaller hills) - smaller amplitude
    std::uniform_int_distribution<int> freqDist2(10, 14);
    std::uniform_int_distribution<int> ampDist2(2, 6); // Much smaller amplitude
    int freq2 = freqDist2(m_rng);
    int amp2 = ampDist2(m_rng);
    
    // Small scale features (tiny details) - very small amplitude
    std::uniform_int_distribution<int> freqDist3(20, 30);
    std::uniform_int_distribution<int> ampDist3(1, 2); // Minimal amplitude
    int freq3 = freqDist3(m_rng);
    int amp3 = ampDist3(m_rng);
    
    // Starting height offset for variation - prevent identical shape patterns
    std::uniform_real_distribution<double> phaseOffsetDist(0, 6.28318530718);
    double phaseOffset1 = phaseOffsetDist(m_rng);
    double phaseOffset2 = phaseOffsetDist(m_rng);
    double phaseOffset3 = phaseOffsetDist(m_rng);
    
    // Generate the height map by combining different frequencies
    for (int x = 0; x < m_width; ++x) {
        // Use noise functions with different phases
        // Primary terrain shape (smoother, gentler)
        double angle1 = static_cast<double>(x) / m_width * freq1 * 6.28318530718 + phaseOffset1;
        int height1 = static_cast<int>(sin(angle1) * amp1);
        
        // Medium details
        double angle2 = static_cast<double>(x) / m_width * freq2 * 6.28318530718 + phaseOffset2;
        int height2 = static_cast<int>(sin(angle2) * amp2);
        
        // Small details
        double angle3 = static_cast<double>(x) / m_width * freq3 * 6.28318530718 + phaseOffset3;
        int height3 = static_cast<int>(sin(angle3) * amp3);
        
        // Combined terrain height
        heightMap[x] = baseGroundLevel + height1 + height2 + height3;
    }
    
    // Apply more smoothing passes to remove sharp edges
    for (int i = 0; i < 4; ++i) { // Increased from 2 to 4 passes
        std::vector<int> smoothedMap = heightMap;
        for (int x = 2; x < m_width - 2; ++x) {
            // Use a wider window for smoothing (5 cells instead of 3)
            smoothedMap[x] = (heightMap[x-2] + heightMap[x-1] + heightMap[x] * 2 + heightMap[x+1] + heightMap[x+2]) / 6;
        }
        heightMap = smoothedMap;
    }
    
    // Add plateaus - areas of constant height
    int numPlateaus = m_width / 250 + 1; // Add a few plateaus
    std::uniform_int_distribution<int> plateauPosDist(0, m_width - 1);
    std::uniform_int_distribution<int> plateauWidthDist(20, 60);
    
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
        
        // Apply plateau with smooth transitions at edges
        for (int x = startX; x <= endX; ++x) {
            double edgeFactor = 1.0;
            // Smooth the edges of the plateau
            if (x < startX + 10) {
                edgeFactor = (x - startX) / 10.0;
            } else if (x > endX - 10) {
                edgeFactor = (endX - x) / 10.0;
            }
            
            heightMap[x] = heightMap[x] * (1.0 - edgeFactor) + plateauHeight * edgeFactor;
        }
    }
    
    // Apply a final smoothing pass
    for (int i = 0; i < 2; ++i) {
        std::vector<int> smoothedMap = heightMap;
        for (int x = 1; x < m_width - 1; ++x) {
            smoothedMap[x] = (heightMap[x-1] + heightMap[x] * 2 + heightMap[x+1]) / 4;
        }
        heightMap = smoothedMap;
    }
    
    // Fill in the terrain based on the height map
    for (int x = 0; x < m_width; ++x) {
        int groundLevel = heightMap[x];
        
        // Main ground (stone)
        for (int y = groundLevel; y < m_height; ++y) {
            if (y >= 0 && y < m_height) {
                set(x, y, MaterialType::Stone);
            }
        }
        
        // Add dirt/sand layer on top with more consistent depth
        int sandDepth = 5 + (std::abs(sin(static_cast<double>(x) / 120.0)) * 2); // Less variation
        
        for (int y = groundLevel - sandDepth; y < groundLevel; ++y) {
            if (y >= 0 && y < m_height) {
                set(x, y, MaterialType::Sand);
            }
        }
    }
}

void World::generateCaves() {
    // Create some cave systems
    int numCaveSystems = 3 + (m_width / 200); 
    
    std::uniform_int_distribution<int> xDist(m_width / 10, m_width * 9 / 10);
    std::uniform_int_distribution<int> yDist(m_height / 2, m_height * 8 / 10);
    std::uniform_int_distribution<int> lengthDist(30, 120);
    std::uniform_int_distribution<int> angleDist(0, 628); // 0 to 2π*100
    std::uniform_int_distribution<int> radiusDist(5, 15);
    
    for (int cave = 0; cave < numCaveSystems; ++cave) {
        // Start point of the cave
        int startX = xDist(m_rng);
        int startY = yDist(m_rng);
        
        // Generate a winding cave path
        int caveLength = lengthDist(m_rng);
        double angle = angleDist(m_rng) / 100.0; // Convert to radians
        
        int x = startX;
        int y = startY;
        
        for (int step = 0; step < caveLength; ++step) {
            // Carve out a circle at this position to make the cave
            int radius = radiusDist(m_rng) / 2 + 3; // Base radius + variation
            
            for (int cy = -radius; cy <= radius; ++cy) {
                for (int cx = -radius; cx <= radius; ++cx) {
                    if (cx*cx + cy*cy <= radius*radius) {
                        int wx = x + cx;
                        int wy = y + cy;
                        
                        if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                            // Don't dig too close to the surface to avoid breaking into open air
                            if (get(wx, wy) != MaterialType::Empty) {
                                set(wx, wy, MaterialType::Empty);
                            }
                        }
                    }
                }
            }
            
            // Change direction slightly for a winding path
            angle += (m_rng() % 100 - 50) / 500.0;
            
            // Move in the current direction
            x += static_cast<int>(cos(angle) * 2);
            y += static_cast<int>(sin(angle) * 2);
            
            // Bound check and reflect if needed
            if (x < 5) { x = 5; angle = 3.14159 - angle; }
            if (x >= m_width - 5) { x = m_width - 5; angle = 3.14159 - angle; }
            if (y < m_height / 2) { y = m_height / 2; angle = -angle; }
            if (y >= m_height - 5) { y = m_height - 5; angle = -angle; }
        }
    }
}

void World::generateWaterPools() {
    // Add water pools (both on surface and in caves)
    int numPools = m_width / 90 + 2;
    
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
    // Generate some underground pockets of materials
    int numDeposits = m_width / 60 + 3;
    
    std::uniform_int_distribution<int> xDist(0, m_width - 1);
    std::uniform_int_distribution<int> yDist(m_height / 2, m_height - 5);
    std::uniform_int_distribution<int> sizeDist(4, 12);
    std::uniform_int_distribution<int> typeDist(0, 100);
    
    for (int i = 0; i < numDeposits; ++i) {
        int depositX = xDist(m_rng);
        int depositY = yDist(m_rng);
        int depositSize = sizeDist(m_rng);
        
        // Determine material type
        MaterialType material;
        int type = typeDist(m_rng);
        
        if (type < 40) {
            material = MaterialType::Sand; // 40% chance
        } else if (type < 70) {
            material = MaterialType::Wood; // 30% chance (embedded tree?)
        } else {
            material = MaterialType::Oil;  // 30% chance
        }
        
        // Create a roughly circular deposit
        for (int y = -depositSize; y <= depositSize; ++y) {
            for (int x = -depositSize; x <= depositSize; ++x) {
                double distance = sqrt(x*x + y*y);
                if (distance <= depositSize) {
                    int wx = depositX + x;
                    int wy = depositY + y;
                    
                    if (wx >= 0 && wx < m_width && wy >= 0 && wy < m_height) {
                        // Only place in existing stone to maintain ground structure
                        if (get(wx, wy) == MaterialType::Stone) {
                            set(wx, wy, material);
                        }
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

} // namespace PixelPhys