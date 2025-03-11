#pragma once

#include "RenderBackend.h"
#include "RenderResources.h"
#include "Materials.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

// Include proper Vulkan headers instead of forward declarations
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

namespace PixelPhys {

// Forward declarations
struct VulkanSwapChainSupportDetails;
struct VulkanQueueFamilyIndices;
class VulkanBackend; // Forward declaration to break circular dependency

// Vulkan implementation of Buffer
class VulkanBuffer : public Buffer {
public:
    VulkanBuffer(RenderBackend* backend, Type type, size_t size, const void* data);
    ~VulkanBuffer() override;

    VkBuffer getVkBuffer() const { return m_buffer; }
    VkDeviceMemory getVkDeviceMemory() const { return m_memory; }

private:
    VkBuffer m_buffer;
    VkDeviceMemory m_memory;
    VkDevice m_device; // Cached device handle for cleanup
    
    // Helper method to create and copy from a staging buffer
    void createAndCopyFromStagingBuffer(VulkanBackend* vulkanBackend, const void* data, size_t size);
};

// Vulkan implementation of Texture
class VulkanTexture : public Texture {
public:
    VulkanTexture(RenderBackend* backend, int width, int height, bool hasAlpha);
    ~VulkanTexture() override;

    void update(const void* data);
    
    VkImage getVkImage() const { return m_image; }
    VkImageView getVkImageView() const { return m_imageView; }
    VkSampler getVkSampler() const { return m_sampler; }

private:
    VkImage m_image;
    VkDeviceMemory m_memory;
    VkImageView m_imageView;
    VkSampler m_sampler;
    VkDevice m_device; // Cached device handle for cleanup
    
    // Staging buffer for texture updates
    VkBuffer m_stagingBuffer;
    VkDeviceMemory m_stagingMemory;
};

// Material push constant struct to match shader - super simplified
struct MaterialPushConstants {
    // Material type ID only
    uint32_t materialType;
};

// Vulkan implementation of Shader
class VulkanShader : public Shader {
public:
    VulkanShader(RenderBackend* backend, const std::string& vertexSource, const std::string& fragmentSource);
    ~VulkanShader() override;

    VkPipeline getVkPipeline() const { return m_pipeline; }
    VkPipelineLayout getVkPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout getVkDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorSet getVkDescriptorSet() const { return m_descriptorSet; }
    
    // Uniform setters
    void setUniform(const std::string& name, float value) override;
    void setUniform(const std::string& name, int value) override;
    void setUniform(const std::string& name, const std::vector<float>& values) override;
    void setUniform(const std::string& name, float x, float y) override;
    void setUniform(const std::string& name, float x, float y, float z) override;
    void setUniform(const std::string& name, float x, float y, float z, float w) override;
    
    // Material property setter
    void setMaterial(MaterialType materialType);
    
    // Update texture in shader
    void updateTexture(std::shared_ptr<Texture> texture);
    
    // Get the currently bound texture
    std::shared_ptr<Texture> getBoundTexture() const { return m_boundTexture; }

private:
    // Super-simplified uniform buffer with just time
    struct UniformBuffer {
        float time[4];  // x = total time, y = delta time, z = frame count, w = unused
    };
    
    VkShaderModule m_vertexShaderModule;
    VkShaderModule m_fragmentShaderModule;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;
    VkDevice m_device; // Cached device handle for cleanup
    
    // Uniform buffer for shader parameters
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    // Default 1x1 white texture for shader
    VkImage m_defaultImage;
    VkDeviceMemory m_defaultImageMemory;
    VkImageView m_defaultImageView;
    VkSampler m_defaultSampler;
    
    // Currently bound texture
    std::shared_ptr<Texture> m_boundTexture;
    
    // Material push constants
    MaterialPushConstants m_materialPushConstants;
    
    // Map to store uniform values by name
    std::unordered_map<std::string, std::vector<float>> m_uniformValues;
    
    // Helper methods
    VkShaderModule createShaderModule(const std::string& source);
    void createPipeline(VulkanBackend* vulkanBackend);
    void updateUniformBuffer();
};

// Vulkan implementation of RenderTarget
class VulkanRenderTarget : public RenderTarget {
public:
    VulkanRenderTarget(RenderBackend* backend, int width, int height, bool hasDepth, bool multisampled);
    ~VulkanRenderTarget() override;

    VkFramebuffer getVkFramebuffer() const { return m_framebuffer; }
    VkRenderPass getVkRenderPass() const { return m_renderPass; }

private:
    VkFramebuffer m_framebuffer;
    VkRenderPass m_renderPass;
    VkImage m_colorImage;
    VkImageView m_colorImageView;
    VkDeviceMemory m_colorMemory;
    VkImage m_depthImage;
    VkImageView m_depthImageView;
    VkDeviceMemory m_depthMemory;
    VkDevice m_device; // Cached device handle for cleanup
};

// Vulkan implementation of RenderBackend
class VulkanBackend : public RenderBackend {
public:
    VulkanBackend(int screenWidth, int screenHeight);
    ~VulkanBackend() override;

    // Implementation of RenderBackend methods
    bool initialize() override;
    void cleanup() override;
    
    void beginFrame() override;
    void endFrame() override;
    
    std::shared_ptr<Buffer> createVertexBuffer(size_t size, const void* data) override;
    std::shared_ptr<Buffer> createIndexBuffer(size_t size, const void* data) override;
    std::shared_ptr<Buffer> createUniformBuffer(size_t size) override;
    void updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, size_t size) override;
    
    std::shared_ptr<Texture> createTexture(int width, int height, bool hasAlpha) override;
    void updateTexture(std::shared_ptr<Texture> texture, const void* data) override;
    
    std::shared_ptr<Shader> createShader(const std::string& vertexSource, const std::string& fragmentSource) override;
    void bindShader(std::shared_ptr<Shader> shader) override;
    
    std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, bool hasDepth, bool multisampled) override;
    void bindRenderTarget(std::shared_ptr<RenderTarget> target) override;
    void bindDefaultRenderTarget() override;
    
    void drawMesh(std::shared_ptr<Buffer> vertexBuffer, size_t vertexCount, 
                 std::shared_ptr<Buffer> indexBuffer, size_t indexCount) override;
    void drawFullscreenQuad() override;
    
    // Enhanced drawing method for visualizing pixels with material properties
    void drawRectangle(float x, float y, float width, float height, float r, float g, float b);
    
    // Material-based rendering for pixels
    void drawMaterialRectangle(float x, float y, float width, float height, 
                              PixelPhys::MaterialType materialType);
    
    // Batched pixel rendering system for high performance
    void beginPixelBatch(float pixelSize);
    void addPixelToBatch(float x, float y, float r, float g, float b);
    void drawPixelBatch();
    void endPixelBatch();
    
    // Internal batched rendering helper
    void drawBatchInternal(size_t instanceCount);
    
    void setViewport(int x, int y, int width, int height) override;
    void setClearColor(float r, float g, float b, float a) override;
    void clear() override;
    
    void beginShadowPass() override;
    void beginMainPass() override;
    void beginPostProcessPass() override;
    
    void* getNativeHandle(int handleType = 0) override;
    BackendType getType() const override { return BackendType::Vulkan; }
    
    bool supportsFeature(const std::string& featureName) const override;
    std::string getRendererInfo() const override;

    // Vulkan-specific methods - to be called by Vulkan resources
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    uint32_t getGraphicsQueueFamily() const;
    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkRenderPass getDefaultRenderPass() const { return m_defaultRenderPass; }
    VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; }
    VkInstance getInstance() const { return m_instance; }
    VkDescriptorPool getDescriptorPool() const;
    uint32_t getSwapchainImageCount() const { return static_cast<uint32_t>(m_swapChainImages.size()); }
    
    // ImGui integration methods - disabled since editor tool was removed
    //VkRenderPass createImGuiRenderPass();
    //bool uploadImGuiFonts();
    
    // Access to command buffers and current frame for shader uniform updates
    VkCommandBuffer getCurrentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }
    size_t getCurrentFrameIndex() const { return m_currentFrame; }
    
    // Screen dimensions for rendering
    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }
    
    // Memory allocation helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkFlags properties);
    void createBuffer(VkDeviceSize size, VkFlags usage, VkFlags properties, 
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    // Vulkan core objects
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkSurfaceKHR m_surface;
    
    // Window and swapchain
    VkSwapchainKHR m_swapChain;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    
    // Depth resources for framebuffers
    std::vector<VkImage> m_depthImages;
    std::vector<VkDeviceMemory> m_depthImageMemories;
    std::vector<VkImageView> m_depthImageViews;
    
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    
    // Rendering resources
    VkRenderPass m_defaultRenderPass;
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    // Synchronization objects
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
    size_t m_currentFrame;
    
    // Resource management
    std::shared_ptr<RenderTarget> m_shadowMapTarget;
    std::shared_ptr<RenderTarget> m_mainRenderTarget;
    std::shared_ptr<Buffer> m_fullscreenQuadVertexBuffer;
    std::shared_ptr<Buffer> m_fullscreenQuadIndexBuffer;
    
    // Current state
    std::shared_ptr<Shader> m_currentShader;
    VkRect2D m_viewport;
    std::array<float, 4> m_clearColor;
    uint32_t m_currentImageIndex;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    bool m_renderPassInProgress;
    
    // Batched rendering resources
    struct PixelInstance {
        float posX, posY;  // Position
        float r, g, b;     // Color
    };
    
    std::vector<PixelInstance> m_pixelBatch;
    float m_batchPixelSize;
    VkBuffer m_instanceBuffer;
    VkDeviceMemory m_instanceBufferMemory;
    size_t m_maxInstanceCount;
    std::shared_ptr<Buffer> m_batchVertexBuffer;
    std::shared_ptr<Buffer> m_batchIndexBuffer;
    bool m_isBatchActive;
    
    // Specialized pipeline for batch rendering
    VkPipeline m_batchPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_batchPipelineLayout = VK_NULL_HANDLE;
    
    // Helper method to create the batch rendering pipeline
    void createBatchPipeline();
    
    // Helper to create shader module from GLSL source code
    VkShaderModule createShaderModule(const std::string& code);
    
    // Constants
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    
    // Debug messenger helper
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
    // Initialization methods
    bool createInstance();
    bool setupDebugMessenger();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createFullscreenQuad();
    
    // Helper methods
    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);
    std::vector<const char*> getRequiredExtensions(bool enableValidationLayers);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VulkanQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    VulkanSwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    // Swapchain recreation
    void cleanupSwapChain();
    void recreateSwapChain();
};

} // namespace PixelPhys