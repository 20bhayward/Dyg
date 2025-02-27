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
    // Use shader program if available
    if (m_shaderProgram != 0) {
        glUseProgram(m_shaderProgram);
    }
    
    // Update texture with world data
    updateTexture(world);
    
    // Setup VAO/VBO for rendering if they don't exist
    if (m_vao == 0) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        
        // Define quad vertices and texture coordinates
        float vertices[] = {
            // positions        // texture coords
            -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, // top left
            -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
             1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
             1.0f,  1.0f, 0.0f, 1.0f, 0.0f  // top right
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Texture coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    
    // If using shader, setup lighting
    if (m_shaderProgram != 0) {
        // Find light sources (fire, lava, etc)
        std::vector<float> lightPositionsX;
        std::vector<float> lightPositionsY;
        std::vector<float> lightColorsR;
        std::vector<float> lightColorsG;
        std::vector<float> lightColorsB;
        std::vector<float> lightIntensities;
        
        // Find fire pixels as light sources
        for (int y = 0; y < world.getHeight(); y += 10) {
            for (int x = 0; x < world.getWidth(); x += 10) {
                MaterialType material = world.get(x, y);
                if (material == MaterialType::Fire) {
                    // Normalize coordinates to 0-1 range for shader
                    float normX = static_cast<float>(x) / world.getWidth();
                    float normY = static_cast<float>(y) / world.getHeight();
                    
                    lightPositionsX.push_back(normX);
                    lightPositionsY.push_back(normY);
                    lightColorsR.push_back(1.0f);
                    lightColorsG.push_back(0.6f);
                    lightColorsB.push_back(0.3f); // Orange fire
                    lightIntensities.push_back(1.0f);
                    
                    // Limit light sources for performance
                    if (lightPositionsX.size() >= 10) break;
                }
            }
            if (lightPositionsX.size() >= 10) break;
        }
        
        // Add ambient light source if no lights found
        if (lightPositionsX.empty()) {
            lightPositionsX.push_back(0.5f); // Top center
            lightPositionsY.push_back(0.1f);
            lightColorsR.push_back(1.0f);
            lightColorsG.push_back(1.0f);
            lightColorsB.push_back(1.0f); // White light
            lightIntensities.push_back(0.8f);
        }
        
        // Send light data to shader
        glUniform1i(glGetUniformLocation(m_shaderProgram, "numLights"), 
                    static_cast<int>(lightPositionsX.size()));
                    
        if (!lightPositionsX.empty()) {
            // Create combined array for positions
            float* posArray = new float[lightPositionsX.size() * 2];
            for (size_t i = 0; i < lightPositionsX.size(); i++) {
                posArray[i*2] = lightPositionsX[i];
                posArray[i*2+1] = lightPositionsY[i];
            }
            glUniform2fv(glGetUniformLocation(m_shaderProgram, "lightPos"), 
                         lightPositionsX.size(), posArray);
            delete[] posArray;
            
            // Create combined array for colors
            float* colorArray = new float[lightColorsR.size() * 3];
            for (size_t i = 0; i < lightColorsR.size(); i++) {
                colorArray[i*3] = lightColorsR[i];
                colorArray[i*3+1] = lightColorsG[i];
                colorArray[i*3+2] = lightColorsB[i];
            }
            glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightColor"), 
                         lightColorsR.size(), colorArray);
            delete[] colorArray;
        }
                     
        if (!lightIntensities.empty()) {
            glUniform1fv(glGetUniformLocation(m_shaderProgram, "lightIntensity"), 
                         lightIntensities.size(), &lightIntensities[0]);
        }
    }
    
    // Draw the fullscreen quad with the world texture
    glBindVertexArray(m_vao);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    // Optional: Draw a border around the viewport
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
    
    if (m_shaderProgram != 0) {
        glUseProgram(0); // Disable shader for drawing UI elements
    }
    
    // Draw border around game view
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glColor3f(0.6f, 0.6f, 0.6f); // Light gray border
    glVertex2f(10, 10);
    glVertex2f(m_screenWidth - 10, 10);
    glVertex2f(m_screenWidth - 10, m_screenHeight - 10);
    glVertex2f(10, m_screenHeight - 10);
    glEnd();
    
    // Restore OpenGL state
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    
    glPopAttrib();
    
    // Disable the shader program
    if (m_shaderProgram != 0) {
        glUseProgram(0);
    }
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
    // Vertex shader source
    const char* vertexShaderSource = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos, 1.0);\n"
        "   TexCoord = aTexCoord;\n"
        "}\n";
    
    // Fragment shader source with lighting
    const char* fragmentShaderSource = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D ourTexture;\n"
        "uniform vec2 lightPos[10];\n" // Support up to 10 light sources
        "uniform vec3 lightColor[10];\n"
        "uniform float lightIntensity[10];\n"
        "uniform int numLights;\n"
        "void main()\n"
        "{\n"
        "   vec4 texColor = texture(ourTexture, TexCoord);\n"
        "   vec3 lighting = vec3(0.2, 0.2, 0.2);\n" // Ambient light
        "   for(int i = 0; i < numLights; i++) {\n"
        "       float distance = length(lightPos[i] - TexCoord);\n"
        "       float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);\n"
        "       lighting += lightColor[i] * lightIntensity[i] * attenuation;\n"
        "   }\n"
        "   lighting = clamp(lighting, 0.0, 1.0);\n"
        "   FragColor = vec4(texColor.rgb * lighting, texColor.a);\n"
        "}\n";
    
    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // Check for compilation errors
    GLint success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    // Check for compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Create shader program and link shaders
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);
    
    // Check for linking errors
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Clean up shaders as they're linked to program now
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return true;
}

bool Renderer::createTexture(int /*width*/, int /*height*/) {
    // We're not using textures in this simple renderer
    return true;
}

void Renderer::updateTexture(const World& world) {
    // Create texture if it doesn't exist
    if (m_textureID == 0) {
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        
        // Set texture parameters for pixel-perfect rendering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Use NEAREST filtering to maintain crisp pixel edges like Noita
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    // Upload the pixel data from the world
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, world.getWidth(), world.getHeight(), 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, world.getPixelData());
}

} // namespace PixelPhys