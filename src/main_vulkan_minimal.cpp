#include <iostream>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include "../include/VulkanBackend.h"

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    std::cout << "Initializing minimal Vulkan test..." << std::endl;
    
    // Create window with Vulkan support - explicitly use SDL_WINDOW_VULKAN
    SDL_Window* window = SDL_CreateWindow(
        "Vulkan Minimal Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Print window ID and flags for debugging
    std::cout << "Window ID: " << SDL_GetWindowID(window) << std::endl;
    std::cout << "Window flags: " << SDL_GetWindowFlags(window) << std::endl;
    
    // Create a Vulkan backend instance
    PixelPhys::VulkanBackend backend(WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // Try to initialize the backend
    if (backend.initialize()) {
        std::cout << "Vulkan backend initialized successfully!" << std::endl;
        std::cout << "Renderer info: " << backend.getRendererInfo() << std::endl;
    } else {
        std::cerr << "Failed to initialize Vulkan backend!" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Main loop
    bool quit = false;
    SDL_Event e;
    
    while (!quit) {
        // Process events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }
        
        // Clear screen (with a simple color to show something is happening)
        backend.beginFrame();
        backend.endFrame();
        
        // Small delay to avoid 100% CPU usage
        SDL_Delay(16); // ~60fps
    }
    
    // Cleanup
    backend.cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}