#include "../include/Renderer.h"
#include <iostream>
#include <vector>
#include <algorithm> // For std::min/max
#include <GL/glew.h>

namespace PixelPhys {

Renderer::Renderer(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight),
      m_textureID(0), m_shaderProgram(0), m_vbo(0), m_vao(0) {
    
    std::cout << "Creating renderer with screen size: " << screenWidth << "x" << screenHeight << std::endl;
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initialize() {
    // Check and report any GL errors before starting
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error at Renderer init start: " << err << std::endl;
    }
    
    // Initialize basic OpenGL state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Explicitly set matrix mode and load identity matrices
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Check for errors after state setup
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after renderer state setup: " << err << std::endl;
        return false;
    }
    
    std::cout << "Renderer initialized successfully!" << std::endl;
    return true;
}

void Renderer::render(const World& world) {
    // Save OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Setup projection for 2D drawing
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, m_screenWidth, m_screenHeight, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Draw a dark gray background for the game view
    glBegin(GL_QUADS);
    glColor3f(0.15f, 0.15f, 0.15f); // Slightly darker than the clear color
    glVertex2f(20, 20);
    glVertex2f(m_screenWidth - 20, 20);
    glVertex2f(m_screenWidth - 20, m_screenHeight - 20);
    glVertex2f(20, m_screenHeight - 20);
    glEnd();
    
    // Draw border around game view
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glColor3f(0.6f, 0.6f, 0.6f); // Light gray border
    glVertex2f(20, 20);
    glVertex2f(m_screenWidth - 20, 20);
    glVertex2f(m_screenWidth - 20, m_screenHeight - 20);
    glVertex2f(20, m_screenHeight - 20);
    glEnd();
    
    // Define the visible area of the world
    int viewWidth = m_screenWidth - 40;  // Margins of 20px on each side
    int viewHeight = m_screenHeight - 40;
    
    // Calculate scaling factor to fit world in view
    float scaleX = static_cast<float>(viewWidth) / world.getWidth();
    float scaleY = static_cast<float>(viewHeight) / world.getHeight();
    float scale = std::min(scaleX, scaleY);
    
    // Draw some of the world's content
    int cellSize = std::max(1, static_cast<int>(scale));
    
    // Draw a sample of materials from the world (for testing)
    int sampleEvery = std::max(1, world.getWidth() / 200); // Don't try to render every pixel
    
    // Draw materials as colored squares
    for (int y = 0; y < world.getHeight(); y += sampleEvery) {
        for (int x = 0; x < world.getWidth(); x += sampleEvery) {
            MaterialType material = world.get(x, y);
            
            // Skip empty cells
            if (material == MaterialType::Empty) {
                continue;
            }
            
            // Calculate screen position
            int screenX = 20 + static_cast<int>(x * scale);
            int screenY = 20 + static_cast<int>(y * scale);
            
            // Set color based on material type
            switch (material) {
                case MaterialType::Sand:
                    glColor3f(0.9f, 0.8f, 0.4f); // Sand color
                    break;
                case MaterialType::Water:
                    glColor4f(0.2f, 0.4f, 0.8f, 0.7f); // Water blue
                    break;
                case MaterialType::Stone:
                    glColor3f(0.5f, 0.5f, 0.5f); // Stone gray
                    break;
                case MaterialType::Wood:
                    glColor3f(0.6f, 0.4f, 0.2f); // Wood brown
                    break;
                case MaterialType::Fire:
                    glColor4f(1.0f, 0.3f, 0.0f, 0.8f); // Fire orange
                    break;
                case MaterialType::Oil:
                    glColor4f(0.2f, 0.1f, 0.1f, 0.8f); // Oil dark
                    break;
                default:
                    glColor3f(1.0f, 0.0f, 1.0f); // Magenta for unknown
                    break;
            }
            
            // Draw the material as a rectangle
            glBegin(GL_QUADS);
            glVertex2f(screenX, screenY);
            glVertex2f(screenX + cellSize, screenY);
            glVertex2f(screenX + cellSize, screenY + cellSize);
            glVertex2f(screenX, screenY + cellSize);
            glEnd();
        }
    }
    
    // Draw a debug grid for reference
    glLineWidth(0.5f);
    glBegin(GL_LINES);
    glColor4f(0.5f, 0.5f, 0.5f, 0.2f); // Transparent gray lines
    
    // Vertical lines (every 100 world units)
    for (int x = 0; x < world.getWidth(); x += 100) {
        int screenX = 20 + static_cast<int>(x * scale);
        glVertex2f(screenX, 20);
        glVertex2f(screenX, m_screenHeight - 20);
    }
    
    // Horizontal lines (every 100 world units)
    for (int y = 0; y < world.getHeight(); y += 100) {
        int screenY = 20 + static_cast<int>(y * scale);
        glVertex2f(20, screenY);
        glVertex2f(m_screenWidth - 20, screenY);
    }
    glEnd();
    
    // Restore OpenGL state
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    
    glPopAttrib();
}

void Renderer::cleanup() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

bool Renderer::createShaders() {
    // We're using immediate mode rendering now, no need for shaders
    return true;
}

bool Renderer::createTexture(int /*width*/, int /*height*/) {
    // We're not using textures in this simple renderer
    return true;
}

void Renderer::updateTexture(const World& /*world*/) {
    // We're not using textures in this simple renderer
}

} // namespace PixelPhys