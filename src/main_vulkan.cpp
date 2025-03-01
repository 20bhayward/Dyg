#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>

#include "../include/Materials.h"
#include "../include/World.h"
#include "../include/Renderer.h"
#include "../include/VulkanBackend.h"

const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;
const int WORLD_WIDTH   = 6000;    // World dimensions in world units (wider for multiple biomes)
const int WORLD_HEIGHT  = 1800;
const int TARGET_FPS    = 60;
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
const float basePixelSize = 2.0f;  // Base pixel size for world rendering (matches Renderer.cpp)

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
    
    // Optionally, set fullscreen (desktop mode)
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    int actualWidth, actualHeight;
    SDL_GetWindowSize(window, &actualWidth, &actualHeight);
    
    // Create the world and generate terrain
    PixelPhys::World world(WORLD_WIDTH, WORLD_HEIGHT);
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    std::cout << "Generating world with seed: " << seed << std::endl;
    world.generate(seed);
    SDL_Delay(200);  // Give the world time to generate
    
    // Position camera at default position - centered on the grasslands biome, at a good viewing height
    cameraX = WORLD_WIDTH / 3; // Start in the grasslands (middle third of the world)
    cameraY = DEFAULT_VIEW_HEIGHT;
    
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
                    seed = static_cast<unsigned int>(std::time(nullptr));
                    world.generate(seed);
                    std::cout << "World reset with seed: " << seed << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_F11) {
                    // Toggle fullscreen mode
                    Uint32 flags = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, flags ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    SDL_GetWindowSize(window, &actualWidth, &actualHeight);
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
                    cameraX = WORLD_WIDTH / 3; // Reset to grasslands (middle third)
                    cameraY = DEFAULT_VIEW_HEIGHT;
                    zoomLevel = 1.0f;
                    std::cout << "Camera reset to default position" << std::endl;
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
                    float worldX = cameraX + x / (basePixelSize * prevZoom);
                    float worldY = cameraY + y / (basePixelSize * prevZoom);
                    
                    // Adjust camera to keep world position under cursor
                    cameraX = static_cast<int>(worldX - x / (basePixelSize * zoomLevel));
                    cameraY = static_cast<int>(worldY - y / (basePixelSize * zoomLevel));
                    
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
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_MIDDLE) {
                    // Stop panning
                    middleMouseDown = false;
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
                SDL_GetWindowSize(window, &actualWidth, &actualHeight);
                std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
            }
        }
        
        // Update world simulation
        world.update();
        
        // Render the world using our renderer with camera position and zoom level
        renderer->render(world, cameraX, cameraY, zoomLevel);
        
        // FPS calculation (print FPS every second)
        frameCount++;
        if (SDL_GetTicks() - fpsTimer >= 1000) {
            std::cout << "FPS: " << frameCount << std::endl;
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
