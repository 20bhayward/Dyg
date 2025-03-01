#include <iostream>
#include <SDL2/SDL.h>
#include "include/Materials.h"

// Simple direct fire color visualization test
int main() {
    using namespace PixelPhys;
    
    std::cout << "Material Type Numbers:" << std::endl;
    std::cout << "Empty = " << static_cast<int>(MaterialType::Empty) << std::endl;
    std::cout << "Sand = " << static_cast<int>(MaterialType::Sand) << std::endl;
    std::cout << "Water = " << static_cast<int>(MaterialType::Water) << std::endl;
    std::cout << "Stone = " << static_cast<int>(MaterialType::Stone) << std::endl;
    std::cout << "Wood = " << static_cast<int>(MaterialType::Wood) << std::endl;
    std::cout << "Fire = " << static_cast<int>(MaterialType::Fire) << std::endl;
    std::cout << "Oil = " << static_cast<int>(MaterialType::Oil) << std::endl;
    
    // Display fire color from materials properties
    auto& fireProps = MATERIAL_PROPERTIES[static_cast<std::size_t>(MaterialType::Fire)];
    std::cout << "\nFire Color in Material Properties:" << std::endl;
    std::cout << "R: " << static_cast<int>(fireProps.r) << " (" << (fireProps.r / 255.0f) << ")" << std::endl;
    std::cout << "G: " << static_cast<int>(fireProps.g) << " (" << (fireProps.g / 255.0f) << ")" << std::endl;
    std::cout << "B: " << static_cast<int>(fireProps.b) << " (" << (fireProps.b / 255.0f) << ")" << std::endl;
    
    // Initialize SDL for a simple visualization
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create a small window to show fire color
    SDL_Window* window = SDL_CreateWindow(
        "Fire Color Test", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        200, 200,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Draw with different fire colors
    SDL_Event e;
    bool quit = false;
    int option = 0;
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    option = (option + 1) % 4;
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Draw fire color based on option
        switch (option) {
            case 0: {
                // Material properties color
                SDL_SetRenderDrawColor(
                    renderer, 
                    fireProps.r, 
                    fireProps.g, 
                    fireProps.b, 
                    255
                );
                SDL_Rect rect = {50, 50, 100, 100};
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
            case 1: {
                // Hardcoded bright orange-red
                SDL_SetRenderDrawColor(renderer, 255, 102, 0, 255);
                SDL_Rect rect = {50, 50, 100, 100};
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
            case 2: {
                // Hardcoded yellow-orange
                SDL_SetRenderDrawColor(renderer, 255, 153, 51, 255);
                SDL_Rect rect = {50, 50, 100, 100};
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
            case 3: {
                // Hardcoded red
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_Rect rect = {50, 50, 100, 100};
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
        }
        
        // Show which color is being displayed
        std::string colorName;
        switch (option) {
            case 0: colorName = "Material Properties"; break;
            case 1: colorName = "Bright Orange-Red"; break;
            case 2: colorName = "Yellow-Orange"; break;
            case 3: colorName = "Pure Red"; break;
        }
        
        std::cout << "\rCurrent color: " << colorName << "   " << std::flush;
        
        // Render to screen
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }
    
    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}