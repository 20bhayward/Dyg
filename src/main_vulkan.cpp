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

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WORLD_WIDTH = 2400;    // Expanded world width for more horizontal exploration
const int WORLD_HEIGHT = 1800;   // Deeper world for vertical exploration
const int TARGET_FPS = 60;
const int FRAME_DELAY = 1000 / TARGET_FPS;

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
    
    // Generate initial terrain with a random seed
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    world.generate(seed);
    
    // Force a short delay to ensure the world is fully generated before spawning player
    SDL_Delay(200);
    
    // Create the renderer with the actual window size
    PixelPhys::Renderer renderer(actualWidth, actualHeight);
    if (!renderer.initialize()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    std::cout << "Renderer info: " << renderer.getRendererInfo() << std::endl;
    
    // Show cursor
    SDL_ShowCursor(SDL_ENABLE);
    
    // Main loop flag
    bool quit = false;
    
    // Event handler
    SDL_Event e;
    
    // Show initial info
    std::cout << "Vulkan backend initialized." << std::endl;
    std::cout << "Press ESC to quit." << std::endl;
    
    // For timing
    Uint32 frameStart;
    Uint32 frameTime;
    
    // For FPS calculation
    int frameCount = 0;
    Uint32 fpsTimer = SDL_GetTicks();
    
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
                }
            }
        }
        
        // Render the world (this will use the Vulkan backend)
        renderer.render(world);
        
        // FPS calculation
        frameCount++;
        if (SDL_GetTicks() - fpsTimer >= 1000) {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsTimer = SDL_GetTicks();
        }
        
        // Delay to cap framerate
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }
    
    // Clean up
    renderer.cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}