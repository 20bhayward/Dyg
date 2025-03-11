#include "../include/Character.h"

namespace PixelPhys {

Character::Character(World& world, int startX, int startY) 
    : m_world(world), m_isActive(false) {
    
    // Initialize all segments at the start position
    for (int i = 0; i < m_length; ++i) {
        m_segments.push_back({startX, startY});
    }
}

void Character::updatePosition(int targetX, int targetY) {
    if (!m_isActive) {
        return;
    }
    
    // Clear old character before updating position
    clear();
    
    // Get head position
    int headX = m_segments.front().x;
    int headY = m_segments.front().y;
    
    // Calculate direction towards target
    float dx = static_cast<float>(targetX - headX);
    float dy = static_cast<float>(targetY - headY);
    
    // Calculate distance to target
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // If the distance is non-zero, normalize direction and calculate move amount
    if (distance > 0.001f) {
        dx /= distance;
        dy /= distance;
        
        // Smooth acceleration/deceleration
        const float acceleration = 0.2f; // Rate of acceleration
        
        // Adjust speed based on distance to target
        if (distance > 50.0f) {
            // Accelerate when far from target
            m_currentSpeed = std::min(m_maxSpeed, m_currentSpeed + acceleration);
        } else {
            // Gradually slow down when close to target (proportional to distance)
            float slowdownFactor = distance / 50.0f;
            float targetSpeed = m_maxSpeed * slowdownFactor;
            
            // Smoothly adjust current speed towards target speed
            if (m_currentSpeed > targetSpeed) {
                m_currentSpeed = std::max(targetSpeed, m_currentSpeed - acceleration);
            } else {
                m_currentSpeed = std::min(targetSpeed, m_currentSpeed + acceleration);
            }
        }
        
        // Calculate new head position with current speed
        int newHeadX = headX + static_cast<int>(dx * m_currentSpeed);
        int newHeadY = headY + static_cast<int>(dy * m_currentSpeed);
        
        // Create a new head segment
        Segment newHead = {newHeadX, newHeadY};
        
        // Move the worm by adding a new head and removing the tail
        m_segments.insert(m_segments.begin(), newHead);
        m_segments.pop_back();
        
        // Eat earth at new head position
        eatEarth(newHeadX, newHeadY, m_radius);
    } else {
        // Slow down when very close to target
        m_currentSpeed = std::max(0.0f, m_currentSpeed - 0.1f);
    }
    
    // Draw character to world
    draw();
}

void Character::draw() {
    if (!m_isActive) {
        return;
    }
    
    // Draw the worm segments from tail to head (so head is drawn last and is always visible)
    for (int i = static_cast<int>(m_segments.size()) - 1; i >= 0; --i) {
        const Segment& segment = m_segments[i];
        
        // Calculate position within the worm (0 = head, 1 = full length)
        float relativePos = static_cast<float>(i) / static_cast<float>(m_segments.size() - 1);
        
        // Calculate segment size - tapers slightly from head to tail
        int segmentRadius;
        if (i == 0) {
            // Head is slightly larger
            segmentRadius = m_radius + 1;
        } else if (i < 3) {
            // Neck segments maintain size
            segmentRadius = m_radius;
        } else {
            // Body tapers slightly toward tail
            segmentRadius = m_radius - static_cast<int>(relativePos * 1.0f);
            segmentRadius = std::max(segmentRadius, m_radius - 1); // Don't taper too much
        }
        
        // Draw the segment as two parts: inner body and outer armor/skin
        // This creates a more complex, layered appearance
        for (int dy = -segmentRadius; dy <= segmentRadius; ++dy) {
            for (int dx = -segmentRadius; dx <= segmentRadius; ++dx) {
                // Distance from center of segment
                float distFromCenter = std::sqrt(dx*dx + dy*dy);
                
                // Skip pixels outside the segment radius
                if (distFromCenter > segmentRadius) continue;
                
                // Calculate world position
                int x = segment.x + dx;
                int y = segment.y + dy;
                
                // Make sure we're within world bounds
                if (x < 0 || x >= m_world.getWidth() || y < 0 || y >= m_world.getHeight()) {
                    continue;
                }
                
                // Determine material based on segment type and position
                MaterialType material;
                
                if (i == 0) {
                    // Head segment with a simple mouth
                    if (distFromCenter < segmentRadius * 0.4f) {
                        // Mouth area in the center of head
                        material = m_mouthMaterial;
                    } else {
                        // Outer head
                        material = m_headMaterial;
                    }
                } else {
                    // Body segments - darker coloration
                    // Create a segmented pattern alternating between skin and armor
                    bool isArmored;
                    
                    if (i % 2 == 0) {
                        // Even segments have armor on outside
                        isArmored = (distFromCenter > segmentRadius * 0.7f);
                    } else {
                        // Odd segments have armor on inside
                        isArmored = (distFromCenter < segmentRadius * 0.5f);
                    }
                    
                    // Apply appropriate material
                    material = isArmored ? m_armorMaterial : m_skinMaterial;
                }
                
                // Set the material in the world
                m_world.set(x, y, material);
            }
        }
    }
}

void Character::clear() {
    if (!m_isActive) {
        return;
    }
    
    // Remove worm from the world by setting all segments to Empty
    // Use a slightly larger radius to ensure all armor/skin elements are cleared
    int clearRadius = m_radius + 1;
    
    for (const Segment& segment : m_segments) {
        for (int dy = -clearRadius; dy <= clearRadius; ++dy) {
            for (int dx = -clearRadius; dx <= clearRadius; ++dx) {
                // Check if point is within radius for circular shape
                if (dx*dx + dy*dy <= clearRadius*clearRadius) {
                    int x = segment.x + dx;
                    int y = segment.y + dy;
                    
                    // Make sure we're within world bounds
                    if (x >= 0 && x < m_world.getWidth() && y >= 0 && y < m_world.getHeight()) {
                        // Only clear worm materials, not terrain
                        MaterialType material = m_world.get(x, y);
                        if (material == m_headMaterial || 
                            material == m_mouthMaterial || 
                            material == m_skinMaterial || 
                            material == m_armorMaterial) {
                            m_world.set(x, y, MaterialType::Empty);
                        }
                    }
                }
            }
        }
    }
}

void Character::eatEarth(int x, int y, int radius) {
    // Eat (clear) the earth around the given position
    // This is the worm's mouth action
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Distance from center (for circular shape)
            float distSq = dx*dx + dy*dy;
            float mouthRadius = radius * 0.7f; // Mouth is slightly smaller than head
            
            // World coordinates
            int eatX = x + dx;
            int eatY = y + dy;
            
            // Skip if out of bounds
            if (eatX < 0 || eatX >= m_world.getWidth() || eatY < 0 || eatY >= m_world.getHeight()) {
                continue;
            }
            
            // Get the current material at this position
            MaterialType material = m_world.get(eatX, eatY);
            
            // Skip empty and bedrock
            if (material == MaterialType::Empty || material == MaterialType::Bedrock) {
                continue;
            }
            
            // Determine what to do based on distance from center
            if (distSq <= mouthRadius*mouthRadius) {
                // Inner area - eat completely
                m_world.set(eatX, eatY, MaterialType::Empty);
            } 
            else if (distSq <= radius*radius) {
                // Outer area - modify terrain based on material
                
                // Different behavior based on material type
                if (material == MaterialType::Stone || material == MaterialType::DenseRock) {
                    // Turn stone to gravel (break it up)
                    if (std::rand() % 100 < 70) {
                        m_world.set(eatX, eatY, MaterialType::Gravel);
                    }
                }
                else if (material == MaterialType::Dirt || material == MaterialType::TopSoil) {
                    // Churn up dirt, making it looser
                    if (std::rand() % 100 < 80) {
                        m_world.set(eatX, eatY, MaterialType::Sand);
                    }
                }
                else if (material == MaterialType::Sand || material == MaterialType::Sandstone) {
                    // Loosen sandstone to sand
                    if (material == MaterialType::Sandstone && std::rand() % 100 < 60) {
                        m_world.set(eatX, eatY, MaterialType::Sand);
                    }
                }
                else if (material == MaterialType::Grass) {
                    // Remove grass, leave dirt
                    m_world.set(eatX, eatY, MaterialType::Dirt);
                }
                // Other materials remain unchanged in the outer radius
            }
        }
    }
}

float Character::calculateDistanceBetween(int x1, int y1, int x2, int y2) const {
    float dx = static_cast<float>(x2 - x1);
    float dy = static_cast<float>(y2 - y1);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace PixelPhys