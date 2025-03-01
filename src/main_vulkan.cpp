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
            }
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GetWindowSize(window, &actualWidth, &actualHeight);
                std::cout << "Window manually resized to " << actualWidth << "x" << actualHeight << std::endl;
            }
        }
        
        // Update world simulation
        world.update();
        
        // Begin rendering a new frame
        renderer->beginFrame();
        
        // Clear the screen with a background color (dark blue-ish)
        renderer->getBackend()->setClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        
        // Access the Vulkan backend for drawing operations
        auto vulkanBackend = static_cast<PixelPhys::VulkanBackend*>(renderer->getBackend());
        
        // Draw the entire world as a grid of cells.
        // For demonstration, each world cell is drawn as a small rectangle.
        const float cellSize = 3.0f;  // Size of each cell on screen (in pixels)
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            for (int x = 0; x < WORLD_WIDTH; ++x) {
                PixelPhys::MaterialType material = world.get(x, y);
                if (material == PixelPhys::MaterialType::Empty)
                    continue; // Skip empty cells
                
                int index = static_cast<int>(material);
                // Use MATERIAL_PROPERTIES to determine the color.
                const auto& props = PixelPhys::MATERIAL_PROPERTIES[index];
                float r = props.r / 255.0f;
                float g = props.g / 255.0f;
                float b = props.b / 255.0f;
                
                // Draw the cell at the appropriate screen position
                vulkanBackend->drawRectangle(
                    x * cellSize, y * cellSize,
                    cellSize, cellSize,
                    r, g, b
                );
            }
        }
        
        // End the frame (present it)
        renderer->endFrame();
        
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
