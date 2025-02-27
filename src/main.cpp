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
const int WORLD_WIDTH = 2400;    // Expanded world width for more horizontal exploration
const int WORLD_HEIGHT = 1800;   // Deeper world for vertical exploration
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
    std::cout << "Left mouse: Place selected material" << std::endl;
    std::cout << "Right mouse: Erase (place Empty)" << std::endl;
    std::cout << "WASD/Arrow keys: Move character" << std::endl;
    std::cout << "Space: Jump" << std::endl;
    std::cout << "C: Toggle between free camera and follow mode" << std::endl;
    std::cout << "Mouse wheel: Zoom in/out" << std::endl;
    std::cout << "Middle mouse: Pan camera (in free cam mode)" << std::endl;
    std::cout << "Material Selection:" << std::endl;
    std::cout << "  1: Sand            6: Oil" << std::endl;
    std::cout << "  2: Water           7: Grass" << std::endl;
    std::cout << "  3: Stone           8: Dirt" << std::endl;
    std::cout << "  4: Wood            9: Gravel" << std::endl;
    std::cout << "  5: Fire            0: Smoke" << std::endl;
    std::cout << "F11: Toggle fullscreen mode" << std::endl;
    std::cout << "R: Reset world with a new seed" << std::endl;
    std::cout << "ESC: Quit" << std::endl;

    // Camera state
    float zoomLevel = 2.0f;  // Start with an integer zoom for perfect pixel alignment
    int cameraX = 0;
    int cameraY = 0;
    bool freeCam = false;  // Free camera mode toggle
    
    // Default settings
    PixelPhys::MaterialType currentMaterial = PixelPhys::MaterialType::Sand;
    int brushSize = 2;  // Smaller brush for tighter pixel alignment
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
                } else if (e.key.keysym.sym == SDLK_7) {
                    currentMaterial = PixelPhys::MaterialType::Grass;
                    std::cout << "Selected material: Grass" << std::endl;
                } else if (e.key.keysym.sym == SDLK_8) {
                    currentMaterial = PixelPhys::MaterialType::Dirt;
                    std::cout << "Selected material: Dirt" << std::endl;
                } else if (e.key.keysym.sym == SDLK_9) {
                    currentMaterial = PixelPhys::MaterialType::Gravel;
                    std::cout << "Selected material: Gravel" << std::endl;
                } else if (e.key.keysym.sym == SDLK_0) {
                    currentMaterial = PixelPhys::MaterialType::Smoke;
                    std::cout << "Selected material: Smoke" << std::endl;
                } else if (e.key.keysym.sym == SDLK_c) {
                    // Toggle free camera mode
                    freeCam = !freeCam;
                    std::cout << "Camera mode: " << (freeCam ? "Free" : "Follow player") << std::endl;
                } else if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_LEFT) {
                    // Move player left
                    world.getPlayerX() > 0 ? world.movePlayerLeft() : void();
                } else if (e.key.keysym.sym == SDLK_d || e.key.keysym.sym == SDLK_RIGHT) {
                    // Move player right
                    world.getPlayerX() < WORLD_WIDTH ? world.movePlayerRight() : void();
                } else if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP || 
                           e.key.keysym.sym == SDLK_SPACE) {
                    // Jump
                    world.playerJump();
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
                // Use integer-only zoom levels for perfect pixel alignment
                int zoomDirection = e.wheel.y > 0 ? 1 : -1;
                
                // Update zoom level with integer steps only
                // This ensures perfect pixel grid alignment at all zoom levels
                zoomLevel = std::max(1.0f, std::min(roundf(zoomLevel) + zoomDirection, 10.0f));
                
                // Always use integer zoom levels for Noita-style pixel perfect rendering
                zoomLevel = roundf(zoomLevel);
                
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
        
        // Create a view rectangle that ensures proper aspect ratio
        SDL_Rect viewRect;
        const int margin = 10;
        
        // Find the best dimensions that maintain the world's aspect ratio
        int availableWidth = actualWidth - 2 * margin;
        int availableHeight = actualHeight - 2 * margin;
        
        // Calculate to maintain aspect ratio while maximizing space usage
        float worldAspect = static_cast<float>(WORLD_WIDTH) / WORLD_HEIGHT;
        float screenAspect = static_cast<float>(availableWidth) / availableHeight;
        
        // Determine view dimensions to maintain world aspect ratio
        int viewWidth, viewHeight;
        if (screenAspect > worldAspect) {
            // Screen is wider than world, constrain by height
            viewHeight = availableHeight;
            viewWidth = static_cast<int>(viewHeight * worldAspect);
        } else {
            // Screen is taller than world, constrain by width
            viewWidth = availableWidth;
            viewHeight = static_cast<int>(viewWidth / worldAspect);
        }
        
        // Center the view in the window
        viewRect.x = margin + (availableWidth - viewWidth) / 2;
        viewRect.y = margin + (availableHeight - viewHeight) / 2;
        viewRect.w = viewWidth;
        viewRect.h = viewHeight;
        
        // Check if mouse is inside the view area
        bool canEdit = (mouseX >= viewRect.x && mouseX < viewRect.x + viewRect.w &&
                       mouseY >= viewRect.y && mouseY < viewRect.y + viewRect.h);
        
        if (canEdit) {
            // Simple coordinate mapping
            
            // 1. Find position within view as 0.0-1.0 percentage
            float viewPercentX = (mouseX - viewRect.x) / static_cast<float>(viewRect.w);
            float viewPercentY = (mouseY - viewRect.y) / static_cast<float>(viewRect.h);
            
            // 2. Calculate visible world area based on zoom
            // Fix cursor alignment by using the same math as in the rendering section
            float integerZoom = roundf(zoomLevel);
            float visibleWorldWidth = WORLD_WIDTH / integerZoom;
            float visibleWorldHeight = WORLD_HEIGHT / integerZoom;
            
            // 3. Map screen position directly using the exact same calculation as the rendering
            // For perfect cursor alignment, we need to match the rendering transformation exactly
            float alignedCameraX = roundf(WORLD_WIDTH/2.0f + cameraX - (WORLD_WIDTH/integerZoom)/2.0f);
            float alignedCameraY = roundf(WORLD_HEIGHT/2.0f + cameraY - (WORLD_HEIGHT/integerZoom)/2.0f);
            
            // Map mouse position using the exact same transformations as the rendering
            int worldMouseX = static_cast<int>(alignedCameraX + viewPercentX * visibleWorldWidth);
            int worldMouseY = static_cast<int>(alignedCameraY + viewPercentY * visibleWorldHeight);
            
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
                
                // Place material in a more pixel-perfect pattern (Noita-like)
                for (int y = -brushSize; y <= brushSize; ++y) {
                    for (int x = -brushSize; x <= brushSize; ++x) {
                        // Custom pattern for different materials
                        bool shouldPlace;
                        
                        if (currentMaterial == PixelPhys::MaterialType::Stone) {
                            // For stone, create a pattern like cobblestone
                            shouldPlace = ((x*x + y*y <= brushSize*brushSize) && 
                                          ((worldMouseX+x + worldMouseY+y) % 3 != 0));
                        } 
                        else if (currentMaterial == PixelPhys::MaterialType::Wood) {
                            // For wood, create grain-like pattern
                            shouldPlace = ((x*x + y*y <= brushSize*brushSize) && 
                                          ((worldMouseY+y) % 3 != 0));
                        }
                        else {
                            // For other materials, just use a circular brush
                            shouldPlace = (x*x + y*y <= brushSize*brushSize); // Circular brush
                        }
                        
                        if (shouldPlace) {
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
            // Calculate delta time for smooth player movement
            static Uint32 lastTime = SDL_GetTicks();
            Uint32 currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            
            // Update player first
            world.updatePlayer(deltaTime);
            
            // Update world physics
            world.update();
            
            // Run additional liquid leveling to fix floating particles
            // Only do this occasionally to avoid performance impact
            if (frameCount % 10 == 0) {
                world.levelLiquids();
            }
            
            // Update camera to follow player if not in free cam mode
            if (!freeCam) {
                cameraX = world.getPlayerX() - WORLD_WIDTH / 2;
                cameraY = world.getPlayerY() - WORLD_HEIGHT / 2;
                
                // Clamp camera to world bounds
                cameraX = std::max(0, std::min(cameraX, WORLD_WIDTH));
                cameraY = std::max(0, std::min(cameraY, WORLD_HEIGHT));
            }
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
        
        // Remove border UI elements for a cleaner look
        
        // Render the world
        glPushMatrix();
        
        // Setup transformation to center the view properly with no gaps
        // Start by translating to the view area top-left corner
        glTranslatef(viewRect.x, viewRect.y, 0.0f);
        
        // Force integer zoom values to prevent subpixel rendering issues
        float integerZoom = roundf(zoomLevel);
        
        // Calculate pixel-aligned scaling factors, ensuring same scale on both axes
        float scaleX = viewRect.w / (WORLD_WIDTH / integerZoom);
        float scaleY = viewRect.h / (WORLD_HEIGHT / integerZoom);
        
        // Use uniform scaling to maintain square pixels
        float uniformScale = std::min(scaleX, scaleY);
        
        // Center the view in the available space
        // This eliminates the gap on the right by properly centering
        float horizontalOffset = (viewRect.w - (WORLD_WIDTH / integerZoom) * uniformScale) / 2;
        float verticalOffset = (viewRect.h - (WORLD_HEIGHT / integerZoom) * uniformScale) / 2;
        
        // Apply the centering offset
        glTranslatef(horizontalOffset, verticalOffset, 0.0f);
        
        // Apply the uniform scale
        glScalef(uniformScale, uniformScale, 1.0f);
        
        // Ensure camera position is aligned to integer grid for pixel-perfect rendering
        float alignedCameraX = roundf(WORLD_WIDTH/2.0f + cameraX - (WORLD_WIDTH/integerZoom)/2.0f);
        float alignedCameraY = roundf(WORLD_HEIGHT/2.0f + cameraY - (WORLD_HEIGHT/integerZoom)/2.0f);
        
        // Apply the camera transformation
        glTranslatef(-alignedCameraX, -alignedCameraY, 0.0f);
        
        // Batch render the world - render each material type separately
        
        // For each material type
        for (int materialIndex = 1; materialIndex < static_cast<int>(PixelPhys::MaterialType::COUNT); materialIndex++) {
            PixelPhys::MaterialType currentMat = static_cast<PixelPhys::MaterialType>(materialIndex);
            
            // Set material color
            float r = 0.0f, g = 0.0f, b = 0.0f;
            
            const auto& props = PixelPhys::MATERIAL_PROPERTIES[static_cast<std::size_t>(currentMat)];
            
            // First handle any time-based animations for certain materials
            if (currentMat == PixelPhys::MaterialType::Fire) {
                // Animate fire with time-based flicker
                float flicker = 0.2f * sin(SDL_GetTicks() * 0.01f);
                r = props.r / 255.0f + flicker;
                g = props.g / 255.0f + flicker * 0.7f;
                b = props.b / 255.0f + flicker * 0.3f;
                
                // Clamp to valid range
                r = std::max(0.0f, std::min(1.0f, r));
                g = std::max(0.0f, std::min(1.0f, g));
                b = std::max(0.0f, std::min(1.0f, b));
            }
            else if (currentMat == PixelPhys::MaterialType::Water) {
                // Slight wave effect for water
                int matIndex = static_cast<int>(currentMat);
                float wave = 0.05f * sin(SDL_GetTicks() * 0.003f + matIndex * 0.1f);
                r = props.r / 255.0f;
                g = props.g / 255.0f + wave;
                b = props.b / 255.0f + wave * 2.0f;
                
                // Clamp to valid range
                r = std::max(0.0f, std::min(1.0f, r));
                g = std::max(0.0f, std::min(1.0f, g));
                b = std::max(0.0f, std::min(1.0f, b));
            }
            else if (currentMat == PixelPhys::MaterialType::Smoke || currentMat == PixelPhys::MaterialType::Steam) {
                // Make smoke/steam partially transparent
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                r = props.r / 255.0f;
                g = props.g / 255.0f;
                b = props.b / 255.0f;
                glColor4f(r, g, b, 0.7f); // Use alpha
                continue; // Skip the regular color setting below
            }
            else if (currentMat == PixelPhys::MaterialType::Grass) {
                // Grass with slight variation
                float time = SDL_GetTicks() * 0.001f;
                float variation = 0.05f * sin(time);
                r = props.r / 255.0f;
                g = props.g / 255.0f + variation;
                b = props.b / 255.0f;
            }
            else {
                // Default colors from material properties
                // We'd normally use the per-pixel colors for variation, but in this
                // rendering approach we'll use a material-wide color with slight randomness
                float variation = 0.05f * sin(frameCount * 0.01f + static_cast<int>(currentMat) * 0.3f);
                r = props.r / 255.0f + variation * 0.3f;
                g = props.g / 255.0f + variation * 0.3f;
                b = props.b / 255.0f + variation * 0.3f;
                
                // Clamp to valid range
                r = std::max(0.0f, std::min(1.0f, r));
                g = std::max(0.0f, std::min(1.0f, g));
                b = std::max(0.0f, std::min(1.0f, b));
            }
            
            // Set the color for this material
            glColor3f(r, g, b);
            
            // Instead of using GL_POINTS which can leave gaps,
            // we'll use quads (GL_QUADS) to ensure pixels are perfectly tiled
            // with no gaps between them, like in Noita
            
            // Calculate exact pixel size for perfect tiling
            float pixelSize = roundf(zoomLevel);
            if (pixelSize < 1.0f) pixelSize = 1.0f;
            
        // Use GL_QUADS for rendering tightly packed pixels with no gaps
        glBegin(GL_QUADS);
        
        // Calculate visible area in world coordinates based on camera position and zoom
        // Use integer zoom for pixel-perfect alignment
        float integerZoom = roundf(zoomLevel);
        float visibleWorldWidth = WORLD_WIDTH / integerZoom;
        float visibleWorldHeight = WORLD_HEIGHT / integerZoom;
        
        // Round all values to integers to ensure perfect pixel alignment
        float worldStartX = roundf(WORLD_WIDTH/2.0f + cameraX - visibleWorldWidth/2.0f);
        float worldStartY = roundf(WORLD_HEIGHT/2.0f + cameraY - visibleWorldHeight/2.0f);
        
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
                
                // Ensure pixels are rendered consistently
                // Always render every pixel for high-quality appearance
                int step = 1;
                
                // Remove the pixel offset that was causing misalignment with terrain
                
                // Only check pixels within visible part of this chunk
                for (int y = renderStartY; y < renderEndY; y += step) {
                    for (int x = renderStartX; x < renderEndX; x += step) {
                        if (world.get(x, y) == currentMat) {
                            // Draw a quad for each pixel to ensure no gaps
                            // This creates perfectly tiled pixels like in Noita
                            glVertex2i(x, y);               // Bottom left
                            glVertex2i(x+1, y);             // Bottom right
                            glVertex2i(x+1, y+1);           // Top right
                            glVertex2i(x, y+1);             // Top left
                        }
                    }
                }
            }
        }
        glEnd();
        }
        
        // Render the player character
        if (!freeCam) { // Only render the player in follow mode
            glColor3f(0.0f, 1.0f, 0.0f); // Bright green color for better visibility
            
            // Calculate player position in screen space
            float playerScreenX, playerScreenY;
            playerScreenX = world.getPlayerX();
            playerScreenY = world.getPlayerY();
            
            // Draw player as a small quad for consistent appearance
            int playerSize = 4; // Larger player size for better visibility
            glBegin(GL_QUADS);
            
            // Convert player position to integer for pixel alignment
            int px = static_cast<int>(playerScreenX);
            int py = static_cast<int>(playerScreenY);
            
            // Draw player as a square centered at player position
            glVertex2i(px - playerSize/2, py - playerSize/2);           // Bottom left
            glVertex2i(px + playerSize/2, py - playerSize/2);           // Bottom right
            glVertex2i(px + playerSize/2, py + playerSize/2);           // Top right
            glVertex2i(px - playerSize/2, py + playerSize/2);           // Top left
            glEnd();
        }
        
        // Reset transform
        glPopMatrix();
        
        // Remove UI text elements for a cleaner look
        
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