#include "../include/RenderBackend.h"
#include "../include/OpenGLBackend.h"
#ifdef USE_VULKAN
#include "../include/VulkanBackend.h"
#endif
#ifdef _WIN32
#include "../include/DirectX12Backend.h"
#endif
#include <iostream>

namespace PixelPhys {

std::unique_ptr<RenderBackend> CreateRenderBackend(BackendType type, int screenWidth, int screenHeight) {
    std::unique_ptr<RenderBackend> backend;
    
    switch (type) {
        case BackendType::OpenGL:
#ifndef USE_VULKAN
            backend = std::make_unique<OpenGLBackend>(screenWidth, screenHeight);
            std::cout << "Created OpenGL rendering backend" << std::endl;
#else
            std::cerr << "OpenGL backend not available in Vulkan build" << std::endl;
#endif
            break;
            
        case BackendType::Vulkan:
#ifdef USE_VULKAN
            backend = std::make_unique<VulkanBackend>(screenWidth, screenHeight);
            std::cout << "Created Vulkan rendering backend" << std::endl;
#else
            std::cerr << "Vulkan backend not available" << std::endl;
#endif
            break;
            
        case BackendType::DirectX12:
#ifdef _WIN32
            backend = std::make_unique<DirectX12Backend>(screenWidth, screenHeight);
            std::cout << "Created DirectX 12 rendering backend" << std::endl;
#else
            std::cerr << "DirectX 12 backend not available on this platform" << std::endl;
#endif
            break;
            
        default:
            std::cerr << "Unknown backend type requested" << std::endl;
            break;
    }
    
    return backend;
}

} // namespace PixelPhys