#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <map>
#include <string>

#include "../include/Materials.h"
#include "../include/World.h"
#include "../include/Renderer.h"
#include "../include/VulkanBackend.h"
#include "../include/Character.h"

const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;
// Test mode with smaller world for physics testing
const bool TEST_MODE = false;

// World dimensions - use smaller world in test mode, deeper world for better exploration
const int WORLD_WIDTH   = TEST_MODE ? 800 : 6000;  // Much smaller for test mode
const int WORLD_HEIGHT  = TEST_MODE ? 600 : 6000;  // Much deeper world for exploration with chunk streaming
const int TARGET_FPS    = 60;  // Higher FPS for smoother simulation
const int FRAME_DELAY   = 1000 / TARGET_FPS;

// Camera parameters
int cameraX = 0;         // Camera position X
int cameraY = 0;         // Camera position Y
const int CAMERA_SPEED = 20;   // Camera movement speed (adjust for zoom level)
const int DEFAULT_VIEW_HEIGHT = 450; // Default height to position camera at start
const float PIXEL_SIZE = 2.0f;  // Global pixel size for world rendering - doubled for better visibility

// Mouse parameters
bool middleMouseDown = false;
int mouseX = 0, mouseY = 0;
int prevMouseX = 0, prevMouseY = 0;

// Character mode parameters
bool playerMode = false;  // Toggle between camera mode and player mode

int main() {
    // Initialize SDL with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " 
                  << SDL_GetError() << std::endl;
        return 1;
    }
    
    // std::cout << "Initializing with Vulkan backend" << std::endl;
    
    // Create a window with Vulkan support
    SDL_Window* window = SDL_CreateWindow(
        "PixelPhys2D (Vulkan)",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " 
                  << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    int actualWidth, actualHeight;
    SDL_Vulkan_GetDrawableSize(window, &actualWidth, &actualHeight); // IMPORTANT
    // std::cout << "Drawable size (after going fullscreen): "
    //          << actualWidth << "x" << actualHeight << std::endl;
    
    // Create the world and generate terrain or simple test environment
    PixelPhys::World world(WORLD_WIDTH, WORLD_HEIGHT);
    
    if (TEST_MODE) {
        // Create a simple test environment instead of a full world
        // std::cout << "Creating test environment for physics testing..." << std::endl;
        
        // Create a flat bottom platform across the world
        for (int x = 0; x < WORLD_WIDTH; x++) {
            for (int y = WORLD_HEIGHT - 50; y < WORLD_HEIGHT; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
    } else {
        // Generate regular terrain for normal mode
        unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
        // std::cout << "Generating world with seed: " << seed << std::endl;
        world.generate(seed);
    }
    
    // Create the character (earthworm) positioned at the center of the world
    // Character will be activated when player presses 'p'
    std::unique_ptr<PixelPhys::Character> character = 
        std::make_unique<PixelPhys::Character>(world, WORLD_WIDTH / 2, DEFAULT_VIEW_HEIGHT);
    
    SDL_Delay(200);  // Give the world time to set up
    
    // Start camera in the upper part of the world for better exploration
    cameraX = WORLD_WIDTH / 2 - actualWidth / (2 * PIXEL_SIZE);  // Center camera horizontally
    cameraY = DEFAULT_VIEW_HEIGHT - actualHeight / (2 * PIXEL_SIZE);  // Position at the default view height
    
    // Ensure camera is properly clamped to world bounds
    cameraX = std::max(0, cameraX);
    cameraX = std::min(WORLD_WIDTH - actualWidth, cameraX);
    cameraY = std::max(0, cameraY);
    cameraY = std::min(WORLD_HEIGHT - 50, cameraY);
    
    // Initialize world player position to center camera view - adjust for pixel size
    world.updatePlayerPosition(cameraX + actualWidth/PIXEL_SIZE/2, cameraY + actualHeight/PIXEL_SIZE/2);
    // std::cout << "Camera positioned at world center" << std::endl;
    
    // Create the renderer (Vulkan only)
    auto renderer = std::make_shared<PixelPhys::Renderer>(
        actualWidth, actualHeight, PixelPhys::BackendType::Vulkan
    );
    if (!renderer->initialize(window)) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Delay(100);  // Allow proper Vulkan initialization
    
    // Enable system cursor
    SDL_ShowCursor(SDL_ENABLE);
    
    // Material placement variables
    bool leftMouseDown = false;
    int placeBrushSize = 3;  // Size of placement brush
    PixelPhys::MaterialType currentMaterial = PixelPhys::MaterialType::Sand;  // Default material to place
    
    // Material name mapping for UI display
    std::map<PixelPhys::MaterialType, std::string> materialNames = {
        {PixelPhys::MaterialType::Empty, "Eraser"},
        {PixelPhys::MaterialType::Sand, "Sand"},
        {PixelPhys::MaterialType::Water, "Water"},
        {PixelPhys::MaterialType::Stone, "Stone"},
        {PixelPhys::MaterialType::Fire, "Fire"},
        {PixelPhys::MaterialType::Oil, "Oil"},
        {PixelPhys::MaterialType::GrassStalks, "Grass Stalks"},
        {PixelPhys::MaterialType::Dirt, "Dirt"},
        {PixelPhys::MaterialType::FlammableGas, "Flammable Gas"},
        {PixelPhys::MaterialType::Grass, "Grass"},
        {PixelPhys::MaterialType::Lava, "Lava"},
        {PixelPhys::MaterialType::Snow, "Snow"},
        {PixelPhys::MaterialType::Bedrock, "Bedrock"},
        {PixelPhys::MaterialType::Sandstone, "Sandstone"},
        {PixelPhys::MaterialType::Gravel, "Gravel"},
        {PixelPhys::MaterialType::TopSoil, "Top Soil"},
        {PixelPhys::MaterialType::DenseRock, "Dense Rock"}
    };
    
    bool quit = false;
    SDL_Event e;
    Uint32 frameStart, frameTime;
    int frameCount = 0;
    Uint32 fpsTimer = SDL_GetTicks();
    
    // Main loop
    while (!quit) {
        frameStart = SDL_GetTicks();
        
        // Process events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT)
                quit = true;
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    quit = true;
                else if (e.key.keysym.sym == SDLK_r) {
                    // Reset world with a new seed
                    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
                    world.generate(seed);
                    // std::cout << "World reset with seed: " << seed << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_F11) {
                    // Toggle fullscreen mode
                    Uint32 flags = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, flags ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    SDL_Vulkan_GetDrawableSize(window, &actualWidth, &actualHeight);
                    // std::cout << "Window resized to " << actualWidth << "x" << actualHeight << std::endl;
                }
                // Simple camera movement with keyboard
                else if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_a) {
                    cameraX -= CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraX = std::max(0, cameraX);
                    // std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) {
                    cameraX += CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraX = std::min(WORLD_WIDTH - actualWidth, cameraX);
                    // std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_w) {
                    cameraY -= CAMERA_SPEED; // Up key moves camera up (decreases Y)
                    // Clamp camera position to world bounds
                    cameraY = std::max(0, cameraY);
                    // std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_s) {
                    cameraY += CAMERA_SPEED; // Down key moves camera down (increases Y)
                    // Allow scrolling to the very bottom of the world
                    cameraY = std::min(WORLD_HEIGHT - 50, cameraY);
                    // std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                // Reset camera position
                else if (e.key.keysym.sym == SDLK_HOME) {
                    cameraX = 0; // Reset to origin
                    cameraY = 0;
                    // std::cout << "Camera reset to default position" << std::endl;
                }
                // Toggle between player mode and camera mode
                else if (e.key.keysym.sym == SDLK_p) {
                    playerMode = !playerMode;
                    if (playerMode) {
                        // std::cout << "Switched to player mode (earthworm)" << std::endl;
                        // Convert screen coordinates to world coordinates
                        const float pixelSize = 4.0f; // Must match the value in Renderer.cpp
                        
                        // Calculate position at center of screen in world coordinates
                        int worldX = cameraX + static_cast<int>(actualWidth / (2 * pixelSize));
                        int worldY = cameraY + static_cast<int>(actualHeight / (2 * pixelSize));
                        
                        // Initialize character at screen center and set it active
                        character = std::make_unique<PixelPhys::Character>(world, worldX, worldY);
                        character->setActive(true);
                        character->draw();
                        
                        // Log character position
                        // std::cout << "Character spawned at world position: " << worldX << "," << worldY << std::endl;
                    } else {
                        // std::cout << "Switched to camera mode" << std::endl;
                        character->setActive(false);
                        character->clear();
                    }
                }
                // Brush size controls
                else if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS || e.key.keysym.sym == SDLK_KP_PLUS) {
                    // Increase brush size (support both keyboard and numeric keypad plus)
                    placeBrushSize = std::min(20, placeBrushSize + 1);
                    // std::cout << "Brush size increased to: " << placeBrushSize << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_MINUS || e.key.keysym.sym == SDLK_KP_MINUS) {
                    // Decrease brush size (support both keyboard and numeric keypad minus)
                    placeBrushSize = std::max(1, placeBrushSize - 1);
                    // std::cout << "Brush size decreased to: " << placeBrushSize << std::endl;
                }
                // Material selection hotkeys
                else if (e.key.keysym.sym == SDLK_1) {
                    currentMaterial = PixelPhys::MaterialType::Sand;
                    // std::cout << "Selected Sand" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_2) {
                    currentMaterial = PixelPhys::MaterialType::Water;
                    // std::cout << "Selected Water" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_3) {
                    currentMaterial = PixelPhys::MaterialType::Stone;
                    // std::cout << "Selected Stone" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_4) {
                    currentMaterial = PixelPhys::MaterialType::Gravel;
                    // std::cout << "Selected Gravel" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_5) {
                    currentMaterial = PixelPhys::MaterialType::Oil;
                    // std::cout << "Selected Oil" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_6) {
                    currentMaterial = PixelPhys::MaterialType::Lava;
                    // std::cout << "Selected Lava" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_7) {
                    currentMaterial = PixelPhys::MaterialType::Fire;
                    // std::cout << "Selected Fire" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_0) {
                    // Eraser - set to Empty
                    currentMaterial = PixelPhys::MaterialType::Empty;
                    // std::cout << "Selected Eraser (Empty)" << std::endl;
                }
                // Add test keys for physics demonstrations
                else if (e.key.keysym.sym == SDLK_t) {
                    // Drop a large column of sand to test inertial mechanics
                    int testX = WORLD_WIDTH / 2;
                    int testWidth = 40;
                    int testHeight = 100;
                    
                    for (int x = testX - testWidth/2; x < testX + testWidth/2; x++) {
                        for (int y = 50; y < 50 + testHeight; y++) {
                            world.set(x, y, PixelPhys::MaterialType::Sand);
                        }
                    }
                    // std::cout << "Created test column of sand" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_y) {
                    // Create a water pool to test liquid dispersal
                    int testX = 200;
                    int testWidth = 30;
                    int testHeight = 60;
                    
                    for (int x = testX - testWidth/2; x < testX + testWidth/2; x++) {
                        for (int y = 200; y < 200 + testHeight; y++) {
                            world.set(x, y, PixelPhys::MaterialType::Water);
                        }
                    }
                    // std::cout << "Created test water pool" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_u) {
                    // Add various materials for visual comparison
                    // Sand
                    for (int x = 100; x < 130; x++) {
                        for (int y = 100; y < 130; y++) {
                            world.set(x, y, PixelPhys::MaterialType::Sand);
                        }
                    }
                    
                    // Gravel (more resistant)
                    for (int x = 150; x < 180; x++) {
                        for (int y = 100; y < 130; y++) {
                            world.set(x, y, PixelPhys::MaterialType::Gravel);
                        }
                    }
                    
                    // Dirt
                    for (int x = 200; x < 230; x++) {
                        for (int y = 100; y < 130; y++) {
                            world.set(x, y, PixelPhys::MaterialType::Dirt);
                        }
                    }
                    // std::cout << "Created material comparison test" << std::endl;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                // Mouse wheel can be used for scrolling vertically instead
                if (e.wheel.y > 0) {
                    // Scroll up
                    cameraY -= CAMERA_SPEED * 5;
                    cameraY = std::max(0, cameraY);
                } else if (e.wheel.y < 0) {
                    // Scroll down - allow scrolling to the very bottom of the world
                    cameraY += CAMERA_SPEED * 5;
                    cameraY = std::min(WORLD_HEIGHT - 50, cameraY);
                }
                
                // Update player position for appropriate chunk loading - adjust for pixel size
                world.updatePlayerPosition(cameraX + actualWidth/PIXEL_SIZE/2, cameraY + actualHeight/PIXEL_SIZE/2);
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_MIDDLE) {
                    // Start panning with middle mouse button
                    middleMouseDown = true;
                    prevMouseX = e.button.x;
                    prevMouseY = e.button.y;
                }
                else if (e.button.button == SDL_BUTTON_LEFT) {
                    // Start placing material with left mouse button
                    leftMouseDown = true;
                    // Update mouse position immediately
                    mouseX = e.button.x;
                    mouseY = e.button.y;
                }
                else if (e.button.button == SDL_BUTTON_RIGHT) {
                    // Cycle through materials with right click
                    int currentMaterialInt = static_cast<int>(currentMaterial);
                    currentMaterialInt = (currentMaterialInt + 1) % static_cast<int>(PixelPhys::MaterialType::COUNT);
                    currentMaterial = static_cast<PixelPhys::MaterialType>(currentMaterialInt);
                    
                    // Skip "Empty" material
                    if (currentMaterial == PixelPhys::MaterialType::Empty) {
                        currentMaterialInt = (currentMaterialInt + 1) % static_cast<int>(PixelPhys::MaterialType::COUNT);
                        currentMaterial = static_cast<PixelPhys::MaterialType>(currentMaterialInt);
                    }
                    
                    // std::cout << "Current material: " << static_cast<int>(currentMaterial) << std::endl;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_MIDDLE) {
                    // Stop panning
                    middleMouseDown = false;
                }
                else if (e.button.button == SDL_BUTTON_LEFT) {
                    // Stop placing material
                    leftMouseDown = false;
                }
            }
            else if (e.type == SDL_MOUSEMOTION) {
                // Track current mouse position
                mouseX = e.motion.x;
                mouseY = e.motion.y;
                
                // Handle panning with middle mouse button
                if (middleMouseDown) {
                    // Simple direct camera movement based on mouse movement
                    int dx = mouseX - prevMouseX;
                    int dy = mouseY - prevMouseY;
                    
                    // Move camera opposite to mouse movement (standard pan behavior)
                    cameraX -= dx;
                    cameraY -= dy;
                    
                    // Ensure camera doesn't go out of bounds
                    cameraX = std::max(0, cameraX);
                    cameraY = std::max(0, cameraY);
                    // Simple right/bottom boundary checks - allow scrolling further down
                    cameraX = std::min(cameraX, WORLD_WIDTH - actualWidth);
                    cameraY = std::min(cameraY, WORLD_HEIGHT - 50);
                    
                    // Update active chunks based on new camera position (streaming system) - adjust for pixel size
                    world.updatePlayerPosition(cameraX + actualWidth/PIXEL_SIZE/2, cameraY + actualHeight/PIXEL_SIZE/2);
                    
                    // Update previous mouse position
                    prevMouseX = mouseX;
                    prevMouseY = mouseY;
                }
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_Vulkan_GetDrawableSize(window, &actualWidth, &actualHeight);
                // std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
            }
        }
        SDL_GetMouseState(&mouseX, &mouseY);
        
        // Convert mouse screen coordinates to world coordinates
        // Use global pixel size for consistency
        int worldX = cameraX + static_cast<int>(mouseX / PIXEL_SIZE);
        int worldY = cameraY + static_cast<int>(mouseY / PIXEL_SIZE);
        
        // Handle player movement if in player mode
        if (playerMode && character) {
            // Update character position based on mouse position
            character->updatePosition(worldX, worldY);
            
            // Get character position
            int charX = character->getX();
            int charY = character->getY();
            
            // Calculate target camera position (centered on character)
            int targetCameraX = charX - static_cast<int>(actualWidth / (2 * PIXEL_SIZE));
            int targetCameraY = charY - static_cast<int>(actualHeight / (2 * PIXEL_SIZE));
            
            // Clamp target camera position to world bounds
            targetCameraX = std::max(0, targetCameraX);
            targetCameraX = std::min(WORLD_WIDTH - actualWidth, targetCameraX);
            targetCameraY = std::max(0, targetCameraY);
            targetCameraY = std::min(WORLD_HEIGHT - 50, targetCameraY);
            
            // Smooth camera movement using interpolation
            // Calculate the interpolation factor - smaller for smoother movement
            const float smoothFactor = 0.1f; 
            
            // Interpolate between current and target camera positions
            cameraX = cameraX + static_cast<int>((targetCameraX - cameraX) * smoothFactor);
            cameraY = cameraY + static_cast<int>((targetCameraY - cameraY) * smoothFactor);
            
            // Update world player position for chunk streaming (use character position, not camera)
            world.updatePlayerPosition(charX, charY);
        }
        else {
            // In camera mode, use the center of the screen as the focus point for chunk streaming
            int centerX = cameraX + static_cast<int>(actualWidth / (2 * PIXEL_SIZE));
            int centerY = cameraY + static_cast<int>(actualHeight / (2 * PIXEL_SIZE));
            
            // Update chunks based on screen center position, but do it less frequently
            // to reduce file I/O overhead (only update every 5 frames)
            static int updateCounter = 0;
            if (updateCounter++ % 5 == 0) {
                world.updatePlayerPosition(centerX, centerY);
            }
            
            // Handle material placement in camera mode
            if (leftMouseDown) {
                // Place material in a circular brush pattern
                for (int dy = -placeBrushSize / 2; dy <= placeBrushSize / 2; dy++) {
                    for (int dx = -placeBrushSize / 2; dx <= placeBrushSize / 2; dx++) {
                        // Check if point is within brush radius for circular brush shape
                        if (dx*dx + dy*dy <= (placeBrushSize/2)*(placeBrushSize/2)) {
                            // Calculate world coordinates with brush offset
                            int placeX = worldX + dx;
                            int placeY = worldY + dy;
                            
                            // Ensure we're within world bounds
                            if (placeX >= 0 && placeX < WORLD_WIDTH && placeY >= 0 && placeY < WORLD_HEIGHT) {
                                world.set(placeX, placeY, currentMaterial);
                            }
                        }
                    }
                }
            }
            
            // Debug output - only show occasionally to avoid console spam
            if (frameCount % 60 == 0) {
                // std::cout << "Mouse at screen: " << mouseX << "," << mouseY 
               //            << " | World: " << worldX << "," << worldY 
               //            << " | Camera: " << cameraX << "," << cameraY 
              //             << " | Zoom: 4x | Brush size: " << placeBrushSize << std::endl;
            }
        }
        // Update the world physics - performance bottleneck
        world.update();
        
        // Render the world using our renderer with camera position
        renderer->render(world, cameraX, cameraY);
        
        // Display current material and brush size in status bar
        std::string materialName = "Unknown";
        auto it = materialNames.find(currentMaterial);
        if (it != materialNames.end()) {
            materialName = it->second;
        }
        // FPS calculation (print FPS every second)
        frameCount++;
        if (SDL_GetTicks() - fpsTimer >= 1000) {
            std::cout << "\nFPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsTimer = SDL_GetTicks();
        }
        
        // Frame rate cap
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }
    
    // Save world state before exiting
    // std::cout << "Saving world state..." << std::endl;
    world.save();
    
    // Clean up resources
    // std::cout << "Cleaning up resources" << std::endl;
    renderer.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
