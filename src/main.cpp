#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>
#include <random>
#include <chrono>

// GLEW must be included before any OpenGL headers
#include <GL/glew.h>
#include <GL/gl.h>

#include "../include/Materials.h"
#include "../include/World.h"
#include "../include/Renderer.h"

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WORLD_WIDTH = 1200;   // Increased world size for panning/zooming
const int WORLD_HEIGHT = 900;   // Increased world size for panning/zooming
const int TARGET_FPS = 60;
const int FRAME_DELAY = 1000 / TARGET_FPS;

int main(int /*argc*/, char* /*argv*/[]) {

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set OpenGL attributes
    // Request legacy OpenGL 2.1 for immediate mode to be available
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    std::cout << "OpenGL attributes set to 2.1 compatibility mode" << std::endl;

    // Create window (windowed by default, will toggle fullscreen via UI)
    SDL_Window* window = SDL_CreateWindow(
        "PixelPhys2D",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Start in fullscreen mode
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    // Get the actual window size after fullscreen
    int actualWidth, actualHeight;
    SDL_GetWindowSize(window, &actualWidth, &actualHeight);

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize GLEW
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "Error initializing GLEW! " << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Print OpenGL info
    std::cout << "OpenGL vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // Set up vsync
    SDL_GL_SetSwapInterval(1);
    
    // Show the system cursor instead of using a custom one
    SDL_ShowCursor(SDL_ENABLE);
    
    // Create the pixel physics world
    PixelPhys::World world(WORLD_WIDTH, WORLD_HEIGHT);
    
    // Generate initial terrain with a random seed
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    world.generate(seed);
    
    // Create the renderer with the actual window size
    PixelPhys::Renderer renderer(actualWidth, actualHeight);
    if (!renderer.initialize()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Force a short delay to allow proper initialization of OpenGL resources
    SDL_Delay(100);

    // Main loop flag
    bool quit = false;

    // Event handler
    SDL_Event e;

    // For timing
    Uint32 frameStart;
    int frameTime;
    
    // For FPS calculation
    int frameCount = 0;
    Uint32 fpsTimer = SDL_GetTicks();
    
    std::cout << "Controls:" << std::endl;
    std::cout << "Left mouse: Place Sand" << std::endl;
    std::cout << "Right mouse: Erase (place Empty)" << std::endl;
    std::cout << "F11: Toggle fullscreen mode" << std::endl;
    std::cout << "R: Reset world with a new seed" << std::endl;
    std::cout << "ESC: Quit" << std::endl;

    // Camera state (no zoom/pan initially)
    float zoomLevel = 1.0f; 
    int cameraX = 0;
    int cameraY = 0;
    
    // Default settings
    PixelPhys::MaterialType currentMaterial = PixelPhys::MaterialType::Sand;
    int brushSize = 5;
    bool simulationRunning = true;

    // Main loop
    while (!quit) {
        frameStart = SDL_GetTicks();

        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                } else if (e.key.keysym.sym == SDLK_r) {
                    // Reset world with a new seed
                    seed = static_cast<unsigned int>(std::time(nullptr));
                    world.generate(seed);
                } else if (e.key.keysym.sym == SDLK_F11) {
                    // Toggle fullscreen
                    Uint32 flags = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, flags ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    
                    // Update with new window size
                    int newWidth, newHeight;
                    SDL_GetWindowSize(window, &newWidth, &newHeight);
                    
                    // Update actual width and height variables for the current frame
                    actualWidth = newWidth;
                    actualHeight = newHeight;
                    
                    // Create a new orthographic projection based on the new window size
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glOrtho(0, actualWidth, actualHeight, 0, -1, 1);
                    
                    // Reset the modelview matrix
                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    
                    // Debug output for new dimensions
                    std::cout << "Window resized to " << actualWidth << "x" << actualHeight << std::endl;
                } else if (e.key.keysym.sym == SDLK_SPACE) {
                    // Toggle simulation
                    simulationRunning = !simulationRunning;
                } else if (e.key.keysym.sym == SDLK_1) {
                    currentMaterial = PixelPhys::MaterialType::Sand;
                    std::cout << "Selected material: Sand" << std::endl;
                } else if (e.key.keysym.sym == SDLK_2) {
                    currentMaterial = PixelPhys::MaterialType::Water;
                    std::cout << "Selected material: Water" << std::endl;
                } else if (e.key.keysym.sym == SDLK_3) {
                    currentMaterial = PixelPhys::MaterialType::Stone;
                    std::cout << "Selected material: Stone" << std::endl;
                } else if (e.key.keysym.sym == SDLK_4) {
                    currentMaterial = PixelPhys::MaterialType::Wood;
                    std::cout << "Selected material: Wood" << std::endl;
                } else if (e.key.keysym.sym == SDLK_5) {
                    currentMaterial = PixelPhys::MaterialType::Fire;
                    std::cout << "Selected material: Fire" << std::endl;
                } else if (e.key.keysym.sym == SDLK_6) {
                    currentMaterial = PixelPhys::MaterialType::Oil;
                    std::cout << "Selected material: Oil" << std::endl;
                }
            } else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Window has been resized
                    int newWidth = e.window.data1;
                    int newHeight = e.window.data2;
                    
                    // Update actual width and height variables for the current frame
                    actualWidth = newWidth;
                    actualHeight = newHeight;
                    
                    // Create a new orthographic projection based on the new window size
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glOrtho(0, actualWidth, actualHeight, 0, -1, 1);
                    
                    // Reset the modelview matrix
                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    
                    // Debug output for manual resize
                    std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                // Zoom in/out with mouse wheel
                float zoomDelta = e.wheel.y > 0 ? 0.1f : -0.1f;
                zoomLevel = std::max(0.1f, std::min(zoomLevel + zoomDelta, 10.0f));
                std::cout << "Zoom level: " << zoomLevel << std::endl;
            } else if (e.type == SDL_MOUSEMOTION && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE))) {
                // Pan with middle mouse button
                cameraX -= e.motion.xrel;
                cameraY -= e.motion.yrel;
                
                // Limit camera position to prevent moving too far from world
                cameraX = std::max(-WORLD_WIDTH/2, std::min(cameraX, WORLD_WIDTH/2));
                cameraY = std::max(-WORLD_HEIGHT/2, std::min(cameraY, WORLD_HEIGHT/2));
            }
        }
        
        // Handle mouse interactions for placing/erasing materials
        int mouseX, mouseY;
        Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        
        // Simple view rectangle - use the full window with small margin
        SDL_Rect viewRect;
        const int margin = 10;
        viewRect.x = margin;
        viewRect.y = margin;
        viewRect.w = actualWidth - 2 * margin;
        viewRect.h = actualHeight - 2 * margin;
        
        // Check if mouse is inside the view area
        bool canEdit = (mouseX >= viewRect.x && mouseX < viewRect.x + viewRect.w &&
                       mouseY >= viewRect.y && mouseY < viewRect.y + viewRect.h);
        
        if (canEdit) {
            // Simple coordinate mapping
            
            // 1. Find position within view as 0.0-1.0 percentage
            float viewPercentX = (mouseX - viewRect.x) / static_cast<float>(viewRect.w);
            float viewPercentY = (mouseY - viewRect.y) / static_cast<float>(viewRect.h);
            
            // 2. Calculate visible world area based on zoom
            float visibleWorldWidth = WORLD_WIDTH / zoomLevel;
            float visibleWorldHeight = WORLD_HEIGHT / zoomLevel;
            
            // 3. Find the top-left corner of visible world area
            float worldStartX = WORLD_WIDTH/2.0f + cameraX - visibleWorldWidth/2.0f;
            float worldStartY = WORLD_HEIGHT/2.0f + cameraY - visibleWorldHeight/2.0f;
            
            // 4. Map screen position directly to world coordinates
            int worldMouseX = static_cast<int>(worldStartX + viewPercentX * visibleWorldWidth);
            int worldMouseY = static_cast<int>(worldStartY + viewPercentY * visibleWorldHeight);
            
            // Ensure coordinates are within the world bounds
            worldMouseX = std::max(0, std::min(worldMouseX, WORLD_WIDTH - 1));
            worldMouseY = std::max(0, std::min(worldMouseY, WORLD_HEIGHT - 1));
            
            // Debug output only when needed
            /*
            std::cout << "Mouse screen: " << mouseX << "," << mouseY 
                     << " | World: " << worldMouseX << "," << worldMouseY 
                     << " | Zoom: " << zoomLevel << std::endl;
            */
            
            // Left mouse button places the current material
            if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                // Only log occasionally to reduce console spam
                if (frameCount % 10 == 0) {
                    std::cout << "Placing " << static_cast<int>(currentMaterial) << " at: " 
                              << worldMouseX << "," << worldMouseY << std::endl;
                }
                
                // Place material in a circular brush area
                for (int y = -brushSize; y <= brushSize; ++y) {
                    for (int x = -brushSize; x <= brushSize; ++x) {
                        if (x*x + y*y <= brushSize*brushSize) { // Circular brush
                            int px = worldMouseX + x;
                            int py = worldMouseY + y;
                            
                            // Make sure we're in bounds of the world
                            if (px >= 0 && px < WORLD_WIDTH && py >= 0 && py < WORLD_HEIGHT) {
                                world.set(px, py, currentMaterial);
                            }
                        }
                    }
                }
            }
            
            // Right mouse button always erases
            if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                // Only log occasionally to reduce console spam
                if (frameCount % 10 == 0) {
                    std::cout << "Erasing at: " << worldMouseX << "," << worldMouseY << std::endl;
                }
                
                // Erase in a circular brush area
                for (int y = -brushSize; y <= brushSize; ++y) {
                    for (int x = -brushSize; x <= brushSize; ++x) {
                        if (x*x + y*y <= brushSize*brushSize) { // Circular brush
                            int px = worldMouseX + x;
                            int py = worldMouseY + y;
                            
                            // Make sure we're in bounds of the world
                            if (px >= 0 && px < WORLD_WIDTH && py >= 0 && py < WORLD_HEIGHT) {
                                world.set(px, py, PixelPhys::MaterialType::Empty);
                            }
                        }
                    }
                }
            }
        }

        // Update world physics if simulation is running
        if (simulationRunning) {
            world.update();
        }

        // Clear the screen with a gray background
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Reset OpenGL state
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Setup 2D orthographic projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, actualWidth, actualHeight, 0, -1, 1);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Draw a border around the view area
        glColor3f(0.4f, 0.4f, 0.4f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(viewRect.x, viewRect.y);
        glVertex2f(viewRect.x + viewRect.w, viewRect.y);
        glVertex2f(viewRect.x + viewRect.w, viewRect.y + viewRect.h);
        glVertex2f(viewRect.x, viewRect.y + viewRect.h);
        glEnd();
        
        // Render the world
        glPushMatrix();
        
        // Setup transformation to match the cursor mapping
        glTranslatef(viewRect.x, viewRect.y, 0.0f);
        glScalef(viewRect.w / (WORLD_WIDTH / zoomLevel), viewRect.h / (WORLD_HEIGHT / zoomLevel), 1.0f);
        glTranslatef(-(WORLD_WIDTH/2.0f + cameraX - (WORLD_WIDTH/zoomLevel)/2.0f), 
                     -(WORLD_HEIGHT/2.0f + cameraY - (WORLD_HEIGHT/zoomLevel)/2.0f), 0.0f);
        
        // Batch render the world - render each material type separately
        
        // For each material type
        for (int materialIndex = 1; materialIndex < static_cast<int>(PixelPhys::MaterialType::COUNT); materialIndex++) {
            PixelPhys::MaterialType currentMat = static_cast<PixelPhys::MaterialType>(materialIndex);
            
            // Set material color
            float r = 0.0f, g = 0.0f, b = 0.0f;
            switch (currentMat) {
                case PixelPhys::MaterialType::Sand:
                    r = 0.9f; g = 0.8f; b = 0.4f; // Yellow-ish
                    break;
                case PixelPhys::MaterialType::Water:
                    r = 0.2f; g = 0.4f; b = 0.8f; // Blue
                    break;
                case PixelPhys::MaterialType::Stone:
                    r = 0.5f; g = 0.5f; b = 0.5f; // Gray
                    break;
                case PixelPhys::MaterialType::Wood:
                    r = 0.6f; g = 0.4f; b = 0.2f; // Brown
                    break;
                case PixelPhys::MaterialType::Fire:
                    r = 1.0f; g = 0.3f; b = 0.0f; // Orange-red
                    break;
                case PixelPhys::MaterialType::Oil:
                    r = 0.2f; g = 0.1f; b = 0.1f; // Dark brown
                    break;
                default:
                    continue; // Skip empty material
            }
            
            // Set the color for this material
            glColor3f(r, g, b);
            
            // Make pixels look larger to reduce gaps and improve performance
        glPointSize(3.0f);
            
        // Use GL_POINTS for rendering, but batch by chunk for better performance
        glBegin(GL_POINTS);
        
        // Calculate visible area in world coordinates based on camera position and zoom
        float visibleWorldWidth = WORLD_WIDTH / zoomLevel;
        float visibleWorldHeight = WORLD_HEIGHT / zoomLevel;
        float worldStartX = WORLD_WIDTH/2.0f + cameraX - visibleWorldWidth/2.0f;
        float worldStartY = WORLD_HEIGHT/2.0f + cameraY - visibleWorldHeight/2.0f;
        
        // Determine chunk coordinates for the visible area
        int startChunkX = std::max(0, static_cast<int>(worldStartX) / world.getChunkWidth());
        int startChunkY = std::max(0, static_cast<int>(worldStartY) / world.getChunkHeight());
        int endChunkX = std::min(world.getChunksX() - 1, static_cast<int>((worldStartX + visibleWorldWidth) / world.getChunkWidth()));
        int endChunkY = std::min(world.getChunksY() - 1, static_cast<int>((worldStartY + visibleWorldHeight) / world.getChunkHeight()));
        
        // Calculate visible bounds in world coordinates for clipping
        int visStartX = std::max(0, static_cast<int>(worldStartX - 5)); // Add a small margin
        int visStartY = std::max(0, static_cast<int>(worldStartY - 5));
        int visEndX = std::min(WORLD_WIDTH - 1, static_cast<int>(worldStartX + visibleWorldWidth + 5));
        int visEndY = std::min(WORLD_HEIGHT - 1, static_cast<int>(worldStartY + visibleWorldHeight + 5));
        
        // Only render chunks in the visible area
        for (int chunkY = startChunkY; chunkY <= endChunkY; chunkY++) {
            for (int chunkX = startChunkX; chunkX <= endChunkX; chunkX++) {
                int chunkStartX = chunkX * world.getChunkWidth();
                int chunkStartY = chunkY * world.getChunkHeight();
                int chunkEndX = std::min(chunkStartX + world.getChunkWidth(), WORLD_WIDTH);
                int chunkEndY = std::min(chunkStartY + world.getChunkHeight(), WORLD_HEIGHT);
                
                // Clip chunk bounds against visible area for improved performance
                int renderStartX = std::max(chunkStartX, visStartX);
                int renderStartY = std::max(chunkStartY, visStartY);
                int renderEndX = std::min(chunkEndX, visEndX);
                int renderEndY = std::min(chunkEndY, visEndY);
                
                // Skip fully invisible chunks
                if (renderStartX >= renderEndX || renderStartY >= renderEndY) {
                    continue;
                }
                
                // Render with a step size based on zoom level for better performance when zoomed out
                int step = std::max(1, static_cast<int>(1.0f / zoomLevel));
                
                // Only check pixels within visible part of this chunk
                for (int y = renderStartY; y < renderEndY; y += step) {
                    for (int x = renderStartX; x < renderEndX; x += step) {
                        if (world.get(x, y) == currentMat) {
                            glVertex2f(x, y);
                        }
                    }
                }
            }
        }
        glEnd();
        }
        
        // Reset transform
        glPopMatrix();
        
        // Display some information at the bottom of the screen
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(10, actualHeight - 20);
        std::string statusText = "FPS: " + std::to_string(frameCount) + 
                               " | Material: " + std::to_string(static_cast<int>(currentMaterial)) +
                               " | Brush Size: " + std::to_string(brushSize) +
                               " | Sim: " + (simulationRunning ? "Running" : "Paused");
                               
        // Status text would go here, but we're removing the gray bar
        // to fix the UI issue and improve appearance
        
        // Update the screen
        SDL_GL_SwapWindow(window);
        
        // FPS calculation
        frameCount++;
        if (SDL_GetTicks() - fpsTimer >= 1000) {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsTimer = SDL_GetTicks();
        }

        // Cap the frame rate
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }

    // Clean up
    renderer.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}