#pragma once

#include "World.h"
#include "RenderBackend.h"
#include "RenderResources.h"
#include <memory>
#include <string>

namespace PixelPhys {

class Renderer {
public:
    Renderer(int screenWidth, int screenHeight, BackendType type = BackendType::Vulkan);
    ~Renderer();
    
    bool initialize();
    bool initialize(SDL_Window* window);
    void render(const World& world);
    void cleanup();

private:
    int m_screenWidth;
    int m_screenHeight;
    
    std::unique_ptr<RenderBackend> m_backend;
    std::shared_ptr<Texture> m_worldTexture;

    void getMaterialColor(MaterialType material, float& r, float& g, float& b);
    void updateWorldTexture(const World& world);
};

} // namespace PixelPhys