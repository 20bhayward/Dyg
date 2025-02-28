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
    
    // Create our advanced shaders
    if (!createShaders()) {
        std::cerr << "Failed to create shaders" << std::endl;
        return false;
    }
    
    // Initialize framebuffers for post-processing effects
    createFramebuffers();
    
#ifdef USE_OPENVDB
    // Initialize OpenVDB library
    openvdb::initialize();
    
    // Create OpenVDB grids for lighting
    m_lightVolume = openvdb::FloatGrid::create(0.0f);
    m_densityVolume = openvdb::FloatGrid::create(0.0f);
    
    m_lightVolume->setName("LightVolume");
    m_densityVolume->setName("DensityVolume");
    
    std::cout << "OpenVDB initialized for volumetric lighting" << std::endl;
#endif

    // Initialize 2D light grid (used when OpenVDB is not available)
    m_lightGridWidth = m_screenWidth / 8;  // Lower resolution for performance
    m_lightGridHeight = m_screenHeight / 8;
    m_lightGrid.resize(m_lightGridWidth * m_lightGridHeight, 0.0f);
    
    // Check for errors after state setup
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after renderer state setup: " << err << std::endl;
        return false;
    }
    
    std::cout << "Advanced lighting renderer with global illumination initialized successfully!" << std::endl;
    return true;
}

void Renderer::renderShadowMap(const World& world) {
    // Bind shadow map framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Use shadow shader
    glUseProgram(m_shadowShader);
    
    // Set shadow shader uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glUniform1i(glGetUniformLocation(m_shadowShader, "worldTexture"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_mainEmissiveTexture);
    glUniform1i(glGetUniformLocation(m_shadowShader, "emissiveTexture"), 1);
    
    // Set player position for shadow calculation
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    glUniform2f(glGetUniformLocation(m_shadowShader, "playerPos"), playerX, playerY);
    
    // Set world size for coordinate conversion
    glUniform2f(glGetUniformLocation(m_shadowShader, "worldSize"), 
                static_cast<float>(world.getWidth()), 
                static_cast<float>(world.getHeight()));
                
    // Set game time for time-based effects
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    glUniform1f(glGetUniformLocation(m_shadowShader, "gameTime"), gameTime);
    
    // Draw fullscreen quad
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer::renderBloomEffect() {
    // Bind bloom framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Use bloom shader
    glUseProgram(m_bloomShader);
    
    // Set bloom shader uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_mainEmissiveTexture);
    glUniform1i(glGetUniformLocation(m_bloomShader, "emissiveTexture"), 0);
    
    // Draw fullscreen quad
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Implementation of the volumetric lighting and global illumination
void Renderer::renderVolumetricLighting(const World& world) {
    // Bind volumetric lighting framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_volumetricLightFBO);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Use volumetric lighting shader
    glUseProgram(m_volumetricLightShader);
    
    // Set shader uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_mainColorTexture);
    glUniform1i(glGetUniformLocation(m_volumetricLightShader, "colorTexture"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_mainEmissiveTexture);
    glUniform1i(glGetUniformLocation(m_volumetricLightShader, "emissiveTexture"), 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_mainDepthTexture);
    glUniform1i(glGetUniformLocation(m_volumetricLightShader, "depthTexture"), 2);
    
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
    glUniform1i(glGetUniformLocation(m_volumetricLightShader, "shadowTexture"), 3);
    
    // Set world size
    glUniform2f(glGetUniformLocation(m_volumetricLightShader, "worldSize"), 
                static_cast<float>(world.getWidth()), 
                static_cast<float>(world.getHeight()));
    
    // Set light positions - player torch and sun
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    glUniform2f(glGetUniformLocation(m_volumetricLightShader, "playerPos"), playerX, playerY);
    
    // Set game time for day/night cycle
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    glUniform1f(glGetUniformLocation(m_volumetricLightShader, "gameTime"), gameTime);
    
    // Day/night cycle calculation
    float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // 0=night, 1=day
    float sunHeight = 0.15 + 0.35 * dayPhase;
    
    // Sun position
    float sunX = world.getWidth() * 0.5f;
    float sunY = world.getHeight() * sunHeight;
    glUniform2f(glGetUniformLocation(m_volumetricLightShader, "sunPos"), sunX, sunY);
    
    // Global illumination strength
    glUniform1f(glGetUniformLocation(m_volumetricLightShader, "giStrength"), 0.7f);
    
    // Set light colors
    glUniform3f(glGetUniformLocation(m_volumetricLightShader, "playerLightColor"), 1.0f, 0.8f, 0.5f); // Warm torch light
    glUniform3f(glGetUniformLocation(m_volumetricLightShader, "sunlightColor"),  1.0f, 0.9f, 0.7f);  // Warm sunlight
    
    // Pass any additional emissive light sources from the world
    // For now we'll just use the emissive texture
    
#ifdef USE_OPENVDB
    // Update OpenVDB volume based on the world state
    // This is where we'd convert our 2D world data into a 3D density grid
    // For example, converting smoke/fire particles to density fields
    // And computing light propagation through the volume
    
    // For performance reasons, we would compute this at a lower resolution
    // and only update regions that have changed
    
    // In this example, we'll use the shader-based approach instead
#endif
    
    // Process light propagation using our 2D light grid
    // This simulates light spreading from emissive sources through the world
    // We'd normally use a compute shader for this, but for now we'll just 
    // implement a simplified version in our fragment shader
    
    // Draw fullscreen quad to apply lighting calculations
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer::applyPostProcessing() {
    // Bind default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Use post-processing shader
    glUseProgram(m_postProcessShader);
    
    // Set post-processing shader uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_mainColorTexture);
    glUniform1i(glGetUniformLocation(m_postProcessShader, "colorTexture"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomTexture);
    glUniform1i(glGetUniformLocation(m_postProcessShader, "bloomTexture"), 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
    glUniform1i(glGetUniformLocation(m_postProcessShader, "shadowMapTexture"), 2);
    
    // Add the volumetric lighting texture
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_volumetricLightTexture);
    glUniform1i(glGetUniformLocation(m_postProcessShader, "volumetricLightTexture"), 3);
    
    // Set game time for time-based effects
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    glUniform1f(glGetUniformLocation(m_postProcessShader, "gameTime"), gameTime);
    
    // Draw fullscreen quad
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer::render(const World& world) {
    // Create quad geometry if it doesn't exist
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
    
    // Update world texture
    updateTexture(world);
    
    // Multi-pass rendering pipeline
    
    // Pass 1: Render main scene with material properties to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_mainFBO);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use main shader
    glUseProgram(m_shaderProgram);
    
    // Set main shader uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "worldTexture"), 0);
    
    // Set world size
    glUniform2f(glGetUniformLocation(m_shaderProgram, "worldSize"), 
                static_cast<float>(world.getWidth()), 
                static_cast<float>(world.getHeight()));
    
    // Set player position
    float playerX = world.getPlayerX();
    float playerY = world.getPlayerY();
    glUniform2f(glGetUniformLocation(m_shaderProgram, "playerPos"), playerX, playerY);
    
    // Set game time
    static float gameTime = 0.0f;
    gameTime += 0.016f; // Approximately 60 FPS
    glUniform1f(glGetUniformLocation(m_shaderProgram, "gameTime"), gameTime);
    
    // Set material properties for all material types
    for (int i = 0; i < static_cast<int>(MaterialType::COUNT); i++) {
        const auto& props = MATERIAL_PROPERTIES[i];
        
        // Create uniform location strings
        std::string isEmissiveLocation = "materials[" + std::to_string(i) + "].isEmissive";
        std::string isRefractiveLocation = "materials[" + std::to_string(i) + "].isRefractive";
        std::string emissiveStrengthLocation = "materials[" + std::to_string(i) + "].emissiveStrength";
        
        // Set material properties in shader
        glUniform1i(glGetUniformLocation(m_shaderProgram, isEmissiveLocation.c_str()), props.isEmissive ? 1 : 0);
        glUniform1i(glGetUniformLocation(m_shaderProgram, isRefractiveLocation.c_str()), props.isRefractive ? 1 : 0);
        glUniform1f(glGetUniformLocation(m_shaderProgram, emissiveStrengthLocation.c_str()), 
                   static_cast<float>(props.emissiveStrength));
    }
    
    // Draw main scene
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    // Pass 2: Generate shadow map
    renderShadowMap(world);
    
    // Pass 3: Volumetric lighting and global illumination
    renderVolumetricLighting(world);
    
    // Pass 4: Generate bloom effect from emissive materials
    renderBloomEffect();
    
    // Pass 5: Final composition with post-processing
    applyPostProcessing();
    
    // Render player on top of everything
    world.renderPlayer(1.0f);
    
    // Optional: Draw a border around the viewport
    // Switch to fixed-function pipeline for simple 2D drawing
    glUseProgram(0);
    
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
}

void Renderer::cleanup() {
    // Delete main world texture
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    
    // Delete vertex buffer objects
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    
    // Delete shader programs
    GLuint shaders[] = {
        m_shaderProgram, m_shadowShader, m_volumetricLightShader,
        m_lightPropagationShader, m_bloomShader, m_postProcessShader
    };
    
    for (GLuint shader : shaders) {
        if (shader != 0) {
            glDeleteProgram(shader);
        }
    }
    
    m_shaderProgram = m_shadowShader = m_volumetricLightShader = 0;
    m_lightPropagationShader = m_bloomShader = m_postProcessShader = 0;
    
    // Delete framebuffer textures
    GLuint fbTextures[] = {
        m_mainColorTexture, m_mainEmissiveTexture, m_mainDepthTexture,
        m_shadowMapTexture, m_volumetricLightTexture, m_bloomTexture
    };
    glDeleteTextures(6, fbTextures);
    
    // Delete framebuffer objects
    GLuint fbos[] = {m_mainFBO, m_shadowMapFBO, m_volumetricLightFBO, m_bloomFBO};
    glDeleteFramebuffers(4, fbos);
    
    // Reset all handles to 0
    m_mainFBO = m_shadowMapFBO = m_volumetricLightFBO = m_bloomFBO = 0;
    m_mainColorTexture = m_mainEmissiveTexture = m_mainDepthTexture = 0;
    m_shadowMapTexture = m_volumetricLightTexture = m_bloomTexture = 0;
    
#ifdef USE_OPENVDB
    // Clean up OpenVDB resources
    m_lightVolume.reset();
    m_densityVolume.reset();
#endif
}

GLuint Renderer::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Compile vertex and fragment shaders
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        return 0;
    }
    
    // Create shader program and link
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check for linking errors
    GLint success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    // Clean up individual shaders as they're linked to program now
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

bool Renderer::createShaders() {
    // Common vertex shader for all passes
    const char* commonVertexShader = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos, 1.0);\n"
        "   TexCoord = aTexCoord;\n"
        "}\n";
        
    // Volumetric lighting and global illumination shader
    const char* volumetricLightFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D colorTexture;\n"
        "uniform sampler2D emissiveTexture;\n"
        "uniform sampler2D depthTexture;\n"
        "uniform sampler2D shadowTexture;\n"
        "uniform vec2 worldSize;\n"
        "uniform vec2 playerPos;\n"
        "uniform vec2 sunPos;\n"
        "uniform float gameTime;\n"
        "uniform float giStrength;\n"
        "uniform vec3 playerLightColor;\n"
        "uniform vec3 sunlightColor;\n"
        
        // Noise function for lighting variation
        "float hash(vec2 p) {\n"
        "   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        
        "float noise(vec2 p) {\n"
        "   vec2 i = floor(p);\n"
        "   vec2 f = fract(p);\n"
        "   f = f * f * (3.0 - 2.0 * f);\n"
        "   float a = hash(i + vec2(0.0, 0.0));\n"
        "   float b = hash(i + vec2(1.0, 0.0));\n"
        "   float c = hash(i + vec2(0.0, 1.0));\n"
        "   float d = hash(i + vec2(1.0, 1.0));\n"
        "   return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        
        // This simulates light propagation through the scene
        "vec3 propagateLight(vec2 uv, vec2 lightPos, vec3 lightColor, float intensity, float range) {\n"
        "   // Calculate world position\n"
        "   vec2 worldPos = uv * worldSize;\n"
        "   float dist = length(worldPos - lightPos);\n"
        "   \n"
        "   // Calculate attenuation with inverse square falloff\n"
        "   float attenuation = 1.0 / (1.0 + dist * dist / (range * range));\n"
        "   attenuation = pow(attenuation, 2.0) * intensity;\n"
        "   \n"
        "   // Cast rays from light source to current pixel to check for occlusion\n"
        "   vec2 dir = normalize(worldPos - lightPos);\n"
        "   const int RAY_STEPS = 16;\n"
        "   float occlusion = 0.0;\n"
        "   \n"
        "   // Sample shadow map along ray to compute occlusion\n"
        "   for(int i = 0; i < RAY_STEPS; i++) {\n"
        "       float t = float(i) / float(RAY_STEPS);\n"
        "       vec2 samplePos = mix(lightPos, worldPos, t);\n"
        "       vec2 sampleUV = samplePos / worldSize;\n"
        "       \n"
        "       // Check if valid UV\n"
        "       if(sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0) {\n"
        "           float shadowValue = texture(shadowTexture, sampleUV).r;\n"
        "           occlusion += (1.0 - shadowValue) * (1.0 - t);\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Normalize and apply soft shadows\n"
        "   occlusion = clamp(occlusion / float(RAY_STEPS) * 4.0, 0.0, 1.0);\n"
        "   \n"
        "   // Add some noise for natural light variation\n"
        "   float variation = noise(worldPos * 0.03 + gameTime * 0.02) * 0.1 + 0.95;\n"
        "   \n"
        "   // Calculate final light contribution with occlusion\n"
        "   vec3 lightContribution = lightColor * attenuation * (1.0 - occlusion) * variation;\n"
        "   \n"
        "   // Apply scattering effect (light visible through semi-transparent materials)\n"
        "   vec4 colorSample = texture(colorTexture, uv);\n"
        "   float transparency = 1.0 - colorSample.a;\n"
        "   lightContribution *= mix(1.0, 2.0, transparency);\n"
        "   \n"
        "   return lightContribution;\n"
        "}\n"
        
        // Apply global illumination - light bounces from surfaces
        "vec3 calculateGI(vec2 uv) {\n"
        "   vec3 gi = vec3(0.0);\n"
        "   vec4 emissive = texture(emissiveTexture, uv);\n"
        "   \n"
        "   // For true GI, we'd sample surrounding pixels and propagate light\n"
        "   // This is a simplified version that samples nearby emissive pixels\n"
        "   const int GI_SAMPLES = 16;\n"
        "   const float GI_RADIUS = 0.05;\n"
        "   \n"
        "   for(int i = 0; i < GI_SAMPLES; i++) {\n"
        "       float angle = float(i) / float(GI_SAMPLES) * 6.28;\n"
        "       float radius = GI_RADIUS * (0.5 + 0.5 * hash(vec2(angle, gameTime)));\n"
        "       vec2 offset = vec2(cos(angle), sin(angle)) * radius;\n"
        "       vec2 sampleUV = uv + offset;\n"
        "       \n"
        "       // Ensure we're within bounds\n"
        "       if(sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0) {\n"
        "           vec4 sampleEmissive = texture(emissiveTexture, sampleUV);\n"
        "           float shadowValue = texture(shadowTexture, sampleUV).r;\n"
        "           \n"
        "           // Only add contribution if there's emission\n"
        "           if(length(sampleEmissive.rgb) > 0.01) {\n"
        "               // Calculate falloff based on distance\n"
        "               float falloff = 1.0 - length(offset) / GI_RADIUS;\n"
        "               \n"
        "               // Add contribution from this sample\n"
        "               gi += sampleEmissive.rgb * falloff * shadowValue;\n"
        "           }\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Normalize and add ambient contribution\n"
        "   gi = gi / float(GI_SAMPLES) * giStrength;\n"
        "   \n"
        "   return gi;\n"
        "}\n"
        
        // Godray effect - volumetric lighting from sun
        "vec3 calculateGodRays(vec2 uv) {\n"
        "   // Convert sun position to UV space\n"
        "   vec2 sunUV = sunPos / worldSize;\n"
        "   \n"
        "   // Calculate direction to sun\n"
        "   vec2 deltaUV = uv - sunUV;\n"
        "   deltaUV *= 1.0 / float(64); // Sample count\n"
        "   \n"
        "   // Raymarching parameters\n"
        "   float decay = 0.97;\n"
        "   float exposure = 0.65;\n"
        "   float density = 0.5;\n"
        "   float weight = 0.3;\n"
        "   \n"
        "   // Start with black\n"
        "   vec3 godray = vec3(0.0);\n"
        "   \n"
        "   // Current position starts at pixel position\n"
        "   vec2 pos = uv;\n"
        "   \n"
        "   // Starting light value\n"
        "   float illuminationDecay = 1.0;\n"
        "   \n"
        "   // Calculate day/night cycle\n"
        "   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // 0=night, 1=day\n"
        "   \n"
        "   // Only render godrays during day\n"
        "   if(dayPhase < 0.2) return vec3(0.0);\n"
        "   \n"
        "   // Raymarch toward the sun\n"
        "   for(int i=0; i<64; i++) {\n"
        "       // Move toward sun\n"
        "       pos -= deltaUV;\n"
        "       \n"
        "       // Get the shadow value at this point\n"
        "       vec4 sampleColor = texture(colorTexture, pos);\n"
        "       float shadowValue = texture(shadowTexture, pos).r;\n"
        "       \n"
        "       // Check for occluders\n"
        "       float occluder = 1.0;\n"
        "       if(sampleColor.a > 0.9) {\n"
        "           occluder = 0.0; // Solid object blocks rays\n"
        "       }\n"
        "       \n"
        "       // Add light contribution with decay\n"
        "       godray += sunlightColor * shadowValue * occluder * illuminationDecay * weight;\n"
        "       \n"
        "       // Update decay\n"
        "       illuminationDecay *= decay;\n"
        "   }\n"
        "   \n"
        "   // Apply exposure and daylight factor\n"
        "   godray *= exposure * dayPhase;\n"
        "   \n"
        "   return godray;\n"
        "}\n"
        
        "void main() {\n"
        "   // Sample colors\n"
        "   vec4 sceneColor = texture(colorTexture, TexCoord);\n"
        "   vec4 emissiveColor = texture(emissiveTexture, TexCoord);\n"
        "   \n"
        "   // Calculate lighting from direct sources\n"
        "   vec3 playerLight = propagateLight(TexCoord, playerPos/worldSize, playerLightColor, 1.5, 300.0);\n"
        "   vec3 sunLight = propagateLight(TexCoord, sunPos/worldSize, sunlightColor, 2.0, 10000.0);\n"
        "   \n"
        "   // Calculate global illumination\n"
        "   vec3 gi = calculateGI(TexCoord);\n"
        "   \n"
        "   // Calculate volumetric lighting (god rays)\n"
        "   vec3 godrays = calculateGodRays(TexCoord);\n"
        "   \n"
        "   // Combine all lighting components\n"
        "   vec3 finalLight = playerLight + sunLight + gi + godrays;\n"
        "   \n"
        "   // Add emissive materials directly\n"
        "   finalLight += emissiveColor.rgb;\n"
        "   \n"
        "   // Output results\n"
        "   FragColor = vec4(finalLight, 1.0);\n"
        "}\n";
    
    // Advanced rendering shader with sophisticated lighting and sky effects
    const char* mainFragmentShader = 
        "#version 330 core\n"
        "layout (location = 0) out vec4 FragColor;\n"
        "layout (location = 1) out vec4 EmissiveColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D worldTexture;\n"
        "uniform vec2 worldSize;\n"
        "uniform vec2 playerPos;\n" // Player position in world coordinates
        "uniform float gameTime;\n"  // For time-based effects
        
        // Material properties buffer (emissiveness, etc.)
        "struct MaterialInfo {\n"
        "   bool isEmissive;\n"
        "   bool isRefractive;\n"
        "   float emissiveStrength;\n"
        "};\n"
        "uniform MaterialInfo materials[30];\n" // Max 30 material types
        
        // Improved Perlin-style noise function
        "vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }\n"
        "vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }\n"
        "vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }\n"
        
        "float snoise(vec2 v) {\n"
        "   const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439);\n"
        "   vec2 i  = floor(v + dot(v, C.yy));\n"
        "   vec2 x0 = v -   i + dot(i, C.xx);\n"
        "   vec2 i1;\n"
        "   i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\n"
        "   vec4 x12 = x0.xyxy + C.xxzz;\n"
        "   x12.xy -= i1;\n"
        "   i = mod289(i);\n"
        "   vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));\n"
        "   vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);\n"
        "   m = m*m;\n"
        "   m = m*m;\n"
        "   vec3 x = 2.0 * fract(p * C.www) - 1.0;\n"
        "   vec3 h = abs(x) - 0.5;\n"
        "   vec3 ox = floor(x + 0.5);\n"
        "   vec3 a0 = x - ox;\n"
        "   m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);\n"
        "   vec3 g;\n"
        "   g.x  = a0.x  * x0.x  + h.x  * x0.y;\n"
        "   g.yz = a0.yz * x12.xz + h.yz * x12.yw;\n"
        "   return 130.0 * dot(m, g);\n"
        "}\n"
        
        "float fbm(vec2 uv) {\n"
        "   float value = 0.0;\n"
        "   float amplitude = 0.5;\n"
        "   float frequency = 0.0;\n"
        "   for (int i = 0; i < 6; ++i) {\n"
        "       value += amplitude * snoise(uv * frequency);\n"
        "       frequency *= 2.0;\n"
        "       amplitude *= 0.5;\n"
        "   }\n"
        "   return value;\n"
        "}\n"
        
        "float random(vec2 st) {\n"
        "   return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);\n"
        "}\n"
        
        "// Function to calculate volumetric light/god rays\n"
        "float godRays(vec2 uv, vec2 lightPos, float density, float decay, float weight, float exposure) {\n"
        "   vec2 deltaTextCoord = (uv - lightPos);\n"
        "   deltaTextCoord *= 1.0 / float(90) * density;\n"
        "   vec2 textCoord = uv;\n"
        "   float illuminationDecay = 1.0;\n"
        "   float color = 0.0;\n"
        "   for(int i=0; i < 90; i++) {\n"
        "       textCoord -= deltaTextCoord;\n"
        "       float sampleColor = 0.0;\n"
        "       // Sample depth-based occlusion\n"
        "       if(textCoord.y > 0.5) {\n" 
        "           sampleColor = 0.0;\n" // Underground blocks rays
        "       } else {\n"
        "           sampleColor = 1.0 - textCoord.y * 2.0; // Fade with depth\n"
        "       }\n"
        "       sampleColor *= illuminationDecay * weight;\n"
        "       color += sampleColor;\n"
        "       illuminationDecay *= decay;\n"
        "   }\n"
        "   return color * exposure;\n"
        "}\n"
        
        "// Lens flare effect function\n"
        "float lensFlare(vec2 uv, vec2 sunPos) {\n"
        "   vec2 uvd = uv * length(uv);\n"
        "   float sunDist = distance(sunPos, uv);\n"
        "   \n"
        "   // Base flare\n"
        "   float flareBase = max(0.0, 1.0 - sunDist * 8.0);\n"
        "   \n"
        "   // Create multiple flare artifacts\n"
        "   float f1 = max(0.0, 1.0 - length(uv - sunPos * 0.25) * 4.0);\n"
        "   float f2 = max(0.0, 1.0 - length(uv - sunPos * 0.5) * 5.0);\n"
        "   float f3 = max(0.0, 1.0 - length(uv - sunPos * 0.8) * 6.0);\n"
        "   float f4 = max(0.0, 1.0 - length(uv - sunPos * 1.3) * 3.0);\n"
        "   float f5 = max(0.0, 1.0 - length(uv - sunPos * 1.6) * 9.0);\n"
        "   \n"
        "   // Rainbow hue artifacts\n"
        "   float haloEffect = 0.0;\n"
        "   for(float i=0.0; i<8.0; i++) {\n"
        "       float haloSize = 0.1 + i*0.1;\n"
        "       float haloWidth = 0.02;\n"
        "       float halo = smoothstep(haloSize-haloWidth, haloSize, sunDist) - smoothstep(haloSize, haloSize+haloWidth, sunDist);\n"
        "       haloEffect += halo * (0.5 - abs(0.5-i/8.0));\n"
        "   }\n"
        "   \n"
        "   // Combine effects\n"
        "   return flareBase * 0.5 + haloEffect * 0.2 + f1 * 0.6 + f2 * 0.35 + f3 * 0.2 + f4 * 0.3 + f5 * 0.2;\n"
        "}\n"
        
        "vec3 createSky(vec2 uv) {\n"
        "   // Day/night cycle based on gameTime\n"
        "   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // 0=night, 1=day\n"
        "   \n"
        "   // Calculate sun position\n"
        "   float sunHeight = 0.15 + 0.35 * dayPhase; // Sun moves from low to high\n"
        "   vec2 sunPos = vec2(0.5, sunHeight);\n"
        "   float sunDist = length(uv - sunPos);\n"
        "   \n"
        "   // Calculate moon position (opposite of sun)\n"
        "   vec2 moonPos = vec2(0.5, 1.1 - sunHeight);\n"
        "   float moonDist = length(uv - moonPos);\n"
        "   \n"
        "   // Sky gradient - changes based on time of day\n"
        "   vec3 zenithColor = mix(vec3(0.03, 0.05, 0.2), vec3(0.1, 0.5, 0.9), dayPhase); // Night to day zenith\n"
        "   vec3 horizonColor = mix(vec3(0.1, 0.12, 0.25), vec3(0.7, 0.75, 0.85), dayPhase); // Night to day horizon\n"
        "   float gradientFactor = pow(1.0 - uv.y, 2.0); // Exponential gradient\n"
        "   vec3 skyColor = mix(zenithColor, horizonColor, gradientFactor);\n"
        "   \n"
        "   // Add atmospheric scattering glow at horizon\n"
        "   float horizonGlow = pow(gradientFactor, 4.0) * dayPhase;\n"
        "   vec3 horizonScatter = vec3(0.9, 0.6, 0.35) * horizonGlow;\n"
        "   skyColor += horizonScatter;\n"
        "   \n"
        "   // Add sun with realistic glow\n"
        "   float sunSize = 0.03;\n"
        "   float sunSharpness = 50.0;\n"
        "   float sunGlow = 0.2;\n"
        "   float sunIntensity = smoothstep(0.0, sunSize, sunDist);\n"
        "   sunIntensity = 1.0 - pow(clamp(sunIntensity, 0.0, 1.0), sunSharpness);\n"
        "   \n"
        "   // Add sun atmospheric glow\n"
        "   float sunGlowFactor = 1.0 - smoothstep(sunSize, sunSize + sunGlow, sunDist);\n"
        "   sunGlowFactor = pow(sunGlowFactor, 2.0); // Stronger in center\n"
        "   \n"
        "   // Create sun color that changes with elevation (more red/orange at horizon)\n"
        "   vec3 sunColorDisk = mix(vec3(1.0, 0.7, 0.3), vec3(1.0, 0.95, 0.8), sunHeight * 2.0);\n"
        "   vec3 sunColorGlow = mix(vec3(1.0, 0.5, 0.2), vec3(1.0, 0.8, 0.5), sunHeight * 2.0);\n"
        "   \n"
        "   // Add moon with subtle glow\n"
        "   float moonSize = 0.025;\n"
        "   float moonSharpness = 60.0;\n"
        "   float moonGlow = 0.1;\n"
        "   float moonIntensity = smoothstep(0.0, moonSize, moonDist);\n"
        "   moonIntensity = 1.0 - pow(clamp(moonIntensity, 0.0, 1.0), moonSharpness);\n"
        "   \n"
        "   // Moon atmospheric glow\n"
        "   float moonGlowFactor = 1.0 - smoothstep(moonSize, moonSize + moonGlow, moonDist);\n"
        "   moonGlowFactor = pow(moonGlowFactor, 2.0); // Stronger in center\n"
        "   vec3 moonColor = vec3(0.9, 0.9, 1.0); // Slightly blue-tinted\n"
        "   \n"
        "   // Noise-based clouds using FBM noise\n"
        "   float cloudCoverage = mix(0.5, 0.75, sin(gameTime * 0.01) * 0.5 + 0.5); // Clouds change over time\n"
        "   float cloudHeight = 0.25; // Clouds height in sky\n"
        "   float cloudThickness = 0.2; // Vertical thickness\n"
        "   float cloudSharpness = 2.0; // Controls transition edge \n"
        "   \n"
        "   // Cloud layer 1: Slow, large clouds\n"
        "   float cloudTime1 = gameTime * 0.005;\n"
        "   vec2 cloudPos1 = vec2(uv.x + cloudTime1, uv.y);\n"
        "   float clouds1 = fbm(cloudPos1 * vec2(2.0, 4.0)) * 0.5 + 0.5;\n"
        "   float cloudMask1 = smoothstep(cloudHeight - cloudThickness, cloudHeight, uv.y) - \n"
        "                      smoothstep(cloudHeight, cloudHeight + cloudThickness, uv.y);\n"
        "   float cloudFactor1 = cloudMask1 * smoothstep(0.5 - cloudCoverage, 0.6, clouds1) * dayPhase;\n"
        "   \n"
        "   // Cloud layer 2: Higher frequency for detail\n"
        "   float cloudTime2 = gameTime * 0.008;\n"
        "   vec2 cloudPos2 = vec2(uv.x + cloudTime2, uv.y);\n"
        "   float clouds2 = fbm(cloudPos2 * vec2(5.0, 10.0)) * 0.3 + 0.5;\n"
        "   float cloudMask2 = smoothstep(cloudHeight - cloudThickness * 0.5, cloudHeight, uv.y) - \n"
        "                      smoothstep(cloudHeight, cloudHeight + cloudThickness * 0.7, uv.y);\n"
        "   float cloudFactor2 = cloudMask2 * smoothstep(0.4, 0.6, clouds2) * dayPhase * 0.7;\n"
        "   \n"
        "   // Combine cloud layers with varying lighting\n"
        "   float totalCloudFactor = max(cloudFactor1, cloudFactor2);\n"
        "   \n"
        "   // Cloud colors change based on time of day and sun position\n"
        "   vec3 cloudBrightColor = mix(vec3(0.9, 0.8, 0.7), vec3(1.0, 1.0, 1.0), dayPhase); // Sunset to day\n"
        "   vec3 cloudDarkColor = mix(vec3(0.1, 0.1, 0.2), vec3(0.7, 0.7, 0.7), dayPhase);  // Night to day\n"
        "   \n"
        "   // Simple cloud lighting based on height\n"
        "   float cloudBrightness = 0.6 + 0.4 * (1.0 - uv.y * 2.0);\n"
        "   vec3 cloudColor = mix(cloudDarkColor, cloudBrightColor, cloudBrightness);\n"
        "   \n"
        "   // Add stars at night (less visible during day)\n"
        "   float starThreshold = 0.98;\n"
        "   float starBrightness = (1.0 - dayPhase) * 0.8; // Stars fade during day\n"
        "   float stars = step(starThreshold, random(uv * 500.0)) * starBrightness;\n"
        "   vec3 starColor = vec3(0.9, 0.9, 1.0) * stars;\n"
        "   \n"
        "   // Add subtle nebulae in night sky\n"
        "   vec3 nebulaColor = vec3(0.1, 0.05, 0.2) * (1.0 - dayPhase) * fbm(uv * 5.0);\n"
        "   \n"
        "   // Calculate god rays from sun\n"
        "   float godRaysEffect = 0.0;\n"
        "   if (dayPhase > 0.2) { // Only during day/sunset\n"
        "       godRaysEffect = godRays(uv, sunPos, 0.5, 0.95, 0.3, 0.3) * dayPhase;\n"
        "   }\n"
        "   \n"
        "   // Calculate lens flare (only when sun is visible)\n"
        "   float lensFlareEffect = 0.0;\n"
        "   if (sunHeight > 0.1 && dayPhase > 0.3) {\n"
        "       lensFlareEffect = lensFlare(uv, sunPos) * dayPhase * 0.5;\n"
        "   }\n"
        "   \n"
        "   // Combine all sky elements\n"
        "   vec3 finalSky = skyColor;\n"
        "   \n"
        "   // Add stars and nebulae (at night)\n"
        "   finalSky += starColor + nebulaColor;\n"
        "   \n"
        "   // Add sun disk and glow\n"
        "   finalSky = mix(finalSky, sunColorDisk, sunIntensity * dayPhase);\n"
        "   finalSky = mix(finalSky, sunColorGlow, sunGlowFactor * dayPhase);\n"
        "   \n"
        "   // Add moon (at night)\n"
        "   finalSky = mix(finalSky, moonColor, moonIntensity * (1.0 - dayPhase) * 0.9);\n"
        "   finalSky = mix(finalSky, moonColor * 0.5, moonGlowFactor * (1.0 - dayPhase) * 0.7);\n"
        "   \n"
        "   // Add clouds\n"
        "   finalSky = mix(finalSky, cloudColor, totalCloudFactor);\n"
        "   \n"
        "   // Add god rays and lens flare\n"
        "   finalSky += vec3(1.0, 0.9, 0.7) * godRaysEffect * 0.4;\n"
        "   finalSky += vec3(1.0, 0.8, 0.6) * lensFlareEffect * 0.6;\n"
        "   \n"
        "   return finalSky;\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   vec4 texColor = texture(worldTexture, TexCoord);\n"
        "   \n"
        "   // Determine material type from texture color (in a real game, this would be passed as data)\n"
        "   int materialId = 0; // Default to empty\n"
        "   if (texColor.a > 0.0) {\n"
        "       // Here we would derive the material ID somehow\n"
        "       // For now, we'll use a simple heuristic\n"
        "       if (texColor.r > 0.9 && texColor.g > 0.4 && texColor.b < 0.3) {\n"
        "           materialId = 5; // Fire\n"
        "       } else if (texColor.r < 0.1 && texColor.g > 0.4 && texColor.b > 0.8) {\n"
        "           materialId = 2; // Water\n"
        "       } else if (texColor.r < 0.2 && texColor.g < 0.2 && texColor.b < 0.2) {\n"
        "           materialId = 15; // Coal\n"
        "       } else if (texColor.r < 0.2 && texColor.g > 0.5 && texColor.b < 0.3) {\n"
        "           materialId = 16; // Moss\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Draw sky for empty space\n"
        "   if (texColor.a < 0.01) {\n"
        "       // Make sky visible in the whole world (not just top half)\n"
        "       // Use a simplified sky rendering when performance is an issue\n"
        "       #ifdef SIMPLIFIED_SKY\n"
        "           // Simple gradient sky (better performance)\n"
        "           float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5;\n"
        "           vec3 zenithColor = mix(vec3(0.03, 0.05, 0.2), vec3(0.1, 0.5, 0.9), dayPhase);\n"
        "           vec3 horizonColor = mix(vec3(0.1, 0.12, 0.25), vec3(0.7, 0.75, 0.85), dayPhase);\n"
        "           vec3 skyColor = mix(zenithColor, horizonColor, pow(1.0 - TexCoord.y, 2.0));\n"
        "           \n"
        "           // Add a simple sun\n"
        "           float sunHeight = 0.15 + 0.35 * dayPhase;\n"
        "           vec2 sunPos = vec2(0.5, sunHeight);\n"
        "           float sunDist = length(TexCoord - sunPos);\n"
        "           float sunGlow = 1.0 - smoothstep(0.03, 0.08, sunDist);\n"
        "           vec3 sunColor = mix(vec3(1.0, 0.7, 0.3), vec3(1.0, 0.95, 0.8), sunHeight * 2.0);\n"
        "           skyColor += sunColor * sunGlow * dayPhase;\n"
        "       #else\n"
        "           // Full sky with all effects\n"
        "           vec3 skyColor = createSky(TexCoord);\n"
        "       #endif\n"
        "       \n"
        "       // Gradually fade sky into a dark cave background for underground areas\n"
        "       if (TexCoord.y > 0.5) {\n"
        "           float fadeDepth = smoothstep(0.5, 1.0, TexCoord.y);\n"
        "           vec3 caveColor = vec3(0.05, 0.04, 0.08); // Dark cave/underground\n"
        "           skyColor = mix(skyColor, caveColor, fadeDepth);\n"
        "       }\n"
        "       \n"
        "       FragColor = vec4(skyColor, 1.0);\n"
        "       EmissiveColor = vec4(0.0); // No emission for sky/empty\n"
        "       return;\n"
        "   }\n"
        "   \n"
        "   // Process emissive materials\n"
        "   bool isEmissive = false;\n"
        "   float emissiveStrength = 0.0;\n"
        "   bool isRefractive = false;\n"
        "   \n"
        "   // Use material properties if available\n"
        "   if (materialId > 0 && materialId < 30) {\n"
        "       isEmissive = materials[materialId].isEmissive;\n"
        "       emissiveStrength = materials[materialId].emissiveStrength / 255.0;\n"
        "       isRefractive = materials[materialId].isRefractive;\n"
        "   }\n"
        "   \n"
        "   // Special case for Fire (always emissive)\n"
        "   if (materialId == 5) {\n"
        "       isEmissive = true;\n"
        "       emissiveStrength = 0.8 + 0.2 * sin(gameTime * 5.0); // Flickering fire\n"
        "   }\n"
        "   \n"
        "   // Special effect for water (refraction and wave effect)\n"
        "   if (isRefractive) {\n"
        "       float time = gameTime * 0.05;\n"
        "       float distortion = 0.005 * sin(TexCoord.x * 30.0 + time) * sin(TexCoord.y * 40.0 + time);\n"
        "       texColor.rgb *= 1.1 + distortion; // Subtle water sparkle\n"
        "   }\n"
        "   \n"
        "   // Apply basic lighting (full implementation will come from shadow map)\n"
        "   vec3 lighting = vec3(0.4, 0.4, 0.5); // Base ambient light\n"
        "   \n"
        "   // Biome detection - simplified for this version\n"
        "   // In a full implementation, biome data would come from the world\n"
        "   int biomeType = 0; // 0 = default, 1 = forest, 2 = desert, 3 = tundra, 4 = swamp\n"
        "   \n"
        "   // Simple biome detection based on x position for demo\n"
        "   float worldPosX = TexCoord.x * worldSize.x;\n"
        "   if (worldPosX < worldSize.x * 0.2) biomeType = 1; // Forest on the left\n"
        "   else if (worldPosX < worldSize.x * 0.4) biomeType = 0; // Default in middle-left\n"
        "   else if (worldPosX < worldSize.x * 0.6) biomeType = 2; // Desert in middle\n"
        "   else if (worldPosX < worldSize.x * 0.8) biomeType = 3; // Tundra in middle-right\n"
        "   else biomeType = 4; // Swamp on the right\n"
        "   \n"
        "   // Time of day and biome-specific fog\n"
        "   vec3 fogColor;\n"
        "   float fogDensity = 0.0;\n"
        "   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5; // Same as in createSky()\n"
        "   \n"
        "   // Base fog color determined by time of day\n"
        "   if (dayPhase < 0.3) { // Night\n"
        "       fogColor = vec3(0.05, 0.05, 0.1); // Dark blue night fog\n"
        "       fogDensity = 0.02;\n"
        "   } else if (dayPhase < 0.4) { // Dawn\n"
        "       fogColor = vec3(0.4, 0.3, 0.3); // Pinkish dawn fog\n"
        "       fogDensity = 0.015;\n"
        "   } else if (dayPhase < 0.6) { // Day\n"
        "       fogColor = vec3(0.8, 0.85, 0.9); // Light day fog\n"
        "       fogDensity = 0.005;\n"
        "   } else if (dayPhase < 0.7) { // Dusk\n"
        "       fogColor = vec3(0.5, 0.3, 0.1); // Orange dusk fog\n"
        "       fogDensity = 0.015;\n"
        "   } else { // Night\n"
        "       fogColor = vec3(0.05, 0.05, 0.1); // Dark blue night fog\n"
        "       fogDensity = 0.02;\n"
        "   }\n"
        "   \n"
        "   // Modify fog based on biome\n"
        "   if (biomeType == 1) { // Forest\n"
        "       fogColor *= vec3(0.7, 0.9, 0.7); // Greener\n"
        "       fogDensity *= 1.2; // Denser\n"
        "   } else if (biomeType == 2) { // Desert\n"
        "       fogColor *= vec3(1.0, 0.9, 0.7); // Yellowish\n"
        "       fogDensity *= 0.3; // Much clearer\n"
        "   } else if (biomeType == 3) { // Tundra\n"
        "       fogColor *= vec3(0.8, 0.9, 1.0); // Blueish\n"
        "       fogDensity *= 0.7; // Clearer\n"
        "   } else if (biomeType == 4) { // Swamp\n"
        "       fogColor *= vec3(0.6, 0.7, 0.5); // Greenish brown\n"
        "       fogDensity *= 1.5; // Thicker\n"
        "   }\n"
        "   \n"
        "   // Depth-based darkness (darker as you go deeper)\n"
        "   float depthFactor = smoothstep(0.0, 1.0, TexCoord.y);\n"
        "   lighting *= mix(1.0, 0.3, depthFactor); // Darker with depth\n"
        "   \n"
        "   // Apply fog to lighting - stronger in the distance and depth\n"
        "   float distFromCenter = length(vec2(0.5, 0.2) - TexCoord) * 2.0;\n"
        "   float fogFactor = 1.0 - exp(-fogDensity * (distFromCenter + depthFactor) * 5.0);\n"
        "   lighting = mix(lighting, fogColor, clamp(fogFactor, 0.0, 0.5));\n"
        "   \n"
        "   // Write to color and emissive buffers\n"
        "   FragColor = vec4(texColor.rgb * lighting, texColor.a);\n"
        "   \n"
        "   if (isEmissive) {\n"
        "       EmissiveColor = vec4(texColor.rgb * emissiveStrength, texColor.a);\n"
        "   } else {\n"
        "       EmissiveColor = vec4(0.0);\n"
        "   }\n"
        "}\n";
    
    // Advanced shadow & lighting calculation shader 
    const char* shadowFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D worldTexture;\n"
        "uniform sampler2D emissiveTexture;\n"
        "uniform vec2 playerPos;\n"
        "uniform vec2 worldSize;\n"
        "uniform float gameTime;\n"
        
        // Helper functions for noise and variety
        "float hash(vec2 p) {\n"
        "   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        
        "float noise(vec2 p) {\n"
        "   vec2 i = floor(p);\n"
        "   vec2 f = fract(p);\n"
        "   f = f * f * (3.0 - 2.0 * f);\n"
        "   float a = hash(i + vec2(0.0, 0.0));\n"
        "   float b = hash(i + vec2(1.0, 0.0));\n"
        "   float c = hash(i + vec2(0.0, 1.0));\n"
        "   float d = hash(i + vec2(1.0, 1.0));\n"
        "   return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   vec4 texColor = texture(worldTexture, TexCoord);\n"
        "   vec4 emissive = texture(emissiveTexture, TexCoord);\n"
        "   \n"
        "   // Day/night cycle calculation (synced with sky shader)\n"
        "   float dayPhase = (sin(gameTime * 0.02) + 1.0) * 0.5;\n"
        "   float sunHeight = 0.15 + 0.35 * dayPhase;\n"
        "   vec2 sunPos = vec2(0.5, sunHeight);\n"
        "   \n"
        "   // Check if pixel is solid/opaque for shadow casting\n"
        "   bool isSolid = texColor.a > 0.7;\n"
        "   bool isWater = false;\n"
        "   \n"
        "   // Detect water by color - approximation for now\n"
        "   if (texColor.b > 0.5 && texColor.r < 0.2 && texColor.a > 0.4) {\n"
        "       isWater = true;\n"
        "   }\n"
        "   \n"
        "   // Convert texture coordinates to world space\n"
        "   vec2 worldPos = TexCoord * worldSize;\n"
        "   \n"
        "   // Distance from player (light source)\n"
        "   float playerDist = length(worldPos - playerPos);\n"
        "   \n"
        "   // Calculate basic ambient light based on depth\n"
        "   float ambientBase = mix(0.05, 0.35, (1.0 - TexCoord.y * 2.0));\n"
        "   ambientBase = max(0.02, ambientBase);\n" // Ensure minimum ambient visibility
        "   \n"
        "   // Sun contribution to global ambient (affects mainly surface/sky areas)\n"
        "   float sunAmbient = dayPhase * 0.5 * max(0.0, 1.0 - TexCoord.y * 3.0);\n"
        "   ambientBase += sunAmbient;\n"
        "   \n"
        "   // Noise-based variation for underground ambient light\n"
        "   float ambientNoise = noise(worldPos * 0.002 + gameTime * 0.01) * 0.04;\n"
        "   \n"
        "   // Cave atmosphere effect - subtle dust particles and ambient flicker\n"
        "   float caveAtmosphere = noise(worldPos * 0.01 + gameTime * 0.02) * 0.02;\n"
        "   float timeBlink = sin(gameTime * 2.0 + worldPos.x * 0.1) * 0.01;\n"
        "   \n"
        "   // Calculate ambient occlusion for concave areas\n"
        "   float ao = 0.0;\n"
        "   const int AO_SAMPLES = 6;\n"
        "   const float AO_RADIUS = 0.01;\n"
        "   \n"
        "   if (!isSolid && !isWater) {\n"
        "       int solidCount = 0;\n"
        "       for (int i = 0; i < AO_SAMPLES; i++) {\n"
        "           float angle = float(i) / float(AO_SAMPLES) * 6.28;\n"
        "           vec2 offset = vec2(cos(angle), sin(angle)) * AO_RADIUS;\n"
        "           vec2 samplePos = TexCoord + offset;\n"
        "           if (texture(worldTexture, samplePos).a > 0.7) {\n"
        "               solidCount++;\n"
        "           }\n"
        "       }\n"
        "       ao = float(solidCount) / float(AO_SAMPLES) * 0.25;\n"
        "   }\n"
        "   \n"
        "   // Simple 2D shadow calculation with soft shadows\n"
        "   float shadowFactor = 1.0;\n"
        "   \n"
        "   // Direction from current pixel to player\n"
        "   vec2 toPlayer = normalize(playerPos - worldPos);\n"
        "   \n"
        "   // Calculate player light color (warm torch-like light)\n"
        "   vec3 playerLightColor = vec3(1.0, 0.8, 0.5);\n"
        "   float playerLightIntensity = 1.2;\n"
        "   \n"
        "   // Add subtle torch flicker effect\n"
        "   float flicker = 0.9 + 0.1 * sin(gameTime * 10.0 + hash(worldPos) * 10.0);\n"
        "   playerLightIntensity *= flicker;\n"
        "   \n"
        "   // Cast ray toward player to see if in shadow - advanced soft shadows\n"
        "   const int MAX_STEPS = 48;\n"
        "   const float STEP_SIZE = 2.0; // pixels\n"
        "   \n"
        "   if (isSolid) {\n"
        "       // Solid objects cast shadows but aren't in shadow themselves\n"
        "       shadowFactor = 0.0;\n"
        "   } else {\n"
        "       // Soft shadow accumulation\n"
        "       float softShadow = 0.0;\n"
        "       float rayLength = 0.0;\n"
        "       \n"
        "       // Ray march toward player\n"
        "       vec2 pos = worldPos;\n"
        "       bool inShadow = false;\n"
        "       \n"
        "       for (int i = 0; i < MAX_STEPS; i++) {\n"
        "           pos += toPlayer * STEP_SIZE;\n"
        "           rayLength += STEP_SIZE;\n"
        "           \n"
        "           // Check if we reached the player\n"
        "           if (length(pos - playerPos) < 6.0) {\n"
        "               if (!inShadow) {\n"
        "                   softShadow = 1.0;\n"
        "               }\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Convert back to texture coordinates\n"
        "           vec2 texPos = pos / worldSize;\n"
        "           \n"
        "           // Check if outside texture bounds\n"
        "           if (texPos.x < 0.0 || texPos.x > 1.0 || texPos.y < 0.0 || texPos.y > 1.0) {\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Sample with slight noise for varied shadows\n"
        "           float jitter = hash(pos * 0.1 + gameTime) * 0.5;\n"
        "           vec2 jitteredPos = texPos + vec2(jitter, jitter) * 0.001;\n"
        "           \n"
        "           // Check if we hit a solid object\n"
        "           float alpha = texture(worldTexture, jitteredPos).a;\n"
        "           if (alpha > 0.7) {\n"
        "               // Hit something solid, in shadow\n"
        "               float penumbra = rayLength / 300.0; // Wider penumbra over distance\n"
        "               softShadow = max(softShadow, noise(pos * 0.1) * penumbra);\n"
        "               inShadow = true;\n"
        "           }\n"
        "       }\n"
        "       \n"
        "       shadowFactor = softShadow;\n"
        "   }\n"
        "   \n"
        "   // Add distance-based falloff (torch-like light)\n"
        "   float torchRadius = 200.0;\n"
        "   float distFalloff = 1.0 / (1.0 + pow(playerDist/torchRadius, 2.5));\n"
        "   \n"
        "   // Directional light from the sun (only affects surface areas)\n"
        "   float sunContribution = 0.0;\n"
        "   if (TexCoord.y < 0.4) { // Only near surface\n"
        "       float surfaceDepth = TexCoord.y / 0.4; // 0 at top, 1 at depth cutoff\n"
        "       \n"
        "       // Cast ray toward sun to check for shadows from terrain\n"
        "       vec2 toSun = normalize(vec2(0.0, -1.0)); // Sun rays from top\n"
        "       vec2 sunCheckPos = worldPos;\n"
        "       bool inSunShadow = false;\n"
        "       \n"
        "       // Shorter ray for sun shadow (doesn't need to go as far)\n"
        "       for (int i = 0; i < 24; i++) {\n"
        "           sunCheckPos += toSun * 4.0;\n"
        "           vec2 sunTexPos = sunCheckPos / worldSize;\n"
        "           \n"
        "           // Check if outside world bounds or reached top\n"
        "           if (sunTexPos.y <= 0.0) {\n"
        "               // Reached sky, not in shadow\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           if (sunTexPos.x < 0.0 || sunTexPos.x > 1.0 || sunTexPos.y > 1.0) {\n"
        "               break;\n"
        "           }\n"
        "           \n"
        "           // Check if we hit terrain that blocks sun\n"
        "           if (texture(worldTexture, sunTexPos).a > 0.7) {\n"
        "               inSunShadow = true;\n"
        "               break;\n"
        "           }\n"
        "       }\n"
        "       \n"
        "       // Calculate sun contribution based on day phase and shadow\n"
        "       if (!inSunShadow) {\n"
        "           sunContribution = dayPhase * (1.0 - surfaceDepth) * 0.8;\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Water caustics effect when underwater and near surface\n"
        "   float caustics = 0.0;\n"
        "   if (isWater) {\n"
        "       // Calculate based on nearby water surfaces\n"
        "       float causticsNoise = noise(worldPos * 0.05 + vec2(gameTime * 0.1, 0));\n"
        "       caustics = pow(causticsNoise, 2.0) * 0.3 * dayPhase;\n"
        "   }\n"
        "   \n"
        "   // Add contribution from emissive materials with glow radius\n"
        "   float emissiveStrength = length(emissive.rgb);\n"
        "   float emissiveContribution = emissiveStrength;\n"
        "   \n"
        "   // Add emissive glow radius effect\n"
        "   if (emissiveStrength < 0.01) {\n"
        "       // If current pixel isn't emissive, check for nearby emissive sources\n"
        "       const int GLOW_SAMPLES = 16;\n"
        "       const float GLOW_RADIUS = 0.03;\n"
        "       \n"
        "       for (int i = 0; i < GLOW_SAMPLES; i++) {\n"
        "           float angle = float(i) / float(GLOW_SAMPLES) * 6.28;\n"
        "           float sampleDist = hash(vec2(angle, gameTime)) * GLOW_RADIUS;\n"
        "           vec2 offset = vec2(cos(angle), sin(angle)) * sampleDist;\n"
        "           vec2 samplePos = TexCoord + offset;\n"
        "           \n"
        "           vec4 sampleEmissive = texture(emissiveTexture, samplePos);\n"
        "           float sampleStrength = length(sampleEmissive.rgb);\n"
        "           \n"
        "           if (sampleStrength > 0.1) {\n"
        "               float glowFalloff = 1.0 - sampleDist/GLOW_RADIUS;\n"
        "               emissiveContribution = max(emissiveContribution, sampleStrength * glowFalloff * 0.4);\n"
        "           }\n"
        "       }\n"
        "   }\n"
        "   \n"
        "   // Calculate refracted light through water (realistic underwater light shafts)\n"
        "   float waterShafts = 0.0;\n"
        "   if (isWater && TexCoord.y > 0.1 && TexCoord.y < 0.45) {\n"
        "       // Calculate shaft intensity based on depth and noise\n"
        "       float depthFactor = 1.0 - (TexCoord.y - 0.1) / 0.35;\n"
        "       float shaftNoise = noise(worldPos * 0.01 + vec2(0, gameTime * 0.02));\n"
        "       waterShafts = shaftNoise * depthFactor * dayPhase * 0.2;\n"
        "   }\n"
        "   \n"
        "   // Calculate final lighting\n"
        "   float playerLight = shadowFactor * distFalloff * playerLightIntensity;\n"
        "   float totalLighting = ambientBase + ambientNoise - ao + playerLight + sunContribution + caustics + waterShafts + caveAtmosphere + timeBlink;\n"
        "   \n"
        "   // Add emissive sources after ambient/shadows\n"
        "   totalLighting = max(totalLighting, emissiveContribution);\n"
        "   \n"
        "   // Apply water light attenuation (deeper = darker/bluer)\n"
        "   if (isWater) {\n"
        "       // Underwater lighting is more strongly affected by depth\n"
        "       float waterDepthAttenuation = max(0.2, 1.0 - TexCoord.y * 3.0);\n"
        "       totalLighting *= waterDepthAttenuation;\n"
        "       \n"
        "       // Add blue tint to final lighting for underwater effect\n"
        "       totalLighting = max(0.1, min(1.0, totalLighting));\n"
        "       FragColor = vec4(totalLighting * 0.7, totalLighting * 0.8, totalLighting, 1.0);\n"
        "       return;\n"
        "   }\n"
        "   \n"
        "   // Output shadow map (1.0 = fully lit, 0.0 = in shadow)\n"
        "   totalLighting = clamp(totalLighting, 0.0, 1.0);\n"
        "   FragColor = vec4(vec3(totalLighting), 1.0);\n"
        "}\n";
    
    // Enhanced bloom effect shader with two-pass blur
    const char* bloomFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D emissiveTexture;\n"
        "uniform float gameTime;\n"
        
        "// Helper function for dynamic bloom intensity\n"
        "float hash(vec2 p) {\n"
        "   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   // Get base emissive color\n"
        "   vec4 baseEmission = texture(emissiveTexture, TexCoord);\n"
        "   float luminance = dot(baseEmission.rgb, vec3(0.299, 0.587, 0.114));\n"
        "   \n"
        "   // Apply threshold to isolate bright areas\n"
        "   float threshold = 0.4;\n"
        "   float softThreshold = 0.1;\n"
        "   float brightness = max(0.0, luminance - threshold) / softThreshold;\n"
        "   brightness = min(brightness, 1.0);\n"
        "   vec4 brightColor = baseEmission * brightness;\n"
        "   \n"
        "   // Skip further processing if not bright enough\n"
        "   if (brightness <= 0.0) {\n"
        "       FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
        "       return;\n"
        "   }\n"
        "   \n"
        "   // Two-pass Gaussian blur for better performance and quality\n"
        "   // First pass: horizontal blur\n"
        "   vec4 hBlurColor = vec4(0.0);\n"
        "   float hTotalWeight = 0.0;\n"
        "   \n"
        "   // Dynamic bloom radius (changes with emissive brightness)\n"
        "   float baseBloomRadius = 0.02;\n"
        "   float bloomRadiusMultiplier = 1.0 + luminance * 0.5;\n"
        "   float bloomRadius = baseBloomRadius * bloomRadiusMultiplier;\n"
        "   \n"
        "   // Add subtle animation to bloom radius\n"
        "   float timeVariation = 1.0 + 0.1 * sin(gameTime * 2.0 + hash(TexCoord) * 6.28);\n"
        "   bloomRadius *= timeVariation;\n"
        "   \n"
        "   // First pass: horizontal blur with variable sampling\n"
        "   const int samplesH = 24;\n"
        "   for (int x = -samplesH/2; x < samplesH/2; x++) {\n"
        "       float offset = float(x) * bloomRadius / float(samplesH);\n"
        "       float weight = exp(-offset * offset * 4.0);\n"
        "       vec2 sampleCoord = TexCoord + vec2(offset, 0.0);\n"
        "       \n"
        "       // Apply color-based weighting (brighter pixels contribute more)\n"
        "       vec4 sampleColor = texture(emissiveTexture, sampleCoord);\n"
        "       float sampleLum = dot(sampleColor.rgb, vec3(0.299, 0.587, 0.114));\n"
        "       float lumWeight = max(0.2, sampleLum);\n"
        "       \n"
        "       hBlurColor += sampleColor * weight * lumWeight;\n"
        "       hTotalWeight += weight * lumWeight;\n"
        "   }\n"
        "   vec4 horizontalBlur = hBlurColor / max(0.001, hTotalWeight);\n"
        "   \n"
        "   // Second pass: vertical blur on the result of horizontal blur\n"
        "   vec4 vBlurColor = vec4(0.0);\n"
        "   float vTotalWeight = 0.0;\n"
        "   const int samplesV = 24;\n"
        "   \n"
        "   // Apply vertical sampling - simulate vertical blur pass\n"
        "   for (int y = -samplesV/2; y < samplesV/2; y++) {\n"
        "       float offset = float(y) * bloomRadius / float(samplesV);\n"
        "       float weight = exp(-offset * offset * 4.0);\n"
        "       vec2 sampleCoord = TexCoord + vec2(0.0, offset);\n"
        "       \n"
        "       // Use the horizontalBlur as input for vertical blur\n"
        "       // In a real two-pass implementation, we'd read from the horizontal blur texture\n"
        "       // But since we're simulating it in a single pass, we recompute the horizontal blur\n"
        "       vec4 hBlur = vec4(0.0);\n"
        "       float hWeight = 0.0;\n"
        "       \n"
        "       for (int x = -samplesH/4; x < samplesH/4; x++) { // Use fewer samples for performance\n"
        "           float h_offset = float(x) * bloomRadius / float(samplesH/2);\n"
        "           float h_weight = exp(-h_offset * h_offset * 4.0);\n"
        "           vec2 h_sampleCoord = sampleCoord + vec2(h_offset, 0.0);\n"
        "           hBlur += texture(emissiveTexture, h_sampleCoord) * h_weight;\n"
        "           hWeight += h_weight;\n"
        "       }\n"
        "       \n"
        "       hBlur = hBlur / max(0.001, hWeight);\n"
        "       vBlurColor += hBlur * weight;\n"
        "       vTotalWeight += weight;\n"
        "   }\n"
        "   \n"
        "   // Normalize the vertical blur result\n"
        "   vec4 finalBlur = vBlurColor / max(0.001, vTotalWeight);\n"
        "   \n"
        "   // Apply color adjustments to bloom - can make fire more orange, lights more yellow, etc.\n"
        "   float colorShift = hash(TexCoord + gameTime * 0.01) * 0.1;\n"
        "   vec3 warmShift = vec3(1.0 + colorShift, 1.0, 1.0 - colorShift * 0.5);\n"
        "   \n"
        "   // Enhance bloom based on material type (can be expanded further)\n"
        "   vec3 bloomColor = finalBlur.rgb;\n"
        "   \n"
        "   // Detect if it's likely fire/lava (reddish-yellow)\n"
        "   if (baseEmission.r > baseEmission.g && baseEmission.g > baseEmission.b) {\n"
        "       // Fire-like bloom enhancement (warmer, more orange)\n"
        "       bloomColor *= warmShift * 1.2;\n"
        "   }\n"
        "   \n"
        "   // Output final bloom with increased intensity\n"
        "   FragColor = vec4(bloomColor * 1.5, finalBlur.a);\n"
        "}\n";
    
    // Final post-processing shader with advanced effects
    const char* postProcessingFragmentShader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D colorTexture;\n"
        "uniform sampler2D bloomTexture;\n"
        "uniform sampler2D shadowMapTexture;\n"
        "uniform float gameTime;\n"
        
        "// Advanced noise helpers\n"
        "float hash13(vec3 p3) {\n"
        "   p3 = fract(p3 * 0.1031);\n"
        "   p3 += dot(p3, p3.yzx + 33.33);\n"
        "   return fract((p3.x + p3.y) * p3.z);\n"
        "}\n"
        
        "float simplex_noise(vec3 p) {\n"
        "   float n = hash13(floor(p));\n"
        "   p = fract(p);\n"
        "   return n * 2.0 - 1.0;\n"
        "}\n"
        
        "// Advanced tone mapping (ACES filmic curve approximation)\n"
        "vec3 ACESToneMapping(vec3 color, float adapted_lum) {\n"
        "   const float A = 2.51;\n"
        "   const float B = 0.03;\n"
        "   const float C = 2.43;\n"
        "   const float D = 0.59;\n"
        "   const float E = 0.14;\n"
        "   color *= adapted_lum;\n"
        "   return (color * (A * color + B)) / (color * (C * color + D) + E);\n"
        "}\n"
        
        "vec3 adjustContrast(vec3 color, float contrast) {\n"
        "   return 0.5 + (contrast * (color - 0.5));\n"
        "}\n"
        
        "vec3 adjustSaturation(vec3 color, float saturation) {\n"
        "   const vec3 luminance = vec3(0.2126, 0.7152, 0.0722);\n"
        "   float grey = dot(color, luminance);\n"
        "   return mix(vec3(grey), color, saturation);\n"
        "}\n"
        
        "vec3 adjustExposure(vec3 color, float exposure) {\n"
        "   return 1.0 - exp(-color * exposure);\n"
        "}\n"
        
        "// Dynamic vignette that changes with light conditions\n"
        "vec3 dynamicVignette(vec3 color, vec2 uv, float lightLevel) {\n"
        "   // Make vignette stronger in dark areas, weaker in well-lit areas\n"
        "   float baseVigStrength = 1.2 - lightLevel * 0.8;\n"
        "   float vigStrength = clamp(baseVigStrength, 0.3, 1.2);\n"
        "   \n"
        "   // Dynamic vignette shape - depth affects shape\n"
        "   float aspect = 1.77; // Assuming 16:9 aspect ratio\n"
        "   vec2 vignetteCenter = vec2(0.5, 0.5);\n"
        "   vec2 coord = (uv - vignetteCenter) * vec2(aspect, 1.0);\n"
        "   \n"
        "   // Non-uniform vignette (stronger at bottom)\n"
        "   float vigShape = length(coord) + abs(coord.y) * 0.2;\n"
        "   float vignette = smoothstep(0.0, 1.4, 1.0 - vigShape * vigStrength);\n"
        "   \n"
        "   // Blue tint in shadows for more atmospheric look\n"
        "   vec3 shadowTint = vec3(0.9, 0.95, 1.05);\n"
        "   vec3 vignetteColor = mix(color * shadowTint, color, vignette);\n"
        "   \n"
        "   return vignetteColor;\n"
        "}\n"
        
        "// Chromatic aberration effect - stronger at screen edges\n"
        "vec3 chromaticAberration(sampler2D tex, vec2 uv, float strength) {\n"
        "   // Distance from center affects aberration strength\n"
        "   float dist = length(uv - 0.5);\n"
        "   float distPower = pow(dist, 1.5) * 2.0;\n"
        "   float aberrationStrength = strength * distPower;\n"
        "   \n"
        "   // Direction from center\n"
        "   vec2 dir = normalize(uv - 0.5);\n"
        "   \n"
        "   // Sample with color channel offsets\n"
        "   vec3 result;\n"
        "   result.r = texture(tex, uv - dir * aberrationStrength).r;\n"
        "   result.g = texture(tex, uv).g;\n"
        "   result.b = texture(tex, uv + dir * aberrationStrength).b;\n"
        "   \n"
        "   return result;\n"
        "}\n"
        
        "// Dynamic film grain that's stronger in darker areas\n"
        "float dynamicFilmGrain(vec2 uv, float time, float luminance) {\n"
        "   // More grain in shadows, less in highlights\n"
        "   float grainStrength = mix(0.08, 0.01, luminance);\n"
        "   \n"
        "   // Animated noise\n"
        "   float noise = simplex_noise(vec3(uv * 500.0, time * 0.5));\n"
        "   return noise * grainStrength;\n"
        "}\n"
        
        "// Light leaks and flares for more cinematic look\n"
        "vec3 lightLeaks(vec2 uv, float time) {\n"
        "   // Slow moving light leak pattern\n"
        "   vec2 leakUV = uv * vec2(0.7, 0.4) + vec2(time * 0.01, time * 0.005);\n"
        "   float leak = pow(simplex_noise(vec3(leakUV, time * 0.1)) * 0.5 + 0.5, 2.0);\n"
        "   \n"
        "   // Light leak is stronger at screen edges\n"
        "   float edgeMask = 1.0 - smoothstep(0.7, 1.0, distance(uv, vec2(0.5)));\n"
        "   leak *= (1.0 - edgeMask) * 0.3;\n"
        "   \n"
        "   // Warm colored light leak\n"
        "   return vec3(leak * 1.0, leak * 0.7, leak * 0.5);\n"
        "}\n"
        
        "// Color grading LUT simulation\n"
        "vec3 colorGrade(vec3 color) {\n"
        "   // Cinematic color grading - slightly teal shadows, golden highlights\n"
        "   vec3 shadows = vec3(0.9, 1.05, 1.1);  // Slightly blue-green shadows\n"
        "   vec3 midtones = vec3(1.0, 1.0, 1.0);  // Neutral midtones\n"
        "   vec3 highlights = vec3(1.1, 1.05, 0.9); // Golden/warm highlights\n"
        "   \n"
        "   // Apply color grading based on luminance\n"
        "   float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
        "   return color * mix(mix(shadows, midtones, smoothstep(0.0, 0.4, luminance)),\n"
        "                      highlights, smoothstep(0.4, 1.0, luminance));\n"
        "}\n"
        
        "void main()\n"
        "{\n"
        "   // Get base colors with chromatic aberration\n"
        "   vec3 baseColorRGB = chromaticAberration(colorTexture, TexCoord, 0.005);\n"
        "   float baseAlpha = texture(colorTexture, TexCoord).a;\n"
        "   vec4 baseColor = vec4(baseColorRGB, baseAlpha);\n"
        "   vec4 bloom = texture(bloomTexture, TexCoord);\n"
        "   \n"
        "   // Get shadow information, potentially with color (for underwater etc.)\n"
        "   vec4 shadowInfo = texture(shadowMapTexture, TexCoord);\n"
        "   float shadow = shadowInfo.r; // Main shadow/lighting value\n"
        "   \n"
        "   // Check if this is likely water by examining shadow's color channels\n"
        "   bool isLikelyWater = (shadowInfo.b > shadowInfo.r && shadowInfo.g > shadowInfo.r && shadowInfo.a > 0.5);\n"
        "   \n"
        "   // Apply shadowmap to base color\n"
        "   vec3 litColor;\n"
        "   if (isLikelyWater) {\n"
        "       // Special underwater coloring (maintain the blue tint from shadow shader)\n"
        "       litColor = baseColor.rgb * shadowInfo.rgb;\n"
        "   } else {\n"
        "       litColor = baseColor.rgb * (shadow * 0.85 + 0.15);\n" // 0.15 is ambient light\n"
        "   }\n"
        "   \n"
        "   // Get overall scene luminance for effects that depend on light level\n"
        "   float sceneLuminance = shadow;\n"
        "   \n"
        "   // Add bloom effect with dynamic intensity\n"
        "   // More bloom in dark areas for dramatic effect\n"
        "   float bloomIntensity = 2.0 - sceneLuminance * 0.8;\n"
        "   vec3 finalColor = litColor + bloom.rgb * bloomIntensity;\n"
        "   \n"
        "   // Add subtle light leaks in well-lit areas\n"
        "   if (sceneLuminance > 0.5) {\n"
        "       finalColor += lightLeaks(TexCoord, gameTime) * sceneLuminance;\n"
        "   }\n"
        "   \n"
        "   // Advanced tonemapping - preserve colors better\n"
        "   float exposure = 1.0 + sin(gameTime * 0.1) * 0.1; // Subtle exposure fluctuation\n"
        "   finalColor = ACESToneMapping(finalColor, exposure);\n"
        "   \n"
        "   // Color grading for cinematic look\n"
        "   finalColor = colorGrade(finalColor);\n"
        "   \n"
        "   // Apply dynamic contrast based on scene luminance\n"
        "   float contrastAmount = 1.2 - sceneLuminance * 0.2; // More contrast in dark scenes\n"
        "   finalColor = adjustContrast(finalColor, contrastAmount);\n"
        "   \n"
        "   // Saturation adjustment (more saturated in well-lit areas)\n"
        "   float saturationAmount = 1.0 + sceneLuminance * 0.2;\n"
        "   finalColor = adjustSaturation(finalColor, saturationAmount);\n"
        "   \n"
        "   // Apply dynamic vignette based on light levels\n"
        "   finalColor = dynamicVignette(finalColor, TexCoord, sceneLuminance);\n"
        "   \n"
        "   // Dynamic film grain - stronger in darker areas\n"
        "   finalColor += dynamicFilmGrain(TexCoord, gameTime, sceneLuminance);\n"
        "   \n"
        "   // Apply a subtle animated noise to very dark areas to simulate film noise\n"
        "   float darkAreaNoise = 0.0;\n"
        "   if (sceneLuminance < 0.2) {\n"
        "       darkAreaNoise = simplex_noise(vec3(TexCoord * 200.0, gameTime)) * 0.03 * (1.0 - sceneLuminance * 5.0);\n"
        "       finalColor += vec3(darkAreaNoise);\n"
        "   }\n"
        "   \n"
        "   // Ensure sky/empty areas aren't affected by certain effects\n"
        "   if (baseColor.a < 0.01) {\n"
        "       // Just recover the original color without grain and other effects\n"
        "       finalColor = baseColorRGB + bloom.rgb * 1.2;\n"
        "       finalColor = ACESToneMapping(finalColor, exposure);\n"
        "   }\n"
        "   \n"
        "   // Output final color with original alpha\n"
        "   FragColor = vec4(finalColor, baseColor.a);\n"
        "}\n";
    
    // Create shader programs
    m_shaderProgram = createShaderProgram(commonVertexShader, mainFragmentShader);
    m_shadowShader = createShaderProgram(commonVertexShader, shadowFragmentShader);
    m_volumetricLightShader = createShaderProgram(commonVertexShader, volumetricLightFragmentShader);
    m_bloomShader = createShaderProgram(commonVertexShader, bloomFragmentShader);
    m_postProcessShader = createShaderProgram(commonVertexShader, postProcessingFragmentShader);
    
    // Check if all shaders compiled successfully
    return (m_shaderProgram != 0 && m_shadowShader != 0 && m_volumetricLightShader != 0 && 
            m_bloomShader != 0 && m_postProcessShader != 0);
}

bool Renderer::createTexture(int /*width*/, int /*height*/) {
    // We're not using textures in this simple renderer
    return true;
}

GLuint Renderer::compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check for compilation errors
    GLint success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

void Renderer::createFramebuffers() {
    // Create main framebuffer for scene rendering
    glGenFramebuffers(1, &m_mainFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_mainFBO);
    
    // Create main color texture
    glGenTextures(1, &m_mainColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mainColorTexture, 0);
    
    // Create emissive texture (for bloom)
    glGenTextures(1, &m_mainEmissiveTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainEmissiveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_mainEmissiveTexture, 0);
    
    // Create depth texture for lighting calculations
    glGenTextures(1, &m_mainDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_mainDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_screenWidth, m_screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_mainDepthTexture, 0);
    
    // Setup multiple render targets
    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    
    // Create shadow map framebuffer
    glGenFramebuffers(1, &m_shadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    
    // Create shadow map texture
    glGenTextures(1, &m_shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_screenWidth, m_screenHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_shadowMapTexture, 0);
    
    // Create volumetric lighting framebuffer
    glGenFramebuffers(1, &m_volumetricLightFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_volumetricLightFBO);
    
    // Create volumetric lighting texture - using RGBA16F for HDR lighting
    glGenTextures(1, &m_volumetricLightTexture);
    glBindTexture(GL_TEXTURE_2D, m_volumetricLightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_volumetricLightTexture, 0);
    
    // Create bloom effect framebuffer
    glGenFramebuffers(1, &m_bloomFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO);
    
    // Create bloom texture
    glGenTextures(1, &m_bloomTexture);
    glBindTexture(GL_TEXTURE_2D, m_bloomTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTexture, 0);
    
    // Reset to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Check for errors
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer creation failed with status: " << status << std::endl;
    }
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