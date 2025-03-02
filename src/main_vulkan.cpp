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

const int WINDOW_WIDTH  = 1980;
const int WINDOW_HEIGHT = 1080;
// Test mode with smaller world for physics testing
const bool TEST_MODE = false;

// World dimensions - use smaller world in test mode
const int WORLD_WIDTH   = TEST_MODE ? 800 : 6000;  // Much smaller for test mode
const int WORLD_HEIGHT  = TEST_MODE ? 600 : 1800;  // Much smaller for test mode
const int TARGET_FPS    = 120;  // Higher FPS for smoother simulation
const int FRAME_DELAY   = 1000 / TARGET_FPS;

// Camera parameters
int cameraX = 0;         // Camera position X
int cameraY = 0;         // Camera position Y
float zoomLevel = 1.0f;  // Zoom level (1.0 = normal, >1.0 = zoomed in, <1.0 = zoomed out)
const float MIN_ZOOM = 0.25f;  // Minimum zoom level
const float MAX_ZOOM = 4.0f;   // Maximum zoom level
const float ZOOM_STEP = 0.1f;  // Zoom amount per scroll
const int CAMERA_SPEED = 15;  // Camera movement speed (pixels per key press)
const int DEFAULT_VIEW_HEIGHT = 450; // Default height to position camera at start
const float basePixelSize = 1.0f;  // Base pixel size for world rendering (smaller for more detail)

// Mouse parameters
bool middleMouseDown = false;
int mouseX = 0, mouseY = 0;
int prevMouseX = 0, prevMouseY = 0;

int main() {
    // Initialize SDL with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " 
                  << SDL_GetError() << std::endl;
        return 1;
    }
    
    std::cout << "Initializing with Vulkan backend" << std::endl;
    
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
    std::cout << "Drawable size (after going fullscreen): "
              << actualWidth << "x" << actualHeight << std::endl;
    
    // Create the world and generate terrain or simple test environment
    PixelPhys::World world(WORLD_WIDTH, WORLD_HEIGHT);
    
    if (TEST_MODE) {
        // Create a simple test environment instead of a full world
        std::cout << "Creating test environment for physics testing..." << std::endl;
        
        // Create a flat bottom platform across the world
        for (int x = 0; x < WORLD_WIDTH; x++) {
            for (int y = WORLD_HEIGHT - 50; y < WORLD_HEIGHT; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
        
        // Add a few platforms for testing material interactions
        for (int x = 100; x < 300; x++) {
            for (int y = WORLD_HEIGHT - 150; y < WORLD_HEIGHT - 140; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
        
        for (int x = 500; x < 700; x++) {
            for (int y = WORLD_HEIGHT - 250; y < WORLD_HEIGHT - 240; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
        
        // Add a "funnel" in the middle for testing
        for (int x = 350; x < 450; x++) {
            // Left slope
            int slopeHeight = (x - 350) / 2;
            for (int y = WORLD_HEIGHT - 200; y < WORLD_HEIGHT - 200 + slopeHeight; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
        
        // Right side funnel
        for (int x = 450; x < 550; x++) {
            // Right slope (descending)
            int slopeHeight = (550 - x) / 2;
            for (int y = WORLD_HEIGHT - 200; y < WORLD_HEIGHT - 200 + slopeHeight; y++) {
                world.set(x, y, PixelPhys::MaterialType::Stone);
            }
        }
    } else {
        // Generate regular terrain for normal mode
        unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
        std::cout << "Generating world with seed: " << seed << std::endl;
        world.generate(seed);
    }
    
    SDL_Delay(200);  // Give the world time to set up
    
    // Position camera for best view of the test area
    if (TEST_MODE) {
        cameraX = 0; // We want to see the whole test area
        cameraY = 0; // Start at the top
        zoomLevel = 1.0f; // Full default zoom to see details
    } else {
        // Normal world position
        cameraX = WORLD_WIDTH / 4;
        cameraY = 0;
        zoomLevel = 0.75f;
    }
    
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
                    std::cout << "World reset with seed: " << seed << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_F11) {
                    // Toggle fullscreen mode
                    Uint32 flags = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, flags ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    SDL_Vulkan_GetDrawableSize(window, &actualWidth, &actualHeight);
                    std::cout << "Window resized to " << actualWidth << "x" << actualHeight << std::endl;
                }
                // Camera movement - adjust speed based on zoom level (move faster when zoomed out)
                else if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_a) {
                    // Move faster when zoomed out, slower when zoomed in
                    int adjustedSpeed = static_cast<int>(CAMERA_SPEED / zoomLevel);
                    cameraX -= adjustedSpeed;
                    // Clamp camera position to world bounds
                    cameraX = std::max(0, cameraX);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) {
                    int adjustedSpeed = static_cast<int>(CAMERA_SPEED / zoomLevel);
                    cameraX += adjustedSpeed;
                    // Clamp camera position to world bounds
                    cameraX = std::min(WORLD_WIDTH - static_cast<int>(actualWidth / (basePixelSize * zoomLevel)), cameraX);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_w) {
                    int adjustedSpeed = static_cast<int>(CAMERA_SPEED / zoomLevel);
                    cameraY -= adjustedSpeed; // Up key moves camera up (decreases Y)
                    // Clamp camera position to world bounds
                    cameraY = std::max(0, cameraY);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_s) {
                    int adjustedSpeed = static_cast<int>(CAMERA_SPEED / zoomLevel);
                    cameraY += adjustedSpeed; // Down key moves camera down (increases Y)
                    // Clamp camera position to world bounds
                    cameraY = std::min(WORLD_HEIGHT - static_cast<int>(actualHeight / (basePixelSize * zoomLevel)), cameraY);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                // Reset camera position
                else if (e.key.keysym.sym == SDLK_HOME) {
                    if (TEST_MODE) {
                        cameraX = 0; // Reset to origin for test world
                        cameraY = 0;
                        zoomLevel = 1.0f;
                    } else {
                        cameraX = WORLD_WIDTH / 4; // Reset to a good viewing position
                        cameraY = 0; // Top of the world
                        zoomLevel = 0.75f; // Slightly zoomed out
                    }
                    std::cout << "Camera reset to default position" << std::endl;
                }
                // Brush size controls
                else if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS) {
                    // Increase brush size
                    placeBrushSize = std::min(20, placeBrushSize + 1);
                    std::cout << "Brush size: " << placeBrushSize << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_MINUS) {
                    // Decrease brush size
                    placeBrushSize = std::max(1, placeBrushSize - 1);
                    std::cout << "Brush size: " << placeBrushSize << std::endl;
                }
                // Material selection hotkeys
                else if (e.key.keysym.sym == SDLK_1) {
                    currentMaterial = PixelPhys::MaterialType::Sand;
                    std::cout << "Selected Sand" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_2) {
                    currentMaterial = PixelPhys::MaterialType::Water;
                    std::cout << "Selected Water" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_3) {
                    currentMaterial = PixelPhys::MaterialType::Stone;
                    std::cout << "Selected Stone" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_4) {
                    currentMaterial = PixelPhys::MaterialType::Gravel;
                    std::cout << "Selected Gravel" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_5) {
                    currentMaterial = PixelPhys::MaterialType::Oil;
                    std::cout << "Selected Oil" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_6) {
                    currentMaterial = PixelPhys::MaterialType::Lava;
                    std::cout << "Selected Lava" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_7) {
                    currentMaterial = PixelPhys::MaterialType::Fire;
                    std::cout << "Selected Fire" << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_0) {
                    // Eraser - set to Empty
                    currentMaterial = PixelPhys::MaterialType::Empty;
                    std::cout << "Selected Eraser (Empty)" << std::endl;
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
                    std::cout << "Created test column of sand" << std::endl;
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
                    std::cout << "Created test water pool" << std::endl;
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
                    std::cout << "Created material comparison test" << std::endl;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                // Zoom in/out with mouse wheel
                float prevZoom = zoomLevel;
                if (e.wheel.y > 0) {
                    // Zoom in - use multiplication for smoother zoom feel
                    zoomLevel *= 1.1f;
                    if (zoomLevel > MAX_ZOOM) zoomLevel = MAX_ZOOM;
                } else if (e.wheel.y < 0) {
                    // Zoom out - use division for smoother zoom feel
                    zoomLevel /= 1.1f;
                    if (zoomLevel < MIN_ZOOM) zoomLevel = MIN_ZOOM;
                }
                
                if (prevZoom != zoomLevel) {
                    // Get current mouse position
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    
                    // Calculate world position under cursor before zoom
                    float worldX = x;
                    float worldY = y;
                    
                    // Adjust camera to keep world position under cursor
                    cameraX = static_cast<int>(x / (basePixelSize * zoomLevel));
                    cameraY = static_cast<int>(y / (basePixelSize * zoomLevel));
                    
                    // Clamp camera position
                    cameraX = std::max(0, cameraX);
                    cameraX = std::min(WORLD_WIDTH - static_cast<int>(actualWidth / (basePixelSize * zoomLevel)), cameraX);
                    cameraY = std::max(0, cameraY);
                    cameraY = std::min(WORLD_HEIGHT - static_cast<int>(actualHeight / (basePixelSize * zoomLevel)), cameraY);
                    
                    std::cout << "Zoom: " << zoomLevel << ", Camera: " << cameraX << "," << cameraY << std::endl;
                }
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
                    
                    std::cout << "Current material: " << static_cast<int>(currentMaterial) << std::endl;
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
                    int dx = (mouseX - prevMouseX);
                    int dy = (mouseY - prevMouseY);
                    
                    // Convert screen space delta to world space
                    // Invert both axes - moving mouse in a direction moves camera in opposite direction
                    // Calculate total movement in both axes simultaneously
                    int deltaX = static_cast<int>(dx / (basePixelSize * zoomLevel));
                    int deltaY = static_cast<int>(dy / (basePixelSize * zoomLevel));
                    
                    // Apply movement in both directions at once
                    // Moving the mouse to the right should move the camera view to the left (panning right)
                    // Moving the mouse down should move the camera view up (panning down)
                    cameraX -= deltaX; // Standard camera panning in X direction
                    cameraY -= deltaY; // Standard camera panning in Y direction
                    
                    // Clamp camera position
                    cameraX = std::max(0, cameraX);
                    cameraX = std::min(WORLD_WIDTH - static_cast<int>(actualWidth / (basePixelSize * zoomLevel)), cameraX);
                    cameraY = std::max(0, cameraY);
                    cameraY = std::min(WORLD_HEIGHT - static_cast<int>(actualHeight / (basePixelSize * zoomLevel)), cameraY);
                    
                    // Update previous mouse position
                    prevMouseX = mouseX;
                    prevMouseY = mouseY;
                }
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_Vulkan_GetDrawableSize(window, &actualWidth, &actualHeight);
                std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
            }
        }
        SDL_GetMouseState(&mouseX, &mouseY);
        // Handle material placement with left mouse button
        if (leftMouseDown) {
            
            // Convert screen coordinates to world coordinates
            // Try a different approach to screen-to-world conversion
            // The logic used in the Renderer would be:
            // viewportWidth = screenWidth / pixelScale
            // viewportHeight = screenHeight / pixelScale
            // So we need to reverse this logic
            
            // Get the actual viewport dimensions in world units
            float viewportWidth = actualWidth / (zoomLevel);
            float viewportHeight = actualHeight / (zoomLevel);
            
            // Calculate percentages across the viewport
            float percentX = static_cast<float>(mouseX) / actualWidth;
            float percentY = static_cast<float>(mouseY) / actualHeight;
            
            // Apply percentages to get position in world space
            int worldX = cameraX + static_cast<int>(percentX * viewportWidth);
            int worldY = cameraY + static_cast<int>(percentY * viewportHeight);
            
            // Place material in a circular brush pattern
            for (int dy = -placeBrushSize / 2; dy <= placeBrushSize / 2; dy++) {
                for (int dx = -placeBrushSize / 2; dx <= placeBrushSize / 2; dx++) {
                    // Check if point is within brush radius for circular brush shape
                    if (dx*dx + dy*dy <= (placeBrushSize/2)*(placeBrushSize/2)) {
                        world.set(mouseX, mouseY, currentMaterial);
                    }
                }
            }
            
            // Debug output - only show occasionally to avoid console spam
            if (frameCount % 100 == 0) {
                std::cout << "\nMouse at screen: " << mouseX << "," << mouseY 
                          << " | World: " << worldX << "," << worldY 
                          << " | Camera: " << cameraX << "," << cameraY 
                          << " | Zoom: " << zoomLevel 
                          << " | ViewportSize: " << viewportWidth << "x" << viewportHeight << std::endl;
            }
        }
        world.update();
        
        // Render the world using our renderer with camera position and zoom level
        renderer->render(world, cameraX, cameraY, zoomLevel);
        
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
    
    // Clean up resources
    std::cout << "Cleaning up resources" << std::endl;
    renderer.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
