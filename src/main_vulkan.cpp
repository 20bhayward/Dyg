#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

#include "../include/Materials.h"
#include "../include/World.h"
#include "../include/Renderer.h"
#include "../include/VulkanBackend.h"

const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;
const int WORLD_WIDTH   = 2400;    // World dimensions in world units
const int WORLD_HEIGHT  = 1800;
const int TARGET_FPS    = 60;
const int FRAME_DELAY   = 1000 / TARGET_FPS;

// Camera parameters
int cameraX = 0;         // Camera position X
int cameraY = 0;         // Camera position Y
const int CAMERA_SPEED = 15;  // Camera movement speed (pixels per key press)

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
                // Camera movement
                else if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_a) {
                    cameraX -= CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraX = std::max(0, cameraX);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) {
                    cameraX += CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraX = std::min(WORLD_WIDTH - actualWidth, cameraX);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_w) {
                    cameraY -= CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraY = std::max(0, cameraY);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_s) {
                    cameraY += CAMERA_SPEED;
                    // Clamp camera position to world bounds
                    cameraY = std::min(WORLD_HEIGHT - actualHeight, cameraY);
                    std::cout << "Camera position: " << cameraX << "," << cameraY << std::endl;
                }
                // Reset camera position
                else if (e.key.keysym.sym == SDLK_HOME) {
                    cameraX = 0;
                    cameraY = 0;
                    std::cout << "Camera reset to origin" << std::endl;
                }
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GetWindowSize(window, &actualWidth, &actualHeight);
                std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
            }
        }
        
        // Update world simulation
        world.update();
        
        // Render the world using our renderer with camera position
        renderer->render(world, cameraX, cameraY);
        
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
