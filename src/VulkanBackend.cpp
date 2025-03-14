#include "VulkanBackend.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <algorithm>
#include <optional>
#include <SDL2/SDL.h>

// Define a debug macro that can be disabled
// Set to 0 to disable all  output, 1 to enable only critical debug, 2 for verbose
#define VULKAN_DEBUG_LEVEL 0

// Completely disable all debug output
#define VULKAN_DEBUG(msg)
#define VULKAN_DEBUG_VERBOSE(msg)

namespace PixelPhys {

// Vulkan helper structs
struct VulkanQueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct VulkanSwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Debug messenger callback for validation layers
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    // Only output errors and warnings to reduce noise
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    }
    return VK_FALSE;
}

// Create debug messenger
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Destroy debug messenger
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// VulkanBackend implementation
VulkanBackend::VulkanBackend(int screenWidth, int screenHeight)
    : RenderBackend(screenWidth, screenHeight),
      m_instance(VK_NULL_HANDLE),
      m_physicalDevice(VK_NULL_HANDLE),
      m_device(VK_NULL_HANDLE),
      m_surface(VK_NULL_HANDLE),
      m_swapChain(VK_NULL_HANDLE),
      m_defaultRenderPass(VK_NULL_HANDLE),
      m_commandPool(VK_NULL_HANDLE),
      m_currentFrame(0),
      m_renderPassInProgress(false),
      m_instanceBuffer(VK_NULL_HANDLE),
      m_instanceBufferMemory(VK_NULL_HANDLE),
      m_maxInstanceCount(100000),
      m_isBatchActive(false) {
    
    m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    m_viewport = {{0, 0}, {static_cast<uint32_t>(screenWidth), static_cast<uint32_t>(screenHeight)}};
    m_debugMessenger = VK_NULL_HANDLE;
    
    // Reserve space for pixel batch
    m_pixelBatch.reserve(m_maxInstanceCount);
    
    // std::cout << "Created Vulkan backend" << std::endl;
}

VulkanBackend::~VulkanBackend() {
    cleanup();
}

bool VulkanBackend::initialize() {
    // std::cout << "Initializing Vulkan backend" << std::endl;
    
    // Initialize Vulkan instance
    if (!createInstance()) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    // Setup debug messenger if validation layers are enabled
    if (!setupDebugMessenger()) {
        std::cerr << "Failed to setup debug messenger" << std::endl;
        return false;
    }
    
    // Create window surface
    if (!createSurface()) {
        std::cerr << "Failed to create window surface" << std::endl;
        return false;
    }
    
    // Pick physical device (GPU)
    if (!pickPhysicalDevice()) {
        std::cerr << "Failed to find a suitable GPU" << std::endl;
        return false;
    }
    
    // Create logical device
    if (!createLogicalDevice()) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    // Create swap chain
    if (!createSwapChain()) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }
    
    // Create image views
    if (!createImageViews()) {
        std::cerr << "Failed to create image views" << std::endl;
        return false;
    }
    
    // Create render pass
    if (!createRenderPass()) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }
    
    // Create framebuffers
    if (!createFramebuffers()) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return false;
    }
    
    // Create command pool
    if (!createCommandPool()) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }
    
    // Create command buffers
    if (!createCommandBuffers()) {
        std::cerr << "Failed to create command buffers" << std::endl;
        return false;
    }
    
    // Create synchronization objects
    if (!createSyncObjects()) {
        std::cerr << "Failed to create synchronization objects" << std::endl;
        return false;
    }
    
    // Create a fullscreen quad for post-processing
    if (!createFullscreenQuad()) {
        std::cerr << "Failed to create fullscreen quad" << std::endl;
        return false;
    }
    
    // std::cout << "Vulkan backend initialized successfully" << std::endl;
    return true;
}

void VulkanBackend::cleanup() {
    // std::cout << "Cleaning up Vulkan resources" << std::endl;
    
    // Only call vkDeviceWaitIdle if we have a valid device
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        
        // Cleanup batched rendering resources
        if (m_instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
            m_instanceBuffer = VK_NULL_HANDLE;
        }
        
        if (m_instanceBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_instanceBufferMemory, nullptr);
            m_instanceBufferMemory = VK_NULL_HANDLE;
        }
        
        // Clean up specialized pipeline resources
        if (m_batchPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_batchPipeline, nullptr);
            m_batchPipeline = VK_NULL_HANDLE;
        }
        
        if (m_batchPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_batchPipelineLayout, nullptr);
            m_batchPipelineLayout = VK_NULL_HANDLE;
        }
        
        // Clear batch data
        m_pixelBatch.clear();
        m_isBatchActive = false;
        
        // Smart pointers will be cleaned up automatically
        m_batchVertexBuffer.reset();
        m_batchIndexBuffer.reset();
        
        // Cleanup fullscreen quad
        m_fullscreenQuadVertexBuffer.reset();
        m_fullscreenQuadIndexBuffer.reset();
        
        // Clean up synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT && i < m_renderFinishedSemaphores.size(); i++) {
            if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            }
            if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            }
            if (m_inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
            }
        }
        
        // Clean up command pool and buffers
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        }
        
        // Clean up framebuffers
        for (auto framebuffer : m_swapChainFramebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_device, framebuffer, nullptr);
            }
        }
        
        // Clean up render pass
        if (m_defaultRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_defaultRenderPass, nullptr);
        }
        
        // Clean up image views
        for (auto imageView : m_swapChainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, imageView, nullptr);
            }
        }
        
        // Clean up swap chain
        if (m_swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        }
        
        // Clean up device
        vkDestroyDevice(m_device, nullptr);
    }
    
    // Clean up debug messenger if it exists
    if (m_instance != VK_NULL_HANDLE && m_debugMessenger != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    
    // Clean up surface if it exists
    if (m_instance != VK_NULL_HANDLE && m_surface != VK_NULL_HANDLE) {
        // std::cout << "Destroying surface" << std::endl;
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    } else {
        // std::cout << "Failed to get SDL window" << std::endl;
    }
    
    // Clean up instance if it exists
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

void VulkanBackend::beginFrame() {
    // Reset render pass state at the beginning of a new frame
    m_renderPassInProgress = false;
    
    try {
        // Disable debug output for beginFrame
    // VULKAN_DEBUG("beginFrame - m_currentFrame: " << m_currentFrame);
        
        // Wait for the previous frame to finish
        if (m_device != VK_NULL_HANDLE && m_inFlightFences[m_currentFrame] != VK_NULL_HANDLE) {
            VULKAN_DEBUG_VERBOSE("Waiting for fence " << m_currentFrame);
            vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
            VULKAN_DEBUG_VERBOSE("Fence signaled and wait complete");
        } else {
            VULKAN_DEBUG("Skipping fence wait - null handles");
        }
        
        // Acquire the next image from the swap chain - use a timeout
        if (m_device != VK_NULL_HANDLE && m_swapChain != VK_NULL_HANDLE) {
            VULKAN_DEBUG_VERBOSE("Acquiring next swapchain image");
            
            // Use a 5-second timeout instead of UINT64_MAX
            VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, 5000000000, // 5 seconds in nanoseconds
                                                  m_imageAvailableSemaphores[m_currentFrame], 
                                                  VK_NULL_HANDLE, &m_currentImageIndex);
            
            VULKAN_DEBUG_VERBOSE("vkAcquireNextImageKHR result = " << result);
            
            // Check if swap chain needs to be recreated
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                VULKAN_DEBUG("Recreating swapchain due to OUT_OF_DATE or SUBOPTIMAL");
                recreateSwapChain();
                return;
            } else if (result != VK_SUCCESS) {
                std::cerr << "Failed to acquire swap chain image! Error: " << result << std::endl;
                
                // Skip this frame instead of crashing
                VULKAN_DEBUG("Skipping frame due to swapchain acquisition failure");
                return;
            }
            
            VULKAN_DEBUG_VERBOSE("Acquired image index: " << m_currentImageIndex);
            
            // Check if a previous frame is using this image (i.e. there is its fence to wait on)
            if (m_imagesInFlight[m_currentImageIndex] != VK_NULL_HANDLE) {
                VULKAN_DEBUG_VERBOSE("Waiting for image in flight fence");
                vkWaitForFences(m_device, 1, &m_imagesInFlight[m_currentImageIndex], VK_TRUE, UINT64_MAX);
                VULKAN_DEBUG_VERBOSE("Image in flight fence wait complete");
            } else {
                VULKAN_DEBUG_VERBOSE("No image in flight fence to wait on");
            }
            
            // Mark the image as now being in use by this frame
            m_imagesInFlight[m_currentImageIndex] = m_inFlightFences[m_currentFrame];
            VULKAN_DEBUG_VERBOSE("Image marked as in flight for frame " << m_currentFrame);
            
            // Reset fence to unsignaled state
            vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
            VULKAN_DEBUG_VERBOSE("Fence reset for frame " << m_currentFrame);
            
            // Reset command buffer
            vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
            VULKAN_DEBUG_VERBOSE("Command buffer reset for frame " << m_currentFrame);
            
            // Start recording the command buffer
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;
            
            VkResult cmdBeginResult = vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo);
            if (cmdBeginResult != VK_SUCCESS) {
                std::cerr << "Failed to begin recording command buffer! Error: " << cmdBeginResult << std::endl;
                return;
            }
            
            VULKAN_DEBUG_VERBOSE("Command buffer recording begun for frame " << m_currentFrame);
            
            // Start a default render pass to ensure we can at least render something
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_defaultRenderPass;
            renderPassInfo.framebuffer = m_swapChainFramebuffers[m_currentImageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapChainExtent;
            
            // Make sure the clear color is very vibrant to ensure we can see it
            // Use a multiplier to make colors more intense
            float colorMultiplier = 1.5f;
            float r = std::min(1.0f, m_clearColor[0] * colorMultiplier);
            float g = std::min(1.0f, m_clearColor[1] * colorMultiplier);
            float b = std::min(1.0f, m_clearColor[2] * colorMultiplier);
            
            // Set the clear values with intensified colors
            VkClearValue clearColor = {
                { {r, g, b, m_clearColor[3]} }
            };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;
            
            // Begin the render pass
            vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            m_renderPassInProgress = true;
            
            // Set viewport and scissor to match the swapchain
            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_swapChainExtent.width);
            viewport.height = static_cast<float>(m_swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
            
            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = m_swapChainExtent;
            vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);
            
            // // std::cout << "Frame " << m_currentFrame << " began with image index " << m_currentImageIndex << std::endl;
        } else {
            std::cerr << "Cannot begin frame - device or swapchain is null" << std::endl;
            if (m_device == VK_NULL_HANDLE) std::cerr << "  - Device handle is null" << std::endl;
            if (m_swapChain == VK_NULL_HANDLE) std::cerr << "  - Swapchain handle is null" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in beginFrame: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in beginFrame" << std::endl;
    }
}

void VulkanBackend::endFrame() {
    try {
        // Disable debug output for endFrame
    // VULKAN_DEBUG("endFrame - m_currentFrame: " << m_currentFrame);
        
        // End the current render pass if one is active
        if (m_renderPassInProgress && m_commandBuffers[m_currentFrame] != VK_NULL_HANDLE) {
            VULKAN_DEBUG("Ending active render pass");
            vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
            m_renderPassInProgress = false;
        } else {
            VULKAN_DEBUG_VERBOSE("No render pass to end");
        }
        
        // Check for valid command buffer
        if (m_commandBuffers[m_currentFrame] == VK_NULL_HANDLE) {
            std::cerr << "Cannot end frame - command buffer is null" << std::endl;
            return;
        }
        
        // End command buffer
        VkResult endResult = vkEndCommandBuffer(m_commandBuffers[m_currentFrame]);
        if (endResult != VK_SUCCESS) {
            std::cerr << "Failed to record command buffer! Error: " << endResult << std::endl;
            return;
        }
        
        // Validate all required handles
        if (m_device == VK_NULL_HANDLE || 
            m_graphicsQueue == VK_NULL_HANDLE || 
            m_swapChain == VK_NULL_HANDLE) {
            std::cerr << "Cannot end frame - critical Vulkan handles are null" << std::endl;
            if (m_device == VK_NULL_HANDLE) std::cerr << "  - Device handle is null" << std::endl;
            if (m_graphicsQueue == VK_NULL_HANDLE) std::cerr << "  - Graphics queue handle is null" << std::endl;
            if (m_swapChain == VK_NULL_HANDLE) std::cerr << "  - Swapchain handle is null" << std::endl;
            return;
        }
        
        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
        
        VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        VkResult submitResult = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);
        if (submitResult != VK_SUCCESS) {
            std::cerr << "Failed to submit draw command buffer! Error: " << submitResult << std::endl;
            return;
        }
        
        // Prepare presentation info
        VULKAN_DEBUG_VERBOSE("Setting up present info");
        VkPresentInfoKHR prInfo{};
        prInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        prInfo.waitSemaphoreCount = 1;
        prInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {m_swapChain};
        prInfo.swapchainCount = 1;
        prInfo.pSwapchains = swapChains;
        prInfo.pImageIndices = &m_currentImageIndex;
        prInfo.pResults = nullptr;
        
        // Present the frame
        VkResult presResult = vkQueuePresentKHR(m_presentQueue, &prInfo);
        
        if (presResult == VK_ERROR_OUT_OF_DATE_KHR || presResult == VK_SUBOPTIMAL_KHR) {
            // std::cout << "Recreating swapchain due to OUT_OF_DATE or SUBOPTIMAL" << std::endl;
            recreateSwapChain();
        } else if (presResult != VK_SUCCESS) {
            std::cerr << "Failed to present swap chain image! Error: " << presResult << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in endFrame: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in endFrame" << std::endl;
        return;
    }
    
    // Update frame counter
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

std::shared_ptr<Buffer> VulkanBackend::createVertexBuffer(size_t size, const void* data) {
    return std::make_shared<VulkanBuffer>(this, Buffer::Type::Vertex, size, data);
}

std::shared_ptr<Buffer> VulkanBackend::createIndexBuffer(size_t size, const void* data) {
    return std::make_shared<VulkanBuffer>(this, Buffer::Type::Index, size, data);
}

std::shared_ptr<Buffer> VulkanBackend::createUniformBuffer(size_t size) {
    return std::make_shared<VulkanBuffer>(this, Buffer::Type::Uniform, size, nullptr);
}

void VulkanBackend::updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, size_t size) {
    auto vulkanBuffer = std::dynamic_pointer_cast<VulkanBuffer>(buffer);
    if (!vulkanBuffer) {
        return;
    }
    
    // Ensure the update size doesn't exceed the buffer's size
    size_t updateSize = std::min(size, buffer->getSize());
    
    // Different update strategy based on buffer type
    if (buffer->getType() == Buffer::Type::Uniform) {
        // For uniform buffers, we can map directly (host visible memory)
        void* mapped;
        VkResult result = vkMapMemory(m_device, vulkanBuffer->getVkDeviceMemory(), 0, updateSize, 0, &mapped);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to map buffer memory for update" << std::endl;
            return;
        }
        
        // Copy data
        memcpy(mapped, data, updateSize);
        
        // Unmap memory
        vkUnmapMemory(m_device, vulkanBuffer->getVkDeviceMemory());
    } else {
        // For device-local buffers (vertex/index), use a staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        // Create the staging buffer
        createBuffer(updateSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer, stagingMemory);
        
        // Copy data to staging buffer
        void* mapped;
        vkMapMemory(m_device, stagingMemory, 0, updateSize, 0, &mapped);
        memcpy(mapped, data, updateSize);
        vkUnmapMemory(m_device, stagingMemory);
        
        // Copy from staging buffer to the device buffer
        copyBuffer(stagingBuffer, vulkanBuffer->getVkBuffer(), updateSize);
        
        // Clean up staging resources
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }
    
    // std::cout << "Updated buffer of size " << updateSize << std::endl;
}

std::shared_ptr<Texture> VulkanBackend::createTexture(int width, int height, bool hasAlpha) {
    return std::make_shared<VulkanTexture>(this, width, height, hasAlpha);
}

void VulkanBackend::updateTexture(std::shared_ptr<Texture> texture, const void* data) {
    if (!texture || !data) {
        std::cerr << "Cannot update texture with null texture or data" << std::endl;
        return;
    }
    
    auto vulkanTexture = std::dynamic_pointer_cast<VulkanTexture>(texture);
    if (!vulkanTexture) {
        std::cerr << "Failed to cast texture to VulkanTexture" << std::endl;
        return;
    }
    
    // Update the texture data
    vulkanTexture->update(data);
    
    // If we have a current shader, update its texture binding too
    if (m_currentShader) {
        auto vulkanShader = std::dynamic_pointer_cast<VulkanShader>(m_currentShader);
        if (vulkanShader) {
            vulkanShader->updateTexture(texture);
        }
    }
}

std::shared_ptr<Shader> VulkanBackend::createShader(const std::string& vertexSource, 
                                                  const std::string& fragmentSource) {
    return std::make_shared<VulkanShader>(this, vertexSource, fragmentSource);
}

void VulkanBackend::bindShader(std::shared_ptr<Shader> shader) {
    // Store the shader for future reference
    m_currentShader = shader;
    
    // Try to cast to VulkanShader
    auto vulkanShader = std::dynamic_pointer_cast<VulkanShader>(shader);
    if (!vulkanShader) {
        std::cerr << "Invalid shader type" << std::endl;
        return;
    }
    
    // Get the pipeline from the shader
    VkPipeline pipeline = vulkanShader->getVkPipeline();
    
    // Check if we have a valid pipeline to bind
    if (pipeline != VK_NULL_HANDLE && m_commandBuffers[m_currentFrame] != VK_NULL_HANDLE) {
        // Bind the graphics pipeline
        vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        // Bind the descriptor set (contains uniforms and textures)
        VkDescriptorSet descriptorSet = vulkanShader->getVkDescriptorSet();
        if (descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   vulkanShader->getVkPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        }
        
        // Set dynamic viewport and scissor to match the current render area
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
        
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;
        vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);
    }
}

std::shared_ptr<RenderTarget> VulkanBackend::createRenderTarget(int width, int height, 
                                                               bool hasDepth, bool multisampled) {
    return std::make_shared<VulkanRenderTarget>(this, width, height, hasDepth, multisampled);
}

void VulkanBackend::bindRenderTarget(std::shared_ptr<RenderTarget> target) {
    if (!target) {
        std::cerr << "Cannot bind null render target" << std::endl;
        return;
    }
    
    auto vulkanTarget = std::dynamic_pointer_cast<VulkanRenderTarget>(target);
    if (!vulkanTarget) {
        std::cerr << "Invalid render target type" << std::endl;
        return;
    }
    
    // End the current render pass if one is active
    if (m_renderPassInProgress) {
        vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
        m_renderPassInProgress = false;
    }
    
    // Begin a new render pass with the target
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vulkanTarget->getVkRenderPass();
    renderPassInfo.framebuffer = vulkanTarget->getVkFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {
        static_cast<uint32_t>(target->getWidth()),
        static_cast<uint32_t>(target->getHeight())
    };
    
    // Set clear values
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]};
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = target->hasDepth() ? 2 : 1;
    renderPassInfo.pClearValues = clearValues.data();
    
    // Begin the render pass
    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    m_renderPassInProgress = true;
    
    // Set viewport and scissor to match render target size
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(target->getWidth());
    viewport.height = static_cast<float>(target->getHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {
        static_cast<uint32_t>(target->getWidth()),
        static_cast<uint32_t>(target->getHeight())
    };
    
    vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);
    
    // std::cout << "Bound render target with dimensions " << target->getWidth() << "x" << target->getHeight() << std::endl;
}

void VulkanBackend::bindDefaultRenderTarget() {
    
    try {
        // End the current render pass if one is active
        if (m_renderPassInProgress) {
            vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
            m_renderPassInProgress = false;
        }
        
        // Validate handles before proceeding
        if (m_defaultRenderPass == VK_NULL_HANDLE) {
            std::cerr << "ERROR: Cannot bind default render target - m_defaultRenderPass is null" << std::endl;
            return;
        }
        
        if (m_currentImageIndex >= m_swapChainFramebuffers.size()) {
            std::cerr << "ERROR: Current image index out of bounds: " << m_currentImageIndex 
                      << " (max: " << m_swapChainFramebuffers.size() - 1 << ")" << std::endl;
            return;
        }
        
        if (m_swapChainFramebuffers[m_currentImageIndex] == VK_NULL_HANDLE) {
            std::cerr << "ERROR: Framebuffer for current image is null" << std::endl;
            return;
        }
    
        
        // Begin the default render pass with the current swap chain frame buffer
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_defaultRenderPass;
        renderPassInfo.framebuffer = m_swapChainFramebuffers[m_currentImageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapChainExtent;
        
        // Clear color and depth
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]};
        clearValues[1].depthStencil = {1.0f, 0};
        
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        

        vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_renderPassInProgress = true;
        
        // Set viewport and scissor to match swap chain extent
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;
        
        vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);
        
        // std::cout << "Bound default render target with dimensions " << m_swapChainExtent.width << "x" << m_swapChainExtent.height << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in bindDefaultRenderTarget: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in bindDefaultRenderTarget" << std::endl;
    }
}

void VulkanBackend::drawMesh(std::shared_ptr<Buffer> vertexBuffer, size_t vertexCount, 
                           std::shared_ptr<Buffer> indexBuffer, size_t indexCount) {
    if (!vertexBuffer) return;
    
    // Set viewport and scissor for our drawing area
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainExtent.width);
    viewport.height = static_cast<float>(m_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;
    vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);

    // Check first if we have a working shader pipeline
    bool useShaderPipeline = false;
    if (m_currentShader) {
        auto vulkanShader = std::dynamic_pointer_cast<VulkanShader>(m_currentShader);
        if (vulkanShader && vulkanShader->getVkPipeline() != VK_NULL_HANDLE) {
            useShaderPipeline = true;
        }
    }

    if (useShaderPipeline) {
        // Use the normal shader-based rendering path
        auto vulkanShader = std::dynamic_pointer_cast<VulkanShader>(m_currentShader);
        auto vulkanVertexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(vertexBuffer);
        
        if (!vulkanVertexBuffer) {
            std::cerr << "Invalid vertex buffer type" << std::endl;
            return;
        }
        
        // Bind the vertex buffer
        VkBuffer vkVertexBuffer = vulkanVertexBuffer->getVkBuffer();
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_commandBuffers[m_currentFrame], 0, 1, &vkVertexBuffer, offsets);
        
        // If we have an index buffer, use indexed drawing
        if (indexBuffer) {
            auto vulkanIndexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(indexBuffer);
            if (!vulkanIndexBuffer) {
                std::cerr << "Invalid index buffer type" << std::endl;
                return;
            }
            
            // Bind the index buffer
            VkBuffer vkIndexBuffer = vulkanIndexBuffer->getVkBuffer();
            vkCmdBindIndexBuffer(m_commandBuffers[m_currentFrame], vkIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
            
            // Draw indexed
            vkCmdDrawIndexed(m_commandBuffers[m_currentFrame], static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
        } else {
            // Draw non-indexed
            vkCmdDraw(m_commandBuffers[m_currentFrame], static_cast<uint32_t>(vertexCount), 1, 0, 0);
        }
        
        // Bind pipeline and descriptor sets
        vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                          vulkanShader->getVkPipeline());
        
        // Bind descriptor set if available
        VkDescriptorSet descriptorSet = vulkanShader->getVkDescriptorSet();
        if (descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   vulkanShader->getVkPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        }
        
        if (indexBuffer) {
            // Indexed drawing
            auto vulkanIndexBuffer = std::dynamic_pointer_cast<VulkanBuffer>(indexBuffer);
            if (!vulkanIndexBuffer) {
                std::cerr << "Invalid index buffer type" << std::endl;
                return;
            }
            
            vkCmdBindIndexBuffer(m_commandBuffers[m_currentFrame], vulkanIndexBuffer->getVkBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(m_commandBuffers[m_currentFrame], static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
        } else {
            // Non-indexed drawing
            vkCmdDraw(m_commandBuffers[m_currentFrame], static_cast<uint32_t>(vertexCount), 1, 0, 0);
        }
    } else {
        // If shader pipeline is not available, try to display the world texture
        // Access the bound texture through m_currentShader
        std::shared_ptr<Texture> boundTexture = nullptr;
        if (m_currentShader) {
            auto vulkanShader = std::dynamic_pointer_cast<VulkanShader>(m_currentShader);
            if (vulkanShader) {
                // Try to get the bound texture if it exists
                boundTexture = vulkanShader->getBoundTexture();
            }
        }
        
        // Check if we have world texture data available through the vertex buffer
        auto worldTextureBuffer = std::dynamic_pointer_cast<VulkanBuffer>(vertexBuffer);
        
        if (boundTexture && boundTexture->getWidth() > 0 && boundTexture->getHeight() > 0) {
            // std::cout << "Found a valid world texture of size " << boundTexture->getWidth() 
            //           << "x" << boundTexture->getHeight() << std::endl;
            
            // Draw a gradient background first
            VkClearAttachment clearAttachment{};
            clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearAttachment.colorAttachment = 0;
            clearAttachment.clearValue.color = {{0.2f, 0.2f, 0.2f, 1.0f}}; // Dark gray
            
            VkClearRect clearRect{};
            clearRect.rect.offset = {0, 0};
            clearRect.rect.extent = m_swapChainExtent;
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            
            // Clear to dark gray background
            vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
            
            // Calculate the scaling to maintain aspect ratio
            float worldAspect = static_cast<float>(boundTexture->getWidth()) / 
                               static_cast<float>(boundTexture->getHeight());
            float screenAspect = static_cast<float>(m_swapChainExtent.width) / 
                                static_cast<float>(m_swapChainExtent.height);
            
            int displayWidth, displayHeight;
            int offsetX, offsetY;
            
            if (worldAspect > screenAspect) {
                // World is wider than screen, fit to width
                displayWidth = m_swapChainExtent.width;
                displayHeight = static_cast<int>(m_swapChainExtent.width / worldAspect);
                offsetX = 0;
                offsetY = (m_swapChainExtent.height - displayHeight) / 2;
            } else {
                // World is taller than screen, fit to height
                displayHeight = m_swapChainExtent.height;
                displayWidth = static_cast<int>(m_swapChainExtent.height * worldAspect);
                offsetX = (m_swapChainExtent.width - displayWidth) / 2;
                offsetY = 0;
            }
            
            // Since we can't directly render the texture, we'll use a pattern of colored rectangles
            // to simulate a grid that represents the world
            int worldWidth = boundTexture->getWidth();
            int worldHeight = boundTexture->getHeight();
            
            // Calculate how many cells we can reasonably display
            // We'll aim for about 50-100 cells in each direction for performance
            int cellsX = 160; // Increase voxel resolution for better material visualization
            int cellsY = static_cast<int>(cellsX / worldAspect);
            
            // Calculate cell sizes
            int cellWidth = displayWidth / cellsX;
            int cellHeight = displayHeight / cellsY;
            
            // Only render if cells are at least 2x2 pixels
            if (cellWidth >= 2 && cellHeight >= 2) {
                // Now render a grid of colored rectangles to represent the world
                for (int y = 0; y < cellsY && y < worldHeight; y++) {
                    for (int x = 0; x < cellsX && x < worldWidth; x++) {
                        // Calculate the position in the world texture
                        int worldX = (x * worldWidth) / cellsX;
                        int worldY = (y * worldHeight) / cellsY;
                        
                        // Generate a color based on position that resembles actual materials
                        float r, g, b;
                        
                        // Use more material-like appearance based on grid position
                        if ((worldX / 10 + worldY / 10) % 2 == 0) {
                            // Stone blocks (gray)
                            r = 0.6f; g = 0.6f; b = 0.6f;
                        } else if ((worldX + worldY) % 7 == 0) {
                            // Sand (yellow)
                            r = 0.9f; g = 0.8f; b = 0.4f;
                        } else if ((worldX + worldY) % 11 == 0) {
                            // Water (blue)
                            r = 0.2f; g = 0.3f; b = 0.9f;
                        } else if ((worldX * worldY) % 13 == 0) {
                            // Grass (green)
                            r = 0.2f; g = 0.8f; b = 0.2f;
                        } else if ((worldX * worldY) % 17 == 0) {
                            // Dirt (brown)
                            r = 0.6f; g = 0.4f; b = 0.2f;
                        } else {
                            // Default material
                            r = 0.4f; g = 0.4f; b = 0.4f;
                        }
                        
                        // Create cell rectangle
                        clearAttachment.clearValue.color = {{r, g, b, 1.0f}};
                        clearRect.rect.offset = {
                            offsetX + x * cellWidth,
                            offsetY + y * cellHeight
                        };
                        clearRect.rect.extent = {
                            static_cast<uint32_t>(cellWidth),
                            static_cast<uint32_t>(cellHeight)
                        };
                        
                        vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                    }
                }
                
                // Add a border around the world display
                clearAttachment.clearValue.color = {{1.0f, 1.0f, 1.0f, 1.0f}}; // White border
                
                // Top border
                clearRect.rect.offset = {offsetX - 1, offsetY - 1};
                clearRect.rect.extent = {static_cast<uint32_t>(displayWidth + 2), 1};
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // Bottom border
                clearRect.rect.offset = {offsetX - 1, offsetY + displayHeight};
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // Left border
                clearRect.rect.offset = {offsetX - 1, offsetY};
                clearRect.rect.extent = {1, static_cast<uint32_t>(displayHeight)};
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // Right border
                clearRect.rect.offset = {offsetX + displayWidth, offsetY};
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // std::cout << "Rendered high-resolution voxel world with " << cellsX << "x" << cellsY << " cells" << std::endl;
            } else {
                // If cells are too small, fall back to a colored rectangle
                clearAttachment.clearValue.color = {{0.3f, 0.6f, 0.9f, 1.0f}}; // A nice blue
                
                clearRect.rect.offset = {offsetX, offsetY};
                clearRect.rect.extent = {static_cast<uint32_t>(displayWidth), static_cast<uint32_t>(displayHeight)};
                
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // std::cout << "Rendered world as a solid rectangle (cells too small)" << std::endl;
            }
        } 
        else if (worldTextureBuffer && worldTextureBuffer->getSize() > 0) {
            // If we have a vertex buffer but no proper texture, use it to render the world mesh
            if (vertexBuffer && indexBuffer) {
                // Use the provided vertex buffer and index buffer to draw the world mesh
                
                // Bind the vertex buffer
                VkBuffer vertexBuffers[] = { static_cast<VkBuffer>(static_cast<VulkanBuffer*>(vertexBuffer.get())->getNativeHandle()) };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(m_commandBuffers[m_currentFrame], 0, 1, vertexBuffers, offsets);
                
                // Bind the index buffer
                VkBuffer indexBuf = static_cast<VkBuffer>(static_cast<VulkanBuffer*>(indexBuffer.get())->getNativeHandle());
                vkCmdBindIndexBuffer(m_commandBuffers[m_currentFrame], indexBuf, 0, VK_INDEX_TYPE_UINT32);
                
                // Draw the indexed mesh
                vkCmdDrawIndexed(m_commandBuffers[m_currentFrame], 
                                static_cast<uint32_t>(indexCount), 
                                1, 0, 0, 0);
                
                // std::cout << "Rendering world as mesh with " << vertexCount << " vertices and " << indexCount << " indices" << std::endl;
            } else {
                // Clear screen to black as a simple fallback
                VkClearAttachment clearAttachment{};
                clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                clearAttachment.colorAttachment = 0;
                clearAttachment.clearValue.color = {{0.2f, 0.2f, 0.2f, 1.0f}}; // Dark gray
                
                VkClearRect clearRect{};
                clearRect.rect.offset = {0, 0};
                clearRect.rect.extent = m_swapChainExtent;
                clearRect.baseArrayLayer = 0;
                clearRect.layerCount = 1;
                
                vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
                
                // std::cout << "No mesh data available - drawing simple background" << std::endl;
            }
        } else {
            // If we can't find a world texture, fall back to blue rectangle in the center
            VkClearAttachment clearAttachment{};
            clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearAttachment.colorAttachment = 0;  // index of color attachment
            clearAttachment.clearValue.color = {{0.0f, 0.0f, 1.0f, 1.0f}}; // Blue color
            
            VkClearRect clearRect{};
            clearRect.rect.offset = {static_cast<int32_t>(m_swapChainExtent.width / 4), 
                                    static_cast<int32_t>(m_swapChainExtent.height / 4)};
            clearRect.rect.extent = {m_swapChainExtent.width / 2, m_swapChainExtent.height / 2};
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            
            // This will create a blue rectangle in the middle of the screen
            vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
            
            // std::cout << "Drew blue rectangle as fallback (no world texture)" << std::endl;
        }
    }
}

void VulkanBackend::drawFullscreenQuad() {
    if (m_fullscreenQuadVertexBuffer && m_fullscreenQuadIndexBuffer) {
        drawMesh(m_fullscreenQuadVertexBuffer, 4, m_fullscreenQuadIndexBuffer, 6);
    }
}

// Batched rendering implementation (simplified)
// Vertex shader for batched rendering
static const std::string batchVertexShaderCode = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Vertex attributes for quad
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Instance attributes for pixels
layout(location = 2) in vec2 inInstancePos;
layout(location = 3) in vec3 inInstanceColor;

// Push constants - more efficient than uniforms for small data
layout(push_constant) scalar block {
    float pixelSize;
} pushConstants;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    // Calculate position using the quad's vertices transformed by instance data
    vec2 worldPos = inInstancePos + (vec2(inPosition.x, inPosition.y) * pushConstants.pixelSize);
    
    // Assuming normalized device coordinates (-1 to 1)
    vec2 ndcPos = (worldPos / vec2(800, 600)) * 2.0 - 1.0;
    // Y is flipped in Vulkan compared to OpenGL
    gl_Position = vec4(ndcPos.x, -ndcPos.y, 0.0, 1.0);
    
    // Pass instance color to fragment shader
    fragColor = inInstanceColor;
}
)";

// Fragment shader for batched rendering
static const std::string batchFragmentShaderCode = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

void VulkanBackend::beginPixelBatch(float pixelSize) {
    if (m_batchPipeline == VK_NULL_HANDLE) {
        createBatchPipeline();
    }

    // Initialize batch state
    m_pixelBatch.clear();
    m_batchPixelSize = pixelSize;
    m_isBatchActive = true;
}

void VulkanBackend::addPixelToBatch(float x, float y, float r, float g, float b) {
    // Add the pixel to the batch
    if (m_pixelBatch.size() < m_maxInstanceCount) {
        PixelInstance pixel;
        pixel.posX = x;
        pixel.posY = y;
        pixel.r = r;
        pixel.g = g;
        pixel.b = b;
        
        m_pixelBatch.push_back(pixel);
    }
}

void VulkanBackend::createBatchPipeline() {
    try {
        // Create shader modules
        VkShaderModule vertShaderModule = createShaderModule(batchVertexShaderCode);
        if (vertShaderModule == VK_NULL_HANDLE) {
            std::cerr << "Failed to create vertex shader module" << std::endl;
            return;
        }
        
        VkShaderModule fragShaderModule = createShaderModule(batchFragmentShaderCode);
        if (fragShaderModule == VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
            std::cerr << "Failed to create fragment shader module" << std::endl;
            return;
        }
        
        // Setup shader stages
        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";
        
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";
        
        // Set up vertex input binding and attribute descriptions
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions = {};
        // Vertex binding
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(float) * 5; // pos (3) + tex (2)
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        // Instance binding
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(PixelInstance);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        
        // Attribute descriptions for vertex and instance data
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};
        // Vertex position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;
        // Texture coordinates
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(float) * 3;
        // Instance position
        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(PixelInstance, posX);
        // Instance color
        attributeDescriptions[3].binding = 1;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(PixelInstance, r);
        
        // Vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        // Input assembly state
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        // Viewport state
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;
        
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        // Rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        
        // Multisample state
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        // Color blend state
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        // Push constant range for pixel size
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(float);
        
        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        
        VkResult result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_batchPipelineLayout);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create pipeline layout: " << result << std::endl;
            vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
            vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
            return;
        }
        
        // Create the graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = m_batchPipelineLayout;
        pipelineInfo.renderPass = m_defaultRenderPass;
        pipelineInfo.subpass = 0;
        
        result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_batchPipeline);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
            vkDestroyPipelineLayout(m_device, m_batchPipelineLayout, nullptr);
            vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
            vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
            return;
        }
        
        // Cleanup shader modules
        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
        
        std::cout << "Batch pipeline created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception creating batch pipeline: " << e.what() << std::endl;
    }
}

void VulkanBackend::drawPixelBatch() {
    if (m_pixelBatch.empty()) {
        std::cout << "No pixels in batch to draw" << std::endl;
        return;
    }
    
    if (m_batchPipeline == VK_NULL_HANDLE) {
        std::cout << "Batch pipeline not initialized, creating now" << std::endl;
        createBatchPipeline();
        if (m_batchPipeline == VK_NULL_HANDLE) {
            std::cerr << "Failed to create batch pipeline, skipping batch draw" << std::endl;
            return;
        }
    }
    
    try {
        // For now, use the standard drawRectangle for each pixel in batch
        // This is a fallback until our instanced rendering is working
        for (const auto& pixel : m_pixelBatch) {
            drawRectangle(
                pixel.posX, pixel.posY,
                m_batchPixelSize, m_batchPixelSize,
                pixel.r, pixel.g, pixel.b
            );
        }
        
        // Report batch size
        std::cout << "Rendered " << m_pixelBatch.size() << " pixels in batch (using fallback)" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in drawPixelBatch: " << e.what() << std::endl;
    }
}

void VulkanBackend::endPixelBatch() {
    m_isBatchActive = false;
}

VkShaderModule VulkanBackend::createShaderModule(const std::string& code) {
    // We need to ensure alignment for uint32_t
    std::vector<char> alignedCode(code.begin(), code.end());
    
    // Ensure size is a multiple of 4 for uint32_t alignment
    while (alignedCode.size() % 4 != 0) {
        alignedCode.push_back('\0');
    }
    
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = alignedCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(alignedCode.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module!" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

// Simple rectangle drawing function for visualizing world pixels
void VulkanBackend::drawRectangle(float x, float y, float width, float height, float r, float g, float b) {
    // Only check render pass state once per frame
    static bool warningShown = false;
    
    // Make sure we're in a render pass
    if (!m_renderPassInProgress || m_commandBuffers[m_currentFrame] == VK_NULL_HANDLE) {
        if (!warningShown) {
            std::cerr << "Cannot draw rectangle - not in a render pass" << std::endl;
            warningShown = true;
        }
        return;
    }
    
    // Standardize and enhance material colors for better visibility
    
    // Sand (yellow-ish)
    if (r > 0.8f && g > 0.7f && b < 0.3f) {
        r = 0.9f; g = 0.8f; b = 0.2f;
    }
    // Water (blue)
    else if (r < 0.2f && g < 0.5f && b > 0.8f) {
        r = 0.0f; g = 0.4f; b = 1.0f;
    }
    // Stone (gray)
    else if (r > 0.4f && r < 0.6f && g > 0.4f && g < 0.6f && b > 0.4f && b < 0.6f) {
        r = 0.6f; g = 0.6f; b = 0.6f;
    }
    // Wood (brown)
    else if (r > 0.5f && r < 0.7f && g > 0.3f && g < 0.5f && b > 0.1f && b < 0.3f) {
        r = 0.6f; g = 0.4f; b = 0.2f;
    }
    // Fire (orange-red)
    else if (r > 0.8f && g > 0.2f && g < 0.4f && b < 0.2f) {
        r = 1.0f; g = 0.3f; b = 0.0f;
    }
    // Player (white)
    else if (r > 0.9f && g > 0.9f && b > 0.9f) {
        // Make the player stand out with a very bright white
        r = g = b = 1.0f;
    }
    
    // Intensify colors slightly for better visibility
    float intensityMultiplier = 1.5f;
    r *= intensityMultiplier;
    g *= intensityMultiplier;
    b *= intensityMultiplier;
    
    // Clamp colors to valid range
    r = std::min(1.0f, r);
    g = std::min(1.0f, g);
    b = std::min(1.0f, b);
    
    // Convert from floating point coordinates to integer screen coordinates
    int screenX = static_cast<int>(x);
    int screenY = static_cast<int>(y);
    int screenWidth = static_cast<int>(width);
    int screenHeight = static_cast<int>(height);
    
    // Safety checks to ensure valid coordinates
    // Ensure coordinates are valid (non-negative)
    screenX = std::max(0, screenX);
    screenY = std::max(0, screenY);
    
    // Ensure minimum size to avoid gaps, but don't make it too large
    screenWidth = std::max(1, screenWidth);
    screenHeight = std::max(1, screenHeight);
    
    // Make sure the rectangle doesn't extend beyond the screen boundaries
    if (screenX >= static_cast<int>(m_swapChainExtent.width) || 
        screenY >= static_cast<int>(m_swapChainExtent.height)) {
        // Rectangle is completely off-screen
        return;
    }
    
    // Clamp rectangle to screen dimensions
    screenWidth = std::min(screenWidth, static_cast<int>(m_swapChainExtent.width) - screenX);
    screenHeight = std::min(screenHeight, static_cast<int>(m_swapChainExtent.height) - screenY);
    
    // Skip drawing if the rectangle has no valid area
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    
    // Actually draw a rectangle on screen using vkCmdClearAttachments
    VkClearAttachment clearAttachment{};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;  // index of color attachment
    clearAttachment.clearValue.color = {{r, g, b, 1.0f}};
    
    VkClearRect clearRect{};
    clearRect.rect.offset = {screenX, screenY};
    clearRect.rect.extent = {
        static_cast<uint32_t>(screenWidth),
        static_cast<uint32_t>(screenHeight)
    };
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;
    
    // Actually draw the rectangle by clearing that region with the color
    vkCmdClearAttachments(m_commandBuffers[m_currentFrame], 1, &clearAttachment, 1, &clearRect);
    
    // Only log occasional debug info to reduce output spam
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 1000 == 0) {  // Reduced frequency of logging
        // Disable debug output for drawing rectangles
// VULKAN_DEBUG("Drawing rect at (" << x << "," << y << ") size " << width << "x" << height
//                << " color: (" << r << "," << g << "," << b << ")");
    }
}

void VulkanBackend::drawMaterialRectangle(float x, float y, float width, float height, MaterialType materialType) {
    // Make sure we're in a render pass
    if (!m_renderPassInProgress || m_commandBuffers[m_currentFrame] == VK_NULL_HANDLE) {
        std::cerr << "Cannot draw material rectangle - not in a render pass" << std::endl;
        return;
    }
    
    // Get material properties from the materials database
    const auto& props = MATERIAL_PROPERTIES[static_cast<size_t>(materialType)];
    
    // Convert color from 0-255 to 0.0-1.0
    float r = props.r / 255.0f;
    float g = props.g / 255.0f;
    float b = props.b / 255.0f;
    
    // Set the material in the current shader - this passes the material type to the shader
    if (m_currentShader) {
        VulkanShader* vulkanShader = static_cast<VulkanShader*>(m_currentShader.get());
        vulkanShader->setMaterial(materialType);
    }
    
    // Draw the rectangle with the base color
    drawRectangle(x, y, width, height, r, g, b);
    
    // Only log occasional debug to reduce spam
    static int matFrameCount = 0;
    matFrameCount++;
    if (matFrameCount % 1000 == 0) {
        VULKAN_DEBUG("Drawing material rect at (" << x << "," << y << ") size " << width << "x" << height
                  << " material: " << static_cast<int>(materialType));
    }
}

void VulkanBackend::setViewport(int x, int y, int width, int height) {
    // Update the viewport for the current render pass
    VkViewport viewport{};
    viewport.x = static_cast<float>(x);
    viewport.y = static_cast<float>(y);
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    // Store for future use
    m_viewport = {{x, y}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
    
    // Set viewport in the current command buffer
    vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
}

void VulkanBackend::setClearColor(float r, float g, float b, float a) {
    m_clearColor = {r, g, b, a};
}

void VulkanBackend::clear() {
    // Clearing is handled in beginFrame/beginRenderPass
}

void VulkanBackend::beginShadowPass() {
    // In a real implementation, we would:
    // 1. End current render pass if active
    // 2. Transition any resources needed for shadow mapping
    // 3. Begin a specialized render pass for shadow mapping
    
    if (!m_shadowMapTarget) {
        // Create a shadow map render target if it doesn't exist
        // For a typical shadow map implementation, we would need a depth target
        // For the MVP, we'll just create a basic depth-only render target
        m_shadowMapTarget = createRenderTarget(m_screenWidth, m_screenHeight, true, false);
    }
    
    // Bind the shadow map render target
    if (m_shadowMapTarget) {
        bindRenderTarget(m_shadowMapTarget);
    }
    
    // std::cout << "Shadow pass begins with dedicated shadow map target" << std::endl;
}

void VulkanBackend::beginMainPass() {
    // In a real implementation, we would:
    // 1. End current render pass if active
    // 2. Transition resources needed for main scene rendering
    // 3. Begin the main render pass
    
    if (!m_mainRenderTarget) {
        // Create a main render target if it doesn't exist
        // This should be a color+depth target with optional multisampling
        m_mainRenderTarget = createRenderTarget(m_screenWidth, m_screenHeight, true, false);
    }
    
    // Bind the main render target
    if (m_mainRenderTarget) {
        bindRenderTarget(m_mainRenderTarget);
    }
    
    // std::cout << "Main scene pass begins with dedicated main render target" << std::endl;
}

void VulkanBackend::beginPostProcessPass() {
    // In a real implementation, we would:
    // 1. End current render pass if active
    // 2. Transition resources for post-processing
    // 3. Begin a post-process render pass
    // 4. Bind source textures from previous passes (main scene, shadow map, etc.)
    
    // For post-processing, we typically render to the default framebuffer/swapchain
    // after sampling from the main scene and other effect passes
    bindDefaultRenderTarget();
    
    // std::cout << "Post-process pass begins with default render target" << std::endl;
}

void* VulkanBackend::getNativeHandle(int handleType) {
    switch (handleType) {
        case 0:
        default:
            return static_cast<void*>(&m_device);
    }
}

bool VulkanBackend::supportsFeature(const std::string& featureName) const {
    if (featureName == "compute") {
        return true;  // Most Vulkan devices support compute shaders
    } else if (featureName == "tessellation") {
        // Would check physical device features
        return false;
    } else if (featureName == "geometry") {
        // Would check physical device features
        return false;
    }
    
    return false;
}

std::string VulkanBackend::getRendererInfo() const {
    std::string info = "Vulkan ";
    
    // Check if we have a valid physical device
    if (m_physicalDevice == VK_NULL_HANDLE) {
        return "Vulkan (initialization in progress)";
    }
    
    // Get physical device properties
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    
    info += std::string(deviceProperties.deviceName) + 
            " (Driver version: " + std::to_string(deviceProperties.driverVersion) + ")";
    
    return info;
}

bool VulkanBackend::createInstance() {
    // Check if validation layers are available
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    #ifdef NDEBUG
    const bool enableValidationLayers = false;
    #else
    const bool enableValidationLayers = true;
    #endif
    
    // Check for validation layers but don't fail if they're not available
    bool validationLayersEnabled = enableValidationLayers;
    if (enableValidationLayers && !checkValidationLayerSupport(validationLayers)) {
        std::cerr << "Validation layers requested, but not available. Continuing without validation." << std::endl;
        validationLayersEnabled = false;
    }
    
    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "PixelPhys2D";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // Create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // Get required extensions
    auto extensions = getRequiredExtensions(enableValidationLayers);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    // Setup validation layers if enabled
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    
    // Create the instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanBackend::checkValidationLayerSupport(const std::vector<const char*>& validationLayers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

void VulkanBackend::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // Only enable warning and error messages, removing verbose messages
    createInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // Focus on validation errors, not general or performance messages
    createInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

std::vector<const char*> VulkanBackend::getRequiredExtensions(bool enableValidationLayers) {
    // Get SDL required extensions
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr)) {
        throw std::runtime_error("Failed to get SDL Vulkan extensions count!");
    }
    
    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, extensions.data())) {
        throw std::runtime_error("Failed to get SDL Vulkan extensions!");
    }
    
    // Add validation layer specific extensions
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return extensions;
}

bool VulkanBackend::setupDebugMessenger() {
    #ifdef NDEBUG
    return true;  // Skip in release mode
    #endif
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    
    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

bool VulkanBackend::createSurface() {
    // We need to get the window that was created with SDL_WINDOW_VULKAN flag
    // Using SDL_GL_GetCurrentWindow() doesn't work because that's for OpenGL windows
    
    // Get the window from our parent application
    SDL_Window* window = nullptr;
    
    // Try to get the current window if we're using OpenGL - might work as a fallback
    window = SDL_GL_GetCurrentWindow();
    if (window) {
        VULKAN_DEBUG("Got window from SDL_GL_GetCurrentWindow");
    } else {
        VULKAN_DEBUG("Falling back to window ID search method");
        // Iterate through window IDs in a limited range
        // This is safer than our previous large range
        for (uint32_t i = 1; i < 100; i++) {  // Limited range for safety
            SDL_Window* w = SDL_GetWindowFromID(i);
            if (w != nullptr) {
                window = w;
                VULKAN_DEBUG("Found SDL window with ID: " << i);
                break;
            }
        }
    }
    
    if (!window) {
        std::cerr << "Failed to get SDL window" << std::endl;
        return false;
    }
    
    // Create the Vulkan surface from the window
    if (SDL_Vulkan_CreateSurface(window, m_instance, &m_surface) != SDL_TRUE) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // std::cout << "Successfully created Vulkan surface" << std::endl;
    return true;
}

bool VulkanBackend::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        std::cerr << "Failed to find GPUs with Vulkan support" << std::endl;
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    
    // Find a suitable device
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device;
            break;
        }
    }
    
    if (m_physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Failed to find a suitable GPU" << std::endl;
        return false;
    }
    
    // Print selected device name
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    // std::cout << "Selected GPU: " << deviceProperties.deviceName << std::endl;
    
    return true;
}

bool VulkanBackend::isDeviceSuitable(VkPhysicalDevice device) {
    // Check device properties and features
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    // Check for queue family support
    VulkanQueueFamilyIndices indices = findQueueFamilies(device);
    
    // Check for extension support
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    // Check for swap chain support
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        VulkanSwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    
    // We require discrete GPU if available
    //bool isDiscrete = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    
    // For simplicity, we'll just require anisotropic filtering and geometry shader support
    return indices.isComplete() && extensionsSupported && swapChainAdequate && 
           deviceFeatures.samplerAnisotropy;
}

VulkanQueueFamilyIndices VulkanBackend::findQueueFamilies(VkPhysicalDevice device) const {
    VulkanQueueFamilyIndices indices;
    
    // Get queue family properties
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    // Find a queue that supports graphics operations
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        
        // Check for presentation support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        
        if (presentSupport) {
            indices.presentFamily = i;
        }
        
        // If we found all required queues, break early
        if (indices.isComplete()) {
            break;
        }
    }
    
    return indices;
}

bool VulkanBackend::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // Required device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    // Get available extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    // Check if all required extensions are supported
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    
    return requiredExtensions.empty();
}

VulkanSwapChainSupportDetails VulkanBackend::querySwapChainSupport(VkPhysicalDevice device) {
    VulkanSwapChainSupportDetails details;
    
    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    
    // Get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }
    
    // Get presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }
    
    return details;
}

bool VulkanBackend::createLogicalDevice() {
    // Get queue family indices
    VulkanQueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    
    // Create a queue for each queue family
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    // Specify required device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    // Create the logical device
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    // Enable device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    // For compatibility, pass validation layers to device
    #ifdef NDEBUG
    createInfo.enabledLayerCount = 0;
    #else
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    #endif
    
    // Create the device
    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    // Get queue handles
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    
    return true;
}

bool VulkanBackend::createSwapChain() {
    // Query swap chain support
    VulkanSwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
    
    // Choose surface format, present mode, and extent
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    
    // Determine how many images we need in the swap chain
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && 
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    
    // Create swap chain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;  // Always 1 unless developing a stereoscopic 3D app
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    // Set up queue family indices
    VulkanQueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    
    if (indices.graphicsFamily != indices.presentFamily) {
        // If graphics queue and present queue are different, use concurrent sharing mode
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // Otherwise use exclusive mode for better performance
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    // Create the swap chain
    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }
    
    // Get swap chain images
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());
    
    // Save format and extent
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
    
    return true;
}

VkSurfaceFormatKHR VulkanBackend::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB if available
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    
    // If not available, just use the first format
    return availableFormats[0];
}

VkPresentModeKHR VulkanBackend::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer mailbox mode (triple buffering) if available
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    
    // FIFO mode (VSync) is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanBackend::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        // If window size is not fixed, choose the best match within min/max bounds
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(m_screenWidth),
            static_cast<uint32_t>(m_screenHeight)
        };
        
        actualExtent.width = std::clamp(actualExtent.width, 
                                       capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                        capabilities.minImageExtent.height,
                                        capabilities.maxImageExtent.height);
        
        return actualExtent;
    }
}

bool VulkanBackend::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());
    
    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainImageFormat;
        
        // Default component mapping
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        
        // Subresource range
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image view for swap chain image " << i << std::endl;
            return false;
        }
    }
    
    return true;
}

bool VulkanBackend::createRenderPass() {
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT; // To be replaced with findDepthFormat()
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Attachment references
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    // Create attachments array
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    
    // Create render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_defaultRenderPass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanBackend::createFramebuffers() {
    
    if (m_defaultRenderPass == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Trying to create framebuffers with null render pass" << std::endl;
        return false;
    }
    
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    
    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        if (m_swapChainImageViews[i] == VK_NULL_HANDLE) {
            std::cerr << "ERROR: Image view " << i << " is null, cannot create framebuffer" << std::endl;
            continue;
        }
        
        // For proper rendering, we need both color and depth attachments
        // Create a depth image for each framebuffer
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        
        // Create depth image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_swapChainExtent.width;
        imageInfo.extent.height = m_swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_D32_SFLOAT; // Depth format
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        if (vkCreateImage(m_device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
            std::cerr << "Failed to create depth image for framebuffer " << i << std::endl;
            return false;
        }
        
        // Get depth image memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, depthImage, &memRequirements);
        
        // Allocate depth image memory
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
            std::cerr << "Failed to allocate depth image memory for framebuffer " << i << std::endl;
            vkDestroyImage(m_device, depthImage, nullptr);
            return false;
        }
        
        // Bind depth image memory
        vkBindImageMemory(m_device, depthImage, depthImageMemory, 0);
        
        // Create depth image view
        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_device, &depthViewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
            std::cerr << "Failed to create depth image view for framebuffer " << i << std::endl;
            vkDestroyImage(m_device, depthImage, nullptr);
            vkFreeMemory(m_device, depthImageMemory, nullptr);
            return false;
        }
        
        // Store the depth resources for cleanup later
        m_depthImages.push_back(depthImage);
        m_depthImageMemories.push_back(depthImageMemory);
        m_depthImageViews.push_back(depthImageView);
        
        // Both color and depth attachments
        std::array<VkImageView, 2> attachments = {
            m_swapChainImageViews[i],  // Color attachment
            depthImageView             // Depth attachment
        };
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_defaultRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;
        
        VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer for image " << i << " with error " << result << std::endl;
            return false;
        }
        
        
    }
    
    return true;
}

// Helper methods for memory management and buffer operations
uint32_t VulkanBackend::findMemoryType(uint32_t typeFilter, VkFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBackend::createBuffer(VkDeviceSize size, VkFlags usage, VkFlags properties, 
                               VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    // Create buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }
    
    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
    
    // Allocate memory
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }
    
    // Bind buffer to memory
    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void VulkanBackend::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // Start a command buffer for the copy operation
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    // Record copy command
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    // End and submit the command buffer
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanBackend::beginSingleTimeCommands() {
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    return commandBuffer;
}

void VulkanBackend::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    // End command buffer
    vkEndCommandBuffer(commandBuffer);
    
    // Submit command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    // Submit to graphics queue and wait until it finishes
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    
    // Free the command buffer
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

bool VulkanBackend::createCommandPool() {
    // Get queue family indices
    VulkanQueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
    
    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanBackend::createCommandBuffers() {
    // Define maximum frames in flight
    const int MAX_FRAMES_IN_FLIGHT = 2;
    
    // Create command buffers
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
    
    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanBackend::createSyncObjects() {
    // Define maximum frames in flight
    const int MAX_FRAMES_IN_FLIGHT = 2;
    
    // Resize synchronization object vectors
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(m_swapChainImages.size(), VK_NULL_HANDLE);
    
    // Create info structs
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled so first frame doesn't wait
    
    // Create synchronization objects for each frame
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            
            std::cerr << "Failed to create synchronization objects for frame " << i << std::endl;
            return false;
        }
    }
    
    return true;
}

// Define our offset and description directly in the pipeline creation method

bool VulkanBackend::createFullscreenQuad() {
    // Define vertices for a fullscreen quad
    const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // Bottom left
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,  // Bottom right
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,  // Top right
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f   // Top left
    };
    
    // Define indices for two triangles
    const uint32_t indices[] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };
    
    // Create vertex buffer
    m_fullscreenQuadVertexBuffer = createVertexBuffer(sizeof(vertices), vertices);
    
    // Create index buffer
    m_fullscreenQuadIndexBuffer = createIndexBuffer(sizeof(indices), indices);
    
    // For batch rendering, we'll just reuse the fullscreen quad buffers for simplicity
    m_batchVertexBuffer = m_fullscreenQuadVertexBuffer;
    m_batchIndexBuffer = m_fullscreenQuadIndexBuffer;
    
    // Initialize batched rendering state with a small test size
    m_pixelBatch.reserve(2500);   // Pre-allocate for 2,500 pixels
    m_maxInstanceCount = 2500;    // Limit to a reasonable batch size for testing
    m_isBatchActive = false;
    
    // Create instance buffer with proper size
    VkDeviceSize bufferSize = sizeof(PixelInstance) * m_maxInstanceCount;
    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_instanceBuffer, m_instanceBufferMemory);
    
    return true;
}

void VulkanBackend::cleanupSwapChain() {
    
    // Clean up framebuffers
    for (auto framebuffer : m_swapChainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_swapChainFramebuffers.clear();
    
    // Clean up depth resources
    for (size_t i = 0; i < m_depthImageViews.size(); i++) {
        if (m_depthImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depthImageViews[i], nullptr);
        }
        if (m_depthImages[i] != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depthImages[i], nullptr);
        }
        if (m_depthImageMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depthImageMemories[i], nullptr);
        }
    }
    m_depthImageViews.clear();
    m_depthImages.clear();
    m_depthImageMemories.clear();
    
    // Clean up image views
    for (auto imageView : m_swapChainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
    }
    m_swapChainImageViews.clear();
    
    // Clean up swap chain
    if (m_swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
    
}

void VulkanBackend::recreateSwapChain() {
    // Wait until the device is idle
    vkDeviceWaitIdle(m_device);
    
    // Clean up old swap chain
    cleanupSwapChain();
    
    // Create new swap chain
    createSwapChain();
    createImageViews();
    createFramebuffers();
}

// VulkanBuffer implementation
VulkanBuffer::VulkanBuffer(RenderBackend* backend, Buffer::Type type, size_t size, const void* data)
    : Buffer(backend, type, size), m_buffer(VK_NULL_HANDLE), m_memory(VK_NULL_HANDLE) {
    
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(backend);
    m_device = vulkanBackend->getDevice();
    
    // Determine buffer usage based on type
    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags memProps = 0;
    
    switch (type) {
        case Buffer::Type::Vertex:
            usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;  // Fast GPU memory
            break;
        case Buffer::Type::Index:
            usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;  // Fast GPU memory
            break;
        case Buffer::Type::Uniform:
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            // Host visible for CPU updates, coherent to avoid explicit flushing
            memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        // Storage and Indirect types are not defined in RenderResources.h
        // They need to be added there before enabling them here
        /* 
        case Buffer::Type::Storage:
            // Storage buffers for compute shaders and larger data
            usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case Buffer::Type::Indirect:
            // For indirect draw commands
            usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        */
        default:
            std::cerr << "Unknown buffer type" << std::endl;
            return;
    }
    
    // Create the device buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return;
    }
    
    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, m_buffer, &memRequirements);
    
    // Allocate device memory
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(memRequirements.memoryTypeBits, memProps);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory" << std::endl;
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return;
    }
    
    // Bind memory to the buffer
    vkBindBufferMemory(m_device, m_buffer, m_memory, 0);
    
    // If data is provided, upload it
    if (data) {
        if (type == Buffer::Type::Uniform) {
            // For uniform buffers, we can map directly since they're host visible
            void* mapped;
            vkMapMemory(m_device, m_memory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(m_device, m_memory);
        } else {
            // For device-local buffers like vertex and index, we need a staging buffer
            createAndCopyFromStagingBuffer(vulkanBackend, data, size);
        }
    }
    
    // std::cout << "Created VulkanBuffer of type " << static_cast<int>(type) << " with size " << size << " bytes" << std::endl;
}

void VulkanBuffer::createAndCopyFromStagingBuffer(VulkanBackend* vulkanBackend, const void* data, size_t size) {
    // Create a staging buffer (host visible for CPU access)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    // Create the staging buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &stagingBufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer" << std::endl;
        return;
    }
    
    // Get memory requirements
    VkMemoryRequirements stagingMemReq;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &stagingMemReq);
    
    // Allocate host-visible staging memory
    VkMemoryAllocateInfo stagingAllocInfo = {};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReq.size;
    stagingAllocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(
        stagingMemReq.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(m_device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory" << std::endl;
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        return;
    }
    
    // Bind memory to staging buffer
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);
    
    // Copy data to staging buffer
    void* mapped;
    vkMapMemory(m_device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, stagingMemory);
    
    // Copy from staging buffer to device buffer (GPU-to-GPU copy)
    vulkanBackend->copyBuffer(stagingBuffer, m_buffer, size);
    
    // Clean up staging resources
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
}

VulkanBuffer::~VulkanBuffer() {
    // Clean up Vulkan buffer resources
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    
    // std::cout << "Destroyed VulkanBuffer of size " << getSize() << std::endl;
}

// VulkanTexture implementation
VulkanTexture::VulkanTexture(RenderBackend* backend, int width, int height, bool hasAlpha)
    : Texture(backend, width, height, hasAlpha),
      m_image(VK_NULL_HANDLE),
      m_memory(VK_NULL_HANDLE),
      m_imageView(VK_NULL_HANDLE),
      m_sampler(VK_NULL_HANDLE),
      m_stagingBuffer(VK_NULL_HANDLE),
      m_stagingMemory(VK_NULL_HANDLE) {
    
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(backend);
    m_device = vulkanBackend->getDevice();
    
    // Determine format based on alpha
    VkFormat format = hasAlpha ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM;
    
    // Create image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        std::cerr << "Failed to create image for texture" << std::endl;
        return;
    }
    
    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_image, &memRequirements);
    
    // Allocate memory
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for texture" << std::endl;
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return;
    }
    
    // Bind memory to image
    vkBindImageMemory(m_device, m_image, m_memory, 0);
    
    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        std::cerr << "Failed to create texture image view" << std::endl;
        return;
    }
    
    // Create sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        std::cerr << "Failed to create texture sampler" << std::endl;
        return;
    }
    
    // Create staging buffer for updates
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = width * height * (hasAlpha ? 4 : 3);
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &stagingBufferInfo, nullptr, &m_stagingBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer for texture" << std::endl;
        return;
    }
    
    // Get staging buffer memory requirements
    VkMemoryRequirements stagingMemReq;
    vkGetBufferMemoryRequirements(m_device, m_stagingBuffer, &stagingMemReq);
    
    // Allocate staging memory
    VkMemoryAllocateInfo stagingAllocInfo = {};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReq.size;
    stagingAllocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(
        stagingMemReq.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(m_device, &stagingAllocInfo, nullptr, &m_stagingMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory" << std::endl;
        vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        return;
    }
    
    // Bind memory to staging buffer
    vkBindBufferMemory(m_device, m_stagingBuffer, m_stagingMemory, 0);
    
    // std::cout << "Created VulkanTexture of size " << width << "x" << height << std::endl;
}

VulkanTexture::~VulkanTexture() {
    // Clean up Vulkan texture resources
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }
    
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
    }
    
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
    }
    
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }
    
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
    }
    
    if (m_stagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_stagingMemory, nullptr);
    }
}

void VulkanTexture::update(const void* data) {
    if (!data) {
        std::cerr << "Cannot update texture with null data" << std::endl;
        return;
    }
    
    
    try {
        VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend);
        
        // Implementing a safer texture update mechanism
        // std::cout << "Updating texture " << m_width << "x" << m_height << std::endl;
        
        // Calculate data size based on texture dimensions - ensure proper alignment
        VkDeviceSize bytesPerPixel = m_hasAlpha ? 4 : 3;
        VkDeviceSize imageSize = m_width * m_height * bytesPerPixel;
        
        // Create a staging buffer if not already present
        if (m_stagingBuffer == VK_NULL_HANDLE || m_stagingMemory == VK_NULL_HANDLE) {
            
            VkBufferCreateInfo stagingBufferInfo = {};
            stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.size = imageSize;
            stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            
            VkResult createResult = vkCreateBuffer(m_device, &stagingBufferInfo, nullptr, &m_stagingBuffer);
            if (createResult != VK_SUCCESS) {
                std::cerr << "Failed to create staging buffer for texture update: " << createResult << std::endl;
                return;
            }
            
            // Get staging buffer memory requirements
            VkMemoryRequirements stagingMemReq;
            vkGetBufferMemoryRequirements(m_device, m_stagingBuffer, &stagingMemReq);
            
            // Allocate staging memory (host visible for CPU writing)
            VkMemoryAllocateInfo stagingAllocInfo = {};
            stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            stagingAllocInfo.allocationSize = stagingMemReq.size;
            
            uint32_t memoryTypeIndex = vulkanBackend->findMemoryType(
                stagingMemReq.memoryTypeBits, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            stagingAllocInfo.memoryTypeIndex = memoryTypeIndex;
            
            VkResult allocResult = vkAllocateMemory(m_device, &stagingAllocInfo, nullptr, &m_stagingMemory);
            if (allocResult != VK_SUCCESS) {
                std::cerr << "Failed to allocate staging buffer memory: " << allocResult << std::endl;
                vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
                m_stagingBuffer = VK_NULL_HANDLE;
                return;
            }
            
            // Bind memory to staging buffer
            VkResult bindResult = vkBindBufferMemory(m_device, m_stagingBuffer, m_stagingMemory, 0);
            if (bindResult != VK_SUCCESS) {
                std::cerr << "Failed to bind memory to staging buffer: " << bindResult << std::endl;
                vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
                vkFreeMemory(m_device, m_stagingMemory, nullptr);
                m_stagingBuffer = VK_NULL_HANDLE;
                m_stagingMemory = VK_NULL_HANDLE;
                return;
            }
        }
        
        // Copy data to staging buffer
        void* mapped = nullptr;
        VkResult mapResult = vkMapMemory(m_device, m_stagingMemory, 0, imageSize, 0, &mapped);
        if (mapResult != VK_SUCCESS) {
            std::cerr << "Failed to map staging buffer memory: " << mapResult << std::endl;
            return;
        }
        
        // Validate the data pointer before copying
        if (data == nullptr) {
            std::cerr << "ERROR: Data pointer is null after validation" << std::endl;
            vkUnmapMemory(m_device, m_stagingMemory);
            return;
        }
        
        // SAFETY CHECK: Only copy if we have valid pointers and a reasonable size
        if (mapped && data && imageSize > 0 && imageSize < 100 * 1024 * 1024) { // 100MB max
            // Use byte-by-byte copy instead of memcpy to avoid potential segfaults with misaligned data
            uint8_t* src = (uint8_t*)data;
            uint8_t* dst = (uint8_t*)mapped;
            
            // For very large textures, show progress
            size_t progressMark = imageSize / 10;
            
            for (size_t i = 0; i < static_cast<size_t>(imageSize); i++) {
                dst[i] = src[i];
                
                // Show progress for large textures
                if (imageSize > 1000000 && i % progressMark == 0) {
                    // std::cout << "Copy progress: " << (i * 100 / imageSize) << "%" << std::endl;
                }
            }
            // std::cout << "Copy complete" << std::endl;
        } else {
            std::cerr << "ERROR: Invalid data or size for copy operation" << std::endl;
            vkUnmapMemory(m_device, m_stagingMemory);
            return;
        }
        
        vkUnmapMemory(m_device, m_stagingMemory);
        
        // Re-enabled texture upload code for proper rendering
        
        // Start a command buffer for the copy operation
        VkCommandBuffer commandBuffer = vulkanBackend->beginSingleTimeCommands();
        
        // Transition image layout for copy operation
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        
        // Copy buffer to image
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            static_cast<uint32_t>(m_width),
            static_cast<uint32_t>(m_height),
            1
        };
        
        vkCmdCopyBufferToImage(
            commandBuffer,
            m_stagingBuffer,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
        
        // Transition image layout for shader access
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        
        // End and submit the command buffer
        vulkanBackend->endSingleTimeCommands(commandBuffer);
        
        // std::cout << "Updated VulkanTexture of size " << m_width << "x" << m_height 
           //        << " (format: " << (m_hasAlpha ? "RGBA" : "RGB") << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in VulkanTexture::update: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in VulkanTexture::update" << std::endl;
    }
}

// VulkanShader implementation
VulkanShader::VulkanShader(RenderBackend* backend, const std::string& vertexSource, 
                         const std::string& fragmentSource)
    : Shader(backend, vertexSource, fragmentSource),
      m_vertexShaderModule(VK_NULL_HANDLE),
      m_fragmentShaderModule(VK_NULL_HANDLE),
      m_pipeline(VK_NULL_HANDLE),
      m_pipelineLayout(VK_NULL_HANDLE),
      m_descriptorSetLayout(VK_NULL_HANDLE),
      m_descriptorPool(VK_NULL_HANDLE),
      m_descriptorSet(VK_NULL_HANDLE),
      m_uniformBuffer(VK_NULL_HANDLE),
      m_uniformMemory(VK_NULL_HANDLE),
      m_defaultImage(VK_NULL_HANDLE),
      m_defaultImageMemory(VK_NULL_HANDLE),
      m_defaultImageView(VK_NULL_HANDLE),
      m_defaultSampler(VK_NULL_HANDLE),
      m_boundTexture(nullptr) {
    
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(backend);
    m_device = vulkanBackend->getDevice();
    
    // Store our uniform values
    m_uniformValues.clear();
    
    // GLSL doesn't work directly in Vulkan - we need to compile to SPIR-V
    // In a real implementation, we would use a library like shaderc or glslang
    // For this implementation we'll use pre-compiled shaders for simplicity
    
    // Basic vertex shader that passes through position and texture coordinates
    std::string basicVertexShader = R"(
        #version 450
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec2 inTexCoord;
        
        layout(location = 0) out vec2 fragTexCoord;
        
        layout(binding = 0) uniform UniformBufferObject {
            mat4 model;
            mat4 view;
            mat4 proj;
            vec4 params1;
            vec4 params2;
        } ubo;
        
        void main() {
            gl_Position = vec4(inPosition, 1.0);
            fragTexCoord = inTexCoord;
        }
    )";
    
    // Basic fragment shader that outputs a constant color
    std::string basicFragmentShader = R"(
        #version 450
        layout(location = 0) in vec2 fragTexCoord;
        
        layout(location = 0) out vec4 outColor;
        
        layout(binding = 1) uniform sampler2D texSampler;
        
        void main() {
            // For simplicity, just output a blue color
            outColor = vec4(0.0, 0.5, 1.0, 1.0);
            
            // In a real implementation we would use:
            // outColor = texture(texSampler, fragTexCoord);
        }
    )";
    
    // Use the provided shaders if they're not empty, otherwise use the basic ones
    std::string vertShaderCode = vertexSource.empty() ? basicVertexShader : vertexSource;
    std::string fragShaderCode = fragmentSource.empty() ? basicFragmentShader : fragmentSource;
    
    // Create shader modules
    m_vertexShaderModule = createShaderModule(vertShaderCode);
    if (m_vertexShaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to create vertex shader module" << std::endl;
        return;
    }
    
    m_fragmentShaderModule = createShaderModule(fragShaderCode);
    if (m_fragmentShaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to create fragment shader module" << std::endl;
        vkDestroyShaderModule(m_device, m_vertexShaderModule, nullptr);
        m_vertexShaderModule = VK_NULL_HANDLE;
        return;
    }
    
    // Create descriptor set layout (for uniform buffers)
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    // Create sampler layout binding
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    
    // Push constants allow us to quickly update small amounts of uniform data
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128; // 128 bytes for push constants
    
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return;
    }
    
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return;
    }
    
    // Create uniform buffer
    VkDeviceSize bufferSize = sizeof(UniformBuffer); // Use our uniform buffer structure
    vulkanBackend->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_uniformBuffer,
        m_uniformMemory
    );
    
    // Create a default uniform buffer with time only
    UniformBuffer ubo{};
    
    // Initialize time array - can't use initializer lists with arrays
    for (int i = 0; i < 4; i++) {
        ubo.time[i] = 0.0f;
    }
    
    // Map the buffer and copy our UBO data
    void* data;
    vkMapMemory(m_device, m_uniformMemory, 0, bufferSize, 0, &data);
    memcpy(data, &ubo, bufferSize);
    vkUnmapMemory(m_device, m_uniformMemory);
    
    // Allocate descriptor sets
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets" << std::endl;
        return;
    }
    
    // Update descriptor set with uniform buffer
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBuffer);
    
    // Create a default image and sampler for the shader - we'll update later with actual texture
    // Create a 1x1 white texture as default
    uint32_t whitePixel = 0xFFFFFFFF;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    // Create staging buffer for this pixel
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = sizeof(whitePixel);
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VulkanBackend* textureVulkanBackend = static_cast<VulkanBackend*>(m_backend);
    textureVulkanBackend->createBuffer(
        sizeof(whitePixel),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );
    
    // Copy white pixel to staging buffer
    void* textureData;
    vkMapMemory(m_device, stagingMemory, 0, sizeof(whitePixel), 0, &textureData);
    memcpy(textureData, &whitePixel, sizeof(whitePixel));
    vkUnmapMemory(m_device, stagingMemory);
    
    // Create default texture image
    VkImage defaultImage;
    VkDeviceMemory defaultImageMemory;
    VkImageView defaultImageView;
    VkSampler defaultSampler;
    
    // Create the default image
    VkImageCreateInfo defaultImageInfo = {};
    defaultImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    defaultImageInfo.imageType = VK_IMAGE_TYPE_2D;
    defaultImageInfo.extent.width = 1;
    defaultImageInfo.extent.height = 1;
    defaultImageInfo.extent.depth = 1;
    defaultImageInfo.mipLevels = 1;
    defaultImageInfo.arrayLayers = 1;
    defaultImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    defaultImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    defaultImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    defaultImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    defaultImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    defaultImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &defaultImageInfo, nullptr, &defaultImage) != VK_SUCCESS) {
        std::cerr << "Failed to create default image for shader" << std::endl;
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return;
    }
    
    // Allocate memory for default image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, defaultImage, &memRequirements);
    
    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memRequirements.size;
    memoryAllocInfo.memoryTypeIndex = textureVulkanBackend->findMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &defaultImageMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for default image" << std::endl;
        vkDestroyImage(m_device, defaultImage, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return;
    }
    
    vkBindImageMemory(m_device, defaultImage, defaultImageMemory, 0);
    
    // Transition image layout and copy data
    VkCommandBuffer commandBuffer = textureVulkanBackend->beginSingleTimeCommands();
    
    // Transition layout for copy
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = defaultImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    // Copy buffer to image
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {1, 1, 1};
    
    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer,
        defaultImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    
    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    textureVulkanBackend->endSingleTimeCommands(commandBuffer);
    
    // Create image view for the default texture
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = defaultImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &defaultImageView) != VK_SUCCESS) {
        std::cerr << "Failed to create image view for default texture" << std::endl;
        // Cleanup resources
        vkDestroyImage(m_device, defaultImage, nullptr);
        vkFreeMemory(m_device, defaultImageMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return;
    }
    
    // Create sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &defaultSampler) != VK_SUCCESS) {
        std::cerr << "Failed to create sampler for default texture" << std::endl;
        // Cleanup resources
        vkDestroyImageView(m_device, defaultImageView, nullptr);
        vkDestroyImage(m_device, defaultImage, nullptr);
        vkFreeMemory(m_device, defaultImageMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        return;
    }
    
    // Now we can create the descriptor set updates with actual texture
    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = defaultImageView;
    descriptorImageInfo.sampler = defaultSampler;
    
    std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &descriptorImageInfo;
    
    // Update descriptor sets with uniform buffer and texture
    vkUpdateDescriptorSets(m_device, 2, descriptorWrites.data(), 0, nullptr);
    
    // Clean up staging resources
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
    
    // Store default texture resources in member variables for later cleanup
    m_defaultImage = defaultImage;
    m_defaultImageMemory = defaultImageMemory;
    m_defaultImageView = defaultImageView;
    m_defaultSampler = defaultSampler;
    
    // std::cout << "Created default 1x1 white texture for shader" << std::endl;
    
    // Create the graphics pipeline
    createPipeline(vulkanBackend);
    
    // std::cout << "Created VulkanShader with vertex and fragment code" << std::endl;
}

// Helper method to create a shader module from GLSL source
VkShaderModule VulkanShader::createShaderModule(const std::string& code) {
    // Vector to hold the shader code as uint32_t
    std::vector<uint32_t> shaderCode;
    
    // Determine whether we should load a vertex or fragment shader
    bool isVertexShader = !(code.find("fragment") != std::string::npos || 
                           code.find("Fragment") != std::string::npos || 
                           code.find("frag") != std::string::npos);
    
    // Try to load precompiled SPIR-V from file
    std::string shaderFilePath;
    
    // Check if we are running from the project root or build directory
    if (isVertexShader) {
        // Try to load the material shader first, fall back to basic if needed
        if (std::ifstream("shaders/spirv/material.vert.spv").good()) {
            shaderFilePath = "shaders/spirv/material.vert.spv";
        } else if (std::ifstream("../shaders/spirv/material.vert.spv").good()) {
            shaderFilePath = "../shaders/spirv/material.vert.spv";
        } else if (std::ifstream("shaders/spirv/basic.vert.spv").good()) {
            shaderFilePath = "shaders/spirv/basic.vert.spv";
        } else if (std::ifstream("../shaders/spirv/basic.vert.spv").good()) {
            shaderFilePath = "../shaders/spirv/basic.vert.spv";
        }
    } else {
        // Try to load the material shader first, fall back to basic if needed
        if (std::ifstream("shaders/spirv/material.frag.spv").good()) {
            shaderFilePath = "shaders/spirv/material.frag.spv";
        } else if (std::ifstream("../shaders/spirv/material.frag.spv").good()) {
            shaderFilePath = "../shaders/spirv/material.frag.spv";
        } else if (std::ifstream("shaders/spirv/basic.frag.spv").good()) {
            shaderFilePath = "shaders/spirv/basic.frag.spv";
        } else if (std::ifstream("../shaders/spirv/basic.frag.spv").good()) {
            shaderFilePath = "../shaders/spirv/basic.frag.spv";
        }
    }
    
    // If we found a shader file, load it
    if (!shaderFilePath.empty()) {
        // std::cout << "Loading shader from: " << shaderFilePath << std::endl;
        
        // Open the file
        std::ifstream file(shaderFilePath, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            // Get the file size and reset position
            size_t fileSize = static_cast<size_t>(file.tellg());
            file.seekg(0);
            
            // Read the file into a temporary buffer
            std::vector<char> buffer(fileSize);
            file.read(buffer.data(), fileSize);
            file.close();
            
            // Convert the buffer to uint32_t (SPIR-V format)
            shaderCode.resize(fileSize / sizeof(uint32_t));
            memcpy(shaderCode.data(), buffer.data(), fileSize);
            
            // std::cout << "Successfully loaded SPIR-V shader: " << shaderFilePath 
           //            << " (" << fileSize << " bytes)" << std::endl;
        } else {
            // std::cout << "Could not open shader file: " << shaderFilePath << std::endl;
            // Fall back to hardcoded shaders
        }
    }
    
    // If we couldn't load from file, use fallback hardcoded SPIR-V
    if (shaderCode.empty()) {
        // std::cout << "Using fallback hardcoded shader" << std::endl;
        
        // Hardcoded fallback SPIR-V binary data for basic shaders
        // These are simplified versions that will work if the SPIR-V files are missing
        
        // Simplified vertex shader that just passes through position
        static const uint32_t basicVertexShaderSpv[] = {
            0x07230203, 0x00010000, 0x000d000a, 0x0000002e,
            0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
            0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
            0x00000000, 0x0000000a, 0x0000000d, 0x00000016,
            0x00030003, 0x00000002, 0x000001c2, 0x00040005,
            0x00000004, 0x6e69616d, 0x00000000, 0x00060005,
            0x00000008, 0x505f6c67, 0x65567265, 0x78657472,
            0x00000000, 0x00060006, 0x00000008, 0x00000000,
            0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006,
            0x00000008, 0x00000001, 0x505f6c67, 0x746e696f,
            0x657a6953, 0x00000000, 0x00070006, 0x00000008,
            0x00000002, 0x435f6c67, 0x4470696c, 0x61747369,
            0x0065636e, 0x00070006, 0x00000008, 0x00000003,
            0x435f6c67, 0x446c6c75, 0x61747369, 0x0065636e,
            0x00030005, 0x0000000a, 0x00000000, 0x00050005,
            0x0000000d, 0x69736f50, 0x6e6f6974, 0x00000000,
            0x00040005, 0x00000016, 0x6f6c6f43, 0x00000072,
            0x00040047, 0x0000000a, 0x0000000b, 0x0000000f,
            0x00040047, 0x0000000d, 0x0000001e, 0x00000000,
            0x00040047, 0x00000016, 0x0000001e, 0x00000001,
            0x00050048, 0x00000008, 0x00000000, 0x0000000b,
            0x00000000, 0x00050048, 0x00000008, 0x00000001,
            0x0000000b, 0x00000001, 0x00050048, 0x00000008,
            0x00000002, 0x0000000b, 0x00000003, 0x00050048,
            0x00000008, 0x00000003, 0x0000000b, 0x00000004,
            0x00030047, 0x00000008, 0x00000002, 0x00020013,
            0x00000002, 0x00030021, 0x00000003, 0x00000002,
            0x00030016, 0x00000006, 0x00000020, 0x00040017,
            0x00000007, 0x00000006, 0x00000004, 0x0004001e,
            0x00000008, 0x00000007, 0x00000006, 0x00040020,
            0x00000009, 0x00000003, 0x00000008, 0x0004003b,
            0x00000009, 0x0000000a, 0x00000003, 0x00040015,
            0x0000000b, 0x00000020, 0x00000001, 0x0004002b,
            0x0000000b, 0x0000000c, 0x00000000, 0x00040020,
            0x00000011, 0x00000001, 0x00000007, 0x0004003b,
            0x00000011, 0x0000000d, 0x00000001, 0x0004003b,
            0x00000011, 0x00000016, 0x00000001, 0x00040020,
            0x00000027, 0x00000003, 0x00000007, 0x00050036,
            0x00000002, 0x00000004, 0x00000000, 0x00000003,
            0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
            0x00000012, 0x0000000d, 0x0004003d, 0x00000007,
            0x00000017, 0x00000016, 0x00040020, 0x00000025,
            0x00000003, 0x00000007, 0x00050041, 0x00000025,
            0x00000026, 0x0000000a, 0x0000000c, 0x0003003e,
            0x00000026, 0x00000012, 0x000100fd, 0x00010038
        };
        
        // Simplified fragment shader that outputs a solid color
        static const uint32_t basicFragShaderSpv[] = {
            0x07230203, 0x00010000, 0x000d000a, 0x00000013,
            0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
            0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x0007000f, 0x00000004, 0x00000004, 0x6e69616d,
            0x00000000, 0x00000009, 0x0000000b, 0x00030010,
            0x00000004, 0x00000007, 0x00030003, 0x00000002,
            0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
            0x00000000, 0x00040005, 0x00000009, 0x6f6c6f43,
            0x00000072, 0x00040005, 0x0000000b, 0x67617246,
            0x00000000, 0x00040047, 0x00000009, 0x0000001e,
            0x00000000, 0x00040047, 0x0000000b, 0x0000001e,
            0x00000000, 0x00020013, 0x00000002, 0x00030021,
            0x00000003, 0x00000002, 0x00030016, 0x00000006,
            0x00000020, 0x00040017, 0x00000007, 0x00000006,
            0x00000004, 0x00040020, 0x00000008, 0x00000001,
            0x00000007, 0x0004003b, 0x00000008, 0x00000009,
            0x00000001, 0x00040020, 0x0000000a, 0x00000003,
            0x00000007, 0x0004003b, 0x0000000a, 0x0000000b,
            0x00000003, 0x00050036, 0x00000002, 0x00000004,
            0x00000000, 0x00000003, 0x000200f8, 0x00000005,
            0x0004003d, 0x00000007, 0x0000000c, 0x00000009,
            0x0003003e, 0x0000000b, 0x0000000c, 0x000100fd,
            0x00010038
        };
        
        // Select the appropriate shader based on the shader type
        if (isVertexShader) {
            shaderCode.assign(basicVertexShaderSpv, 
                              basicVertexShaderSpv + sizeof(basicVertexShaderSpv)/sizeof(basicVertexShaderSpv[0]));
        } else {
            shaderCode.assign(basicFragShaderSpv, 
                              basicFragShaderSpv + sizeof(basicFragShaderSpv)/sizeof(basicFragShaderSpv[0]));
        }
    }

    // Create the shader module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size() * sizeof(uint32_t);
    createInfo.pCode = shaderCode.data();
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

// Create a simplified graphics pipeline for pixel rendering
void VulkanShader::createPipeline(VulkanBackend* vulkanBackend) {
    // Create a proper shader pipeline
    // std::cout << "Creating full graphics pipeline" << std::endl;
    
    // Setup descriptor set layouts
    // Binding 0: Uniform buffer for transformation matrices
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    // Binding 1: Texture sampler
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    // Combine the layout bindings
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return;
    }
    
    // Define push constant range for material properties
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MaterialPushConstants); // Full material properties structure
    // std::cout << "Push constant size: " << sizeof(MaterialPushConstants) << " bytes" << std::endl;
    
    // Create pipeline layout with descriptor set layout and push constants
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return;
    }
    
    // Create descriptor pool with both uniform buffer and sampler
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor set" << std::endl;
        return;
    }
    
    // Now create the full pipeline
    // Configure shader stages
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = m_vertexShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = m_fragmentShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input state - describes the format of vertex data
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 8; // 2 for position, 2 for texcoord, 4 for color
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    // Texture coordinates
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = sizeof(float) * 2;
    // Color
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[2].offset = sizeof(float) * 4;
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    // Input assembly - how to assemble vertices into primitives
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(vulkanBackend->getSwapChainExtent().width);
    viewport.height = static_cast<float>(vulkanBackend->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = vulkanBackend->getSwapChainExtent();
    
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = vulkanBackend->getDefaultRenderPass();
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    // Create the graphics pipeline
    VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline! Error: " << result << std::endl;
        
        // Provide more details about error codes
        if (result == VK_ERROR_OUT_OF_HOST_MEMORY) {
            std::cerr << "  Error type: VK_ERROR_OUT_OF_HOST_MEMORY" << std::endl;
        } else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            std::cerr << "  Error type: VK_ERROR_OUT_OF_DEVICE_MEMORY" << std::endl;
        } else if (result == VK_ERROR_INVALID_SHADER_NV) {
            std::cerr << "  Error type: VK_ERROR_INVALID_SHADER_NV" << std::endl;
        } else if (result == -13) { // VK_ERROR_DEVICE_LOST
            std::cerr << "  Error type: VK_ERROR_DEVICE_LOST - Check shader compatibility" << std::endl;
        }
        m_pipeline = VK_NULL_HANDLE;
    } else {
        // std::cout << "Successfully created graphics pipeline" << std::endl;
    }
}

VulkanShader::~VulkanShader() {
    // Clean up Vulkan shader resources
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    }
    
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    }
    
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }
    
    if (m_vertexShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_vertexShaderModule, nullptr);
    }
    
    if (m_fragmentShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
    }
    
    if (m_uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    }
    
    if (m_uniformMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_uniformMemory, nullptr);
    }
    
    // Clean up default texture resources
    if (m_defaultSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_defaultSampler, nullptr);
    }
    
    if (m_defaultImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_defaultImageView, nullptr);
    }
    
    if (m_defaultImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_defaultImage, nullptr);
    }
    
    if (m_defaultImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_defaultImageMemory, nullptr);
    }
}

void VulkanShader::updateTexture(std::shared_ptr<Texture> texture) {
    if (!texture) {
        std::cerr << "Cannot update shader with null texture" << std::endl;
        return;
    }
    
    // Store the texture and update descriptor set if we have valid handles
    m_boundTexture = texture;
    VulkanTexture* vulkanTexture = static_cast<VulkanTexture*>(texture.get());
    
    // Skip descriptor update if we don't have valid descriptor set or sampler
    if (m_descriptorSet == VK_NULL_HANDLE || vulkanTexture->getVkSampler() == VK_NULL_HANDLE || 
        vulkanTexture->getVkImageView() == VK_NULL_HANDLE) {
        // std::cout << "Stored texture reference " << texture->getWidth() << "x" << texture->getHeight() 
         //          << " but cannot update descriptor set (not all handles are valid)" << std::endl;
        return;
    }
    
    try {
        // Update the descriptor set with the texture
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = vulkanTexture->getVkImageView();
        imageInfo.sampler = vulkanTexture->getVkSampler();
        
        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        
        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
        // std::cout << "Updated descriptor set with texture " << texture->getWidth() << "x" << texture->getHeight() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error updating descriptor set: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error updating descriptor set" << std::endl;
    }
}

void VulkanShader::setUniform(const std::string& name, float value) {
    // Store the uniform value
    m_uniformValues[name] = {value};
    
    // If this is a common matrix or parameter name, update the appropriate field
    if (name == "model" || name == "view" || name == "projection" || 
        name.find("param") != std::string::npos) {
        updateUniformBuffer();
    } else {
        // For non-standard uniforms, we'll use push constants
        // Map the uniform to a push constant range - this is a simplified approach
        // In a real implementation, we'd have a more sophisticated mapping system
        
        // For now, just use the VK_SHADER_STAGE_FRAGMENT_BIT for all push constants
        VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend);
        VkCommandBuffer cmdBuffer = vulkanBackend->getCurrentCommandBuffer();
        
        vkCmdPushConstants(
            cmdBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(float),
            &value
        );
    }
    
    // std::cout << "Setting float uniform " << name << " = " << value << std::endl;
}

void VulkanShader::setUniform(const std::string& name, int value) {
    // Convert int to float for storage
    float floatValue = static_cast<float>(value);
    m_uniformValues[name] = {floatValue};
    
    // Update uniform buffer or push constants as needed
    // For simplicity in the MVP, we'll just update the uniform buffer
    updateUniformBuffer();
    
    // std::cout << "Setting int uniform " << name << " = " << value << std::endl;
}

void VulkanShader::setUniform(const std::string& name, const std::vector<float>& values) {
    // Store the uniform values
    m_uniformValues[name] = values;
    
    // Update based on uniform name
    if (name == "model" || name == "view" || name == "projection") {
        updateUniformBuffer();
    } else {
        // For non-standard uniforms, use push constants if possible
        // This is a simplified approach - in a real implementation, we'd have a more robust system
        if (values.size() <= 32) { // Push constants are limited in size
            VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend);
            VkCommandBuffer cmdBuffer = vulkanBackend->getCurrentCommandBuffer();
            
            vkCmdPushConstants(
                cmdBuffer,
                m_pipelineLayout,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                values.size() * sizeof(float),
                values.data()
            );
        }
    }
    
    // std::cout << "Setting float array uniform " << name << " of size " << values.size() << std::endl;
}

void VulkanShader::setUniform(const std::string& name, float x, float y) {
    // Store as a vector
    m_uniformValues[name] = {x, y};
    
    // Skip push constants for now to avoid Vulkan issues
    // std::cout << "Setting vec2 uniform " << name << " = (" << x << ", " << y << ")" << std::endl;
}

void VulkanShader::setUniform(const std::string& name, float x, float y, float z) {
    // Store as a vector
    m_uniformValues[name] = {x, y, z};
    
    // Skip uniform buffer update for now to avoid Vulkan issues
    // std::cout << "Setting vec3 uniform " << name << " = (" << x << ", " << y << ", " << z << ")" << std::endl;
}

void VulkanShader::setUniform(const std::string& name, float x, float y, float z, float w) {
    // Store as a vector
    m_uniformValues[name] = {x, y, z, w};
    
    // Skip uniform buffer update for now to avoid Vulkan issues
    // std::cout << "Setting vec4 uniform " << name << " = (" << x << ", " << y << ", " << z << ", " << w << ")" << std::endl;
}

// Set material properties based on the material type
void VulkanShader::setMaterial(MaterialType materialType) {
    // Create super simplified material push constants
    MaterialPushConstants constants{};
    
    // Only set the material type ID
    constants.materialType = static_cast<uint32_t>(materialType);
    
    // Store in shader state
    m_materialPushConstants = constants;
    
    // Update the push constants if pipeline is active
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend);
    if (vulkanBackend && m_pipelineLayout != VK_NULL_HANDLE) {
        VkCommandBuffer cmdBuffer = vulkanBackend->getCurrentCommandBuffer();
        if (cmdBuffer != VK_NULL_HANDLE) {
            vkCmdPushConstants(
                cmdBuffer,
                m_pipelineLayout,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(MaterialPushConstants),
                &m_materialPushConstants
            );
        }
    }
}

void VulkanShader::updateUniformBuffer() {
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(m_backend);
    
    if (!vulkanBackend || m_uniformBuffer == VK_NULL_HANDLE || m_uniformMemory == VK_NULL_HANDLE) {
        std::cerr << "Cannot update uniform buffer - null handles" << std::endl;
        return;
    }
    
    // Create super-simplified uniform buffer with just time
    UniformBuffer ubo{};
    
    // Set time information
    float currentTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    static float lastTime = currentTime;
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;
    
    ubo.time[0] = currentTime;
    ubo.time[1] = deltaTime;
    ubo.time[2] = static_cast<float>(SDL_GetTicks()) / 16.67f; // approximate frame count
    ubo.time[3] = 0.0f;
    
    // Map memory and update the uniform buffer
    void* data;
    vkMapMemory(m_device, m_uniformMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(m_device, m_uniformMemory);
}

// VulkanRenderTarget implementation
VulkanRenderTarget::VulkanRenderTarget(RenderBackend* backend, int width, int height, 
                                     bool hasDepth, bool multisampled)
    : RenderTarget(backend, width, height, hasDepth, multisampled),
      m_framebuffer(VK_NULL_HANDLE),
      m_renderPass(VK_NULL_HANDLE),
      m_colorImage(VK_NULL_HANDLE),
      m_colorImageView(VK_NULL_HANDLE),
      m_colorMemory(VK_NULL_HANDLE),
      m_depthImage(VK_NULL_HANDLE),
      m_depthImageView(VK_NULL_HANDLE),
      m_depthMemory(VK_NULL_HANDLE) {
    
    VulkanBackend* vulkanBackend = static_cast<VulkanBackend*>(backend);
    m_device = vulkanBackend->getDevice();
    
    // For this MVP implementation, we'll create a basic render target
    // with a color attachment (and optionally a depth attachment)
    
    // Create color attachment
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = multisampled ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_colorImage) != VK_SUCCESS) {
        std::cerr << "Failed to create color image for render target" << std::endl;
        return;
    }
    
    // Allocate memory for color image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_colorImage, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for color image" << std::endl;
        vkDestroyImage(m_device, m_colorImage, nullptr);
        m_colorImage = VK_NULL_HANDLE;
        return;
    }
    
    vkBindImageMemory(m_device, m_colorImage, m_colorMemory, 0);
    
    // Create color image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_colorImageView) != VK_SUCCESS) {
        std::cerr << "Failed to create color image view for render target" << std::endl;
        return;
    }
    
    // Create depth attachment if needed
    if (hasDepth) {
        imageInfo.format = VK_FORMAT_D32_SFLOAT; // Simple depth format for MVP
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        
        if (vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
            std::cerr << "Failed to create depth image for render target" << std::endl;
            return;
        }
        
        vkGetImageMemoryRequirements(m_device, m_depthImage, &memRequirements);
        
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = vulkanBackend->findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS) {
            std::cerr << "Failed to allocate memory for depth image" << std::endl;
            vkDestroyImage(m_device, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
            return;
        }
        
        vkBindImageMemory(m_device, m_depthImage, m_depthMemory, 0);
        
        viewInfo.image = m_depthImage;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
            std::cerr << "Failed to create depth image view for render target" << std::endl;
            return;
        }
    }
    
    // Create render pass
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    
    // Define color attachment
    VkAttachmentDescription colorAttachmentDesc = {};
    colorAttachmentDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorAttachmentDesc.samples = multisampled ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments.push_back(colorAttachmentDesc);
    
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs.push_back(colorAttachmentRef);
    
    // Define depth attachment if needed
    VkAttachmentReference depthAttachmentRef = {};
    if (hasDepth) {
        VkAttachmentDescription depthAttachmentDesc = {};
        depthAttachmentDesc.format = VK_FORMAT_D32_SFLOAT;
        depthAttachmentDesc.samples = multisampled ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachmentDesc);
        
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    
    // Create subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = colorAttachmentRefs.size();
    subpass.pColorAttachments = colorAttachmentRefs.data();
    if (hasDepth) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }
    
    // Create render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    
    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass for render target" << std::endl;
        return;
    }
    
    // Create framebuffer
    std::vector<VkImageView> framebufferAttachments;
    framebufferAttachments.push_back(m_colorImageView);
    if (hasDepth) {
        framebufferAttachments.push_back(m_depthImageView);
    }
    
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
    framebufferInfo.pAttachments = framebufferAttachments.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
    
    if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create framebuffer for render target" << std::endl;
        return;
    }
    
    // std::cout << "Created VulkanRenderTarget of size " << width << "x" << height << std::endl;
}

VulkanRenderTarget::~VulkanRenderTarget() {
    // Clean up Vulkan render target resources
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    }
    
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }
    
    if (m_colorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_colorImageView, nullptr);
    }
    
    if (m_colorImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_colorImage, nullptr);
    }
    
    if (m_colorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_colorMemory, nullptr);
    }
    
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_depthImageView, nullptr);
    }
    
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_depthImage, nullptr);
    }
    
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_depthMemory, nullptr);
    }
}

// ImGui integration methods
uint32_t VulkanBackend::getGraphicsQueueFamily() const {
    VulkanQueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    return indices.graphicsFamily.value();
}

VkDescriptorPool VulkanBackend::getDescriptorPool() const {
    // Create a dedicated descriptor pool for ImGui if needed
    static VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    
    if (imguiPool == VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        
        VkResult result = vkCreateDescriptorPool(m_device, &pool_info, nullptr, &imguiPool);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create ImGui descriptor pool" << std::endl;
            return VK_NULL_HANDLE;
        }
        
        // std::cout << "Created dedicated descriptor pool for ImGui" << std::endl;
    }
    
    return imguiPool;
}

// ImGui integration removed since editor tool was removed
/*
VkRenderPass VulkanBackend::createImGuiRenderPass() {
    // Create a dedicated render pass for ImGui
    VkRenderPass imguiRenderPass = VK_NULL_HANDLE;
    
    // Attachment description for color (same as the swapchain format)
    VkAttachmentDescription attachment = {};
    attachment.format = m_swapChainImageFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // Load existing content
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // Subpass dependency to ensure proper synchronization
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    // Attachment reference for color
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Subpass definition
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    // Create the render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkResult result = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &imguiRenderPass);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create ImGui render pass" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    // std::cout << "Created dedicated render pass for ImGui" << std::endl;
    return imguiRenderPass;
}
*/

/*
bool VulkanBackend::uploadImGuiFonts() {
    // Create a command buffer for font upload
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    // Let ImGui implementation create and upload fonts
    if (!ImGui_ImplVulkan_CreateFontsTexture(commandBuffer)) {
        std::cerr << "Failed to create ImGui fonts texture" << std::endl;
        vkEndCommandBuffer(commandBuffer);  // Removed extra parameter
        return false;
    }
    
    // Submit and wait for completion
    endSingleTimeCommands(commandBuffer);
    
    // Cleanup font data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    
    // std::cout << "Uploaded ImGui fonts texture" << std::endl;
    return true;
}
*/

} // namespace PixelPhys