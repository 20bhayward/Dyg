#pragma once

#include "World.h"
#include <SDL2/SDL.h>

// GLEW must be included before any OpenGL headers
#include <GL/glew.h>
#include <GL/gl.h>

namespace PixelPhys {

class Renderer {
public:
    Renderer(int screenWidth, int screenHeight);
    ~Renderer();
    
    // Initialize OpenGL resources
    bool initialize();
    
    // Render the world to the screen
    void render(const World& world);
    
    // Clean up OpenGL resources
    void cleanup();
    
private:
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // OpenGL texture ID for the world
    GLuint m_textureID;
    
    // Shader program ID
    GLuint m_shaderProgram;
    
    // Vertex buffer objects
    GLuint m_vbo;
    GLuint m_vao;
    
    // Create a simple shader program
    bool createShaders();
    
    // Create a texture to hold the world pixel data
    bool createTexture(int width, int height);
    
    // Update the texture with world pixel data
    void updateTexture(const World& world);
    
    // Simple utility function to create and compile shaders
    GLuint compileShader(const char* source, GLenum type);
};

} // namespace PixelPhys