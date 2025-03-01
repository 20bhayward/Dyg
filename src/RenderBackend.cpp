#include "../include/RenderBackend.h"
#include "../include/VulkanBackend.h"
#include <iostream>

namespace PixelPhys {

std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int screenWidth, int screenHeight) {
    std::unique_ptr<RenderBackend> backend;
    
    switch (type) {
        case BackendType::Vulkan:
            backend = std::make_unique<VulkanBackend>(screenWidth, screenHeight);
            std::cout << "Created Vulkan rendering backend" << std::endl;
            std::cerr << "Vulkan backend not available" << std::endl;
            break;
            
        default:
            std::cerr << "Unknown backend type requested" << std::endl;
            break;
    }
    
    return backend;
}

} // namespace PixelPhys