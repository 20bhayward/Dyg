#pragma once

#include "Materials.h"
#include "World.h"
#include <vector>
#include <cmath>
#include <memory>

namespace PixelPhys {

class Character {
public:
    Character(World& world, int startX, int startY);
    ~Character() = default;
    
    // Update character position based on mouse position (slither.io style)
    void updatePosition(int targetX, int targetY);
    
    // Get character position (head position)
    int getX() const { return m_segments.front().x; }
    int getY() const { return m_segments.front().y; }
    
    // Draw character to world
    void draw();
    
    // Clear character from world (for toggling between character and camera mode)
    void clear();
    
    // Get character status
    bool isActive() const { return m_isActive; }
    
    // Activate/deactivate character
    void setActive(bool active) { m_isActive = active; }
    
private:
    // Segment of the worm body
    struct Segment {
        int x, y;
    };
    
    // Reference to the world for interactions
    World& m_world;
    
    // Character body segments (head is at front)
    std::vector<Segment> m_segments;
    
    // Character status
    bool m_isActive;
    
    // Character properties
    const int m_length = 20;      // Number of segments in the worm
    const float m_maxSpeed = 3.0f; // Maximum movement speed (slower for smoother movement)
    const int m_radius = 3;       // Radius of each segment
    float m_currentSpeed = 0.0f;  // Current speed of the worm (for smooth acceleration)
    
    // Material types used for worm rendering
    MaterialType m_headMaterial = MaterialType::WormHead;      // Head material
    MaterialType m_mouthMaterial = MaterialType::WormMouth;    // Mouth material
    MaterialType m_skinMaterial = MaterialType::WormSkin;      // Skin material (inner segments)
    MaterialType m_armorMaterial = MaterialType::WormArmor;    // Armored scales (outer segments)
    
    // Helper functions
    void eatEarth(int x, int y, int radius);
    float calculateDistanceBetween(int x1, int y1, int x2, int y2) const;
};

} // namespace PixelPhys