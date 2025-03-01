#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>
#include <random>
#include <chrono>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

#include "../include/Materials.h"
#include "../include/World.h"
#include "../include/Renderer.h"
#include "../include/VulkanBackend.h" // Include VulkanBackend for direct access to drawing functions

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WORLD_WIDTH = 2400;    // Expanded world width for horizontal exploration
const int WORLD_HEIGHT = 1800;   // Deeper world for vertical exploration
const int TARGET_FPS = 60;
const int FRAME_DELAY = 1000 / TARGET_FPS;

// Default values for maximum performance
const float MIN_ZOOM = 1.0f;     // Minimum zoom level (maximum view size)
const float MAX_ZOOM = 10.0f;    // Maximum zoom level (minimum view size)
const int MAX_PHYSICS_ITERATIONS = 3; // Maximum physics updates per frame
const int ACTIVE_SIMULATION_RADIUS = 300; // No optimization, use full world size
const bool ENABLE_CULLING = false; // Disable culling for better quality

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    std::cout << "Initializing with Vulkan backend" << std::endl;
    
    // Create window with Vulkan support
    SDL_Window* window = SDL_CreateWindow(
        "PixelPhys2D (Vulkan)",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Set to fullscreen
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    // Get actual window size
    int actualWidth, actualHeight;
    SDL_GetWindowSize(window, &actualWidth, &actualHeight);
    
    // Create the pixel physics world
    PixelPhys::World world(WORLD_WIDTH, WORLD_HEIGHT);
    
    // Generate initial terrain with a random seed (same as main.cpp)
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    std::cout << "Generating world with seed: " << seed << std::endl;
    world.generate(seed);
    
    // Force a short delay to ensure the world is fully generated before spawning player
    SDL_Delay(200);
    
    // Create the renderer with the actual window size
    auto renderer = std::make_shared<PixelPhys::Renderer>(actualWidth, actualHeight, PixelPhys::BackendType::Vulkan);
    if (!renderer->initialize(window)) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Force a short delay to allow proper initialization of Vulkan resources
    SDL_Delay(100);
    
    // Show the system cursor instead of using a custom one
    SDL_ShowCursor(SDL_ENABLE);
    
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
    std::cout << "Shift + Left mouse: Dig terrain (when close enough)" << std::endl;
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
    float zoomLevel = 0.5f;  // Start with a wider zoom to see more of the world
    int cameraX = 0;
    int cameraY = 0;
    bool freeCam = false;  // Free camera mode toggle
    
    // Default settings
    PixelPhys::MaterialType currentMaterial = PixelPhys::MaterialType::Sand;
    int brushSize = 2;  // Smaller brush for tighter pixel alignment
    bool simulationRunning = true;

    // Fixed timestep for physics
    const float PHYSICS_TIMESTEP = 1.0f / 60.0f; // Physics update at 60Hz
    float accumulator = 0.0f;
    Uint32 lastFrameTime = SDL_GetTicks();
    
    // Main loop
    while (!quit) {
        frameStart = SDL_GetTicks();

        // Calculate delta time in seconds
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        // Cap delta time to prevent huge physics jumps after lag
        if (deltaTime > 0.25f) {
            deltaTime = 0.25f;
        }

        // Accumulate time for physics updates
        accumulator += deltaTime;

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
                    
                    // Debug output for new dimensions
                    std::cout << "Window resized to " << actualWidth << "x" << actualHeight << std::endl;
                } else if (e.key.keysym.sym == SDLK_SPACE) {
                    // Only respond to key press, not hold
                    if (e.key.repeat == 0) {
                        world.playerJump();
                    }
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
                    // Jump - only respond to key press, not hold
                    if (e.key.repeat == 0) {
                        world.playerJump();
                    }
                }
            } else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Window has been resized
                    int newWidth = e.window.data1;
                    int newHeight = e.window.data2;
                    
                    // Update actual width and height variables for the current frame
                    actualWidth = newWidth;
                    actualHeight = newHeight;
                    
                    // Debug output for manual resize
                    std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                // Use integer-only zoom levels for perfect pixel alignment
                int zoomDirection = e.wheel.y > 0 ? 1 : -1;
                
                // Update zoom level with capped range for performance optimization
                // Using MIN_ZOOM and MAX_ZOOM constants to limit zoom range
                zoomLevel = (std::max)(MIN_ZOOM, (std::min)(roundf(zoomLevel) + zoomDirection, MAX_ZOOM));
                
                // Always use integer zoom levels for Noita-style pixel perfect rendering
                zoomLevel = roundf(zoomLevel);
                
                std::cout << "Zoom level: " << zoomLevel << std::endl;
            } else if (e.type == SDL_MOUSEMOTION && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE))) {
                // Pan with middle mouse button
                cameraX -= e.motion.xrel;
                cameraY -= e.motion.yrel;
                
                // Limit camera position to prevent moving too far from world
                cameraX = (std::max)(-WORLD_WIDTH/2, (std::min)(cameraX, WORLD_WIDTH/2));
                cameraY = (std::max)(-WORLD_HEIGHT/2, (std::min)(cameraY, WORLD_HEIGHT/2));
            }
        }
        
        // Handle mouse interactions for placing/erasing materials and digging
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
            // Improved coordinate mapping for Vulkan backend
            
            // Get mouse position relative to the view area (0 to 1)
            float mouseViewX = (mouseX - viewRect.x) / static_cast<float>(viewRect.w);
            float mouseViewY = (mouseY - viewRect.y) / static_cast<float>(viewRect.h);
            
            // Calculate the visible portion of the world
            float visibleWorldWidth = WORLD_WIDTH / zoomLevel;
            float visibleWorldHeight = WORLD_HEIGHT / zoomLevel;
            
            // Calculate the world origin in screen space
            // This is the most critical part - we need to use the exact same camera
            // calculation as we do in the rendering code
            float worldOriginX, worldOriginY;
            
            if (!freeCam) {
                // In follow mode, camera is centered on player
                worldOriginX = world.getPlayerX() - visibleWorldWidth / 2;
                worldOriginY = world.getPlayerY() - visibleWorldHeight / 2;
            } else {
                // In free cam mode, camera is at the specified position
                worldOriginX = cameraX;
                worldOriginY = cameraY;
            }
            
            // Clamp camera to world boundaries
            worldOriginX = std::max(0.0f, std::min(worldOriginX, WORLD_WIDTH - visibleWorldWidth));
            worldOriginY = std::max(0.0f, std::min(worldOriginY, WORLD_HEIGHT - visibleWorldHeight));
            
            // Convert mouse position to world coordinates using a direct linear mapping
            // We map the view rect [0,1] to the visible world area [origin, origin+visible]
            int worldMouseX = static_cast<int>(worldOriginX + mouseViewX * visibleWorldWidth);
            int worldMouseY = static_cast<int>(worldOriginY + mouseViewY * visibleWorldHeight);
            
            // Ensure coordinates are within the world bounds
            worldMouseX = (std::max)(0, (std::min)(worldMouseX, WORLD_WIDTH - 1));
            worldMouseY = (std::max)(0, (std::min)(worldMouseY, WORLD_HEIGHT - 1));
            
            // Left mouse button: if holding shift, dig. Otherwise, place material
            if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                // Check if shift is held down (digging mode)
                const Uint8* keyState = SDL_GetKeyboardState(NULL);
                bool shiftPressed = keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT];
                
                if (shiftPressed && world.isPlayerDigging()) {
                    // Digging mode - try to dig terrain
                    PixelPhys::MaterialType dugMaterial;
                    if (world.performPlayerDigging(worldMouseX, worldMouseY, dugMaterial)) {
                        // Successfully dug, could spawn particles here
                        if (frameCount % 5 == 0) {
                            std::cout << "Digging at: " << worldMouseX << "," << worldMouseY 
                                     << " Material: " << static_cast<int>(dugMaterial) << std::endl;
                        }
                    }
                } else {
                    // Material placement mode
                    // Log coordinates more frequently to debug placement issues
                    std::cout << "Placing " << static_cast<int>(currentMaterial) << " at: " 
                             << worldMouseX << "," << worldMouseY 
                             << " (brush size: " << brushSize << ")" << std::endl;
                    
                    // Directly place material at target position
                    world.set(worldMouseX, worldMouseY, currentMaterial);
                    
                    // Place material in a more pixel-perfect pattern (Noita-like)
                    for (int y = -brushSize; y <= brushSize; ++y) {
                        for (int x = -brushSize; x <= brushSize; ++x) {
                            // Skip the center pixel - we already placed it
                            if (x == 0 && y == 0) continue;
                            
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
                                
                                // Make sure we're in bounds of the world and explicitly set the material
                                if (px >= 0 && px < WORLD_WIDTH && py >= 0 && py < WORLD_HEIGHT) {
                                    // Force material placement with explicit call
                                    world.set(px, py, currentMaterial);
                                }
                            }
                        }
                    }
                }
            }
            
            // Right mouse button always erases
            if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                // Log more frequently for debugging
                std::cout << "Erasing at: " << worldMouseX << "," << worldMouseY << std::endl;
                
                // Directly erase at target position
                world.set(worldMouseX, worldMouseY, PixelPhys::MaterialType::Empty);
                
                // Erase in a circular brush area
                for (int y = -brushSize; y <= brushSize; ++y) {
                    for (int x = -brushSize; x <= brushSize; ++x) {
                        // Skip the center pixel - we already erased it
                        if (x == 0 && y == 0) continue;
                        
                        if (x*x + y*y <= brushSize*brushSize) { // Circular brush
                            int px = worldMouseX + x;
                            int py = worldMouseY + y;
                            
                            // Make sure we're in bounds of the world
                            if (px >= 0 && px < WORLD_WIDTH && py >= 0 && py < WORLD_HEIGHT) {
                                // Force empty placement with explicit call
                                world.set(px, py, PixelPhys::MaterialType::Empty);
                            }
                        }
                    }
                }
            }
            
            // Show a visual indicator of digging range when shift is pressed
            const Uint8* keyState = SDL_GetKeyboardState(NULL);
            bool shiftPressed = keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT];
            
            if (shiftPressed && world.isPlayerDigging() && !freeCam) {
                // Player position and direction information will be calculated when rendering the dig indicator
                
                // Store the digging info to draw it during the rendering phase
                // We'll draw this after the world rendering
            }
        }

        // MINIMAL IMPLEMENTATION: Use only the most basic functionality
        // Update player position
        world.updatePlayer(PHYSICS_TIMESTEP);
        
        // Do a single physics step - no complex updates
        world.update();
        
        // Update camera to follow player
        if (!freeCam) {
            cameraX = world.getPlayerX() - WORLD_WIDTH / 2;
            cameraY = world.getPlayerY() - WORLD_HEIGHT / 2;
        }
        
        // Advanced rendering with a grid to show materials
        try {
            // Call the renderer's beginFrame to set up the frame 
            renderer->beginFrame();
            
            // Set a dark background
            renderer->getBackend()->setClearColor(0.05f, 0.05f, 0.1f, 1.0f);
            renderer->getBackend()->clear();
            
            // Every few frames, print the player position so you can track movement
            if (frameCount % 60 == 0) {
                std::cout << "Player at: (" << world.getPlayerX() << ", " 
                          << world.getPlayerY() << ") - "
                          << "Material: " << static_cast<int>(world.get(world.getPlayerX(), world.getPlayerY()))
                          << std::endl;
            }
            
            // Camera calculations
            // Calculate visible area based on zoom and window size
            float visibleWorldWidth = WORLD_WIDTH / zoomLevel;
            float visibleWorldHeight = WORLD_HEIGHT / zoomLevel;
            
            // Store the original camera for reference
            float worldOriginX = cameraX;
            float worldOriginY = cameraY;
            
            // Update camera position based on player if in follow mode
            if (!freeCam) {
                worldOriginX = world.getPlayerX() - visibleWorldWidth / 2;
                worldOriginY = world.getPlayerY() - visibleWorldHeight / 2;
            }
            
            // Clamp camera position to world boundaries
            float maxCameraX = static_cast<float>(WORLD_WIDTH) - visibleWorldWidth;
            float maxCameraY = static_cast<float>(WORLD_HEIGHT) - visibleWorldHeight;
            worldOriginX = std::max(0.0f, std::min(worldOriginX, maxCameraX));
            worldOriginY = std::max(0.0f, std::min(worldOriginY, maxCameraY));
            
            // Update actual camera position
            cameraX = worldOriginX;
            cameraY = worldOriginY;
            
            // Calculate visible area in world coordinates
            int startX = static_cast<int>(cameraX);
            int startY = static_cast<int>(cameraY);
            int endX = startX + static_cast<int>(visibleWorldWidth) + 1;
            int endY = startY + static_cast<int>(visibleWorldHeight) + 1;
            
            // Clamp to world boundaries
            endX = std::min(endX, WORLD_WIDTH);
            endY = std::min(endY, WORLD_HEIGHT);
            
            // Draw a grid of colored rectangles to represent world cells
            // We'll use the grid step size calculated below for the drawing loop
            
            // Set a background color to make pixels visible
            renderer->getBackend()->setClearColor(0.1f, 0.1f, 0.2f, 1.0f);
            
            // Access the Vulkan backend for drawing
            auto vulkanBackend = static_cast<PixelPhys::VulkanBackend*>(renderer->getBackend());
            
            // Log player position occasionally
            if (frameCount % 10 == 0) {
                std::cout << "Player at: (" << world.getPlayerX() << ", " 
                        << world.getPlayerY() << ") - " 
                        << "Material: " << static_cast<int>(world.get(world.getPlayerX(), world.getPlayerY()))
                        << std::endl;
            }
            
            // Calculate proper cell sizes based on zoom level
            // This is the number of screen pixels per world unit
            float pixelsPerWorldUnit = actualWidth / visibleWorldWidth;
            
            // Base cell size that scales with zoom
            float cellWidth = std::max(1.0f, pixelsPerWorldUnit);
            float cellHeight = std::max(1.0f, pixelsPerWorldUnit);
            
            // For consistent appearance, we use a minimum cell size of 3 pixels
            // At high zoom levels, cells should be larger for better visibility
            cellWidth = std::max(3.0f, cellWidth);
            cellHeight = std::max(3.0f, cellHeight);
            
            // Calculate visible area in world coordinates more precisely
            int visibleStartX = std::max(0, static_cast<int>(cameraX));
            int visibleStartY = std::max(0, static_cast<int>(cameraY));
            int visibleEndX = std::min(WORLD_WIDTH, visibleStartX + static_cast<int>(visibleWorldWidth) + 2); // Add extra margin
            int visibleEndY = std::min(WORLD_HEIGHT, visibleStartY + static_cast<int>(visibleWorldHeight) + 2); // Add extra margin
            
            // Draw a grid representation of the world (simplified)
            // Adapt grid step size based on zoom level
            // When zoomed out, use larger steps for better performance
            // When zoomed in, use small steps for detail
            int gridStepSize = std::max(1, static_cast<int>(2.0f / zoomLevel));
            
            // Limit the max step size to avoid too sparse terrain
            gridStepSize = std::min(4, gridStepSize);
            
            // Draw visible grid cells to represent the world
            for (int y = visibleStartY; y < visibleEndY; y += gridStepSize) {
                for (int x = visibleStartX; x < visibleEndX; x += gridStepSize) {
                    // Get material at this position
                    PixelPhys::MaterialType material = world.get(x, y);
                    
                    // Skip empty cells
                    if (material == PixelPhys::MaterialType::Empty) {
                        continue;
                    }
                    
                    // Calculate screen coordinates for this cell
                    // This is the exact inverse of the mouse coordinate calculation
                    // Map from world space [cameraX, cameraX+visibleWidth] to view space [0, viewWidth]
                    float screenX = (x - cameraX) / visibleWorldWidth * viewRect.w + viewRect.x;
                    float screenY = (y - cameraY) / visibleWorldHeight * viewRect.h + viewRect.y;
                    
                    // Determine color based on material using the values from MATERIAL_PROPERTIES
                    float r = 0.5f, g = 0.5f, b = 0.5f; // Default gray
                    
                    // Get the material color from MATERIAL_PROPERTIES array
                    int materialIndex = static_cast<int>(material);
                    if (materialIndex >= 0 && materialIndex < static_cast<int>(PixelPhys::MaterialType::COUNT)) {
                        const auto& props = PixelPhys::MATERIAL_PROPERTIES[materialIndex];
                        
                        // Convert 0-255 values to 0.0-1.0 range
                        r = props.r / 255.0f;
                        g = props.g / 255.0f;
                        b = props.b / 255.0f;
                        
                        // For emissive materials, make them slightly brighter
                        if (props.isEmissive) {
                            float brightnessBoost = props.emissiveStrength / 255.0f * 0.5f;
                            r = std::min(1.0f, r + brightnessBoost);
                            g = std::min(1.0f, g + brightnessBoost);
                            b = std::min(1.0f, b + brightnessBoost);
                        }
                    }
                    
                    // Draw this cell with slightly larger dimensions to avoid gaps
                    vulkanBackend->drawRectangle(
                        screenX, screenY,
                        cellWidth + 1.0f, cellHeight + 1.0f,  // Add 1 pixel to both dimensions
                        r, g, b
                    );
                }
            }
            
            // Get player position in screen coordinates using the same transformation
            // as the world cells for consistency
            float playerScreenX = (world.getPlayerX() - cameraX) / visibleWorldWidth * viewRect.w + viewRect.x;
            float playerScreenY = (world.getPlayerY() - cameraY) / visibleWorldHeight * viewRect.h + viewRect.y;
            
            // Get the material under the player
            PixelPhys::MaterialType playerMaterial = world.get(world.getPlayerX(), world.getPlayerY());
            
            // Default color (used if material isn't recognized)
            float r = 1.0f, g = 1.0f, b = 1.0f;
            
            // Get player material color from MATERIAL_PROPERTIES
            int materialIndex = static_cast<int>(playerMaterial);
            if (materialIndex >= 0 && materialIndex < static_cast<int>(PixelPhys::MaterialType::COUNT)) {
                const auto& props = PixelPhys::MATERIAL_PROPERTIES[materialIndex];
                
                // Convert 0-255 values to 0.0-1.0 range
                r = props.r / 255.0f;
                g = props.g / 255.0f;
                b = props.b / 255.0f;
                
                // For emissive materials, make them slightly brighter
                if (props.isEmissive) {
                    float brightnessBoost = props.emissiveStrength / 255.0f * 0.5f;
                    r = std::min(1.0f, r + brightnessBoost);
                    g = std::min(1.0f, g + brightnessBoost);
                    b = std::min(1.0f, b + brightnessBoost);
                }
            } else {
                // Use white as fallback for unknown materials
                r = 1.0f; g = 1.0f; b = 1.0f;
            }
            
            // Draw the cell with the material color under the player
            vulkanBackend->drawRectangle(
                playerScreenX, playerScreenY,
                cellWidth, cellHeight,
                r, g, b
            );
            
            // And also draw a white box for the player itself (always white)
            // Make sure player is visible with slightly larger cell
            vulkanBackend->drawRectangle(
                playerScreenX, playerScreenY,
                cellWidth + 1.0f, cellHeight + 1.0f,  // Add 1 pixel to both dimensions
                1.0f, 1.0f, 1.0f  // Pure white for player
            );
            
            // End the frame
            renderer->endFrame();
        } catch (const std::exception& e) {
            std::cerr << "Rendering error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown rendering error" << std::endl;
        }
        
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
    
    // Safe cleanup strategy to avoid crashes
    std::cout << "Cleaning up resources" << std::endl;
    
    // First reset the renderer (not doing full cleanup)
    // This prevents the vkDeviceWaitIdle error
    renderer.reset();
    
    // SDL cleanup only after renderer is destroyed
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}