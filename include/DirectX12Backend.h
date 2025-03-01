#pragma once

#include "RenderBackend.h"
#include "RenderResources.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

// Forward declarations of DirectX 12 types to avoid including d3d12.h in the header
// These will be properly included in the implementation file
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;
struct ID3D12Fence;
struct IDXGISwapChain3;
struct IDXGIFactory4;
struct IDXGIAdapter1;
struct ID3DBlob;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;
struct D3D12_VIEWPORT;
struct D3D12_RECT;

namespace PixelPhys {

// DirectX 12 implementation of Buffer
class DX12Buffer : public Buffer {
public:
    DX12Buffer(RenderBackend* backend, Type type, size_t size, const void* data);
    ~DX12Buffer() override;

    ID3D12Resource* getD3DResource() const { return m_resource; }
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUDescriptorHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUDescriptorHandle() const;

private:
    ID3D12Resource* m_resource;
    ID3D12Resource* m_uploadResource; // For staging data to GPU
    void* m_mappedData; // For CPU-writable buffer data
    unsigned int m_descriptorIndex; // Index in the descriptor heap
};

// DirectX 12 implementation of Texture
class DX12Texture : public Texture {
public:
    DX12Texture(RenderBackend* backend, int width, int height, bool hasAlpha);
    ~DX12Texture() override;

    void update(const void* data);
    
    ID3D12Resource* getD3DResource() const { return m_resource; }
    D3D12_CPU_DESCRIPTOR_HANDLE getSRVCPUDescriptorHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE getSRVGPUDescriptorHandle() const;

private:
    ID3D12Resource* m_resource;
    ID3D12Resource* m_uploadResource; // For staging data to GPU
    unsigned int m_srvDescriptorIndex; // Index in the SRV descriptor heap
    unsigned int m_rtvDescriptorIndex; // Index in the RTV descriptor heap (if used as render target)
};

// DirectX 12 implementation of Shader
class DX12Shader : public Shader {
public:
    DX12Shader(RenderBackend* backend, const std::string& vertexSource, const std::string& fragmentSource);
    ~DX12Shader() override;

    ID3D12PipelineState* getD3DPipelineState() const { return m_pipelineState; }
    ID3D12RootSignature* getD3DRootSignature() const { return m_rootSignature; }
    
    // Uniform setters
    void setUniform(const std::string& name, float value) override;
    void setUniform(const std::string& name, int value) override;
    void setUniform(const std::string& name, const std::vector<float>& values) override;
    void setUniform(const std::string& name, float x, float y) override;
    void setUniform(const std::string& name, float x, float y, float z) override;
    void setUniform(const std::string& name, float x, float y, float z, float w) override;

private:
    ID3D12PipelineState* m_pipelineState;
    ID3D12RootSignature* m_rootSignature;
    ID3D12Resource* m_constantBuffer;
    ID3DBlob* m_vertexShaderBlob;
    ID3DBlob* m_pixelShaderBlob;
    
    // Map of uniform names to offsets in the constant buffer
    std::unordered_map<std::string, unsigned int> m_uniformOffsets;
    void* m_constantBufferData; // CPU-visible copy of constant buffer data
    
    // Helper method to compile shader from HLSL source
    ID3DBlob* compileShader(const std::string& source, const std::string& entryPoint, const std::string& target);
};

// DirectX 12 implementation of RenderTarget
class DX12RenderTarget : public RenderTarget {
public:
    DX12RenderTarget(RenderBackend* backend, int width, int height, bool hasDepth, bool multisampled);
    ~DX12RenderTarget() override;

    ID3D12Resource* getColorResource() const { return m_colorResource; }
    ID3D12Resource* getDepthResource() const { return m_depthResource; }
    D3D12_CPU_DESCRIPTOR_HANDLE getRTVCPUDescriptorHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDSVCPUDescriptorHandle() const;

private:
    ID3D12Resource* m_colorResource;
    ID3D12Resource* m_depthResource;
    unsigned int m_rtvDescriptorIndex; // Index in the RTV descriptor heap
    unsigned int m_dsvDescriptorIndex; // Index in the DSV descriptor heap
};

// DirectX 12 implementation of RenderBackend
class DirectX12Backend : public RenderBackend {
public:
    DirectX12Backend(int screenWidth, int screenHeight);
    ~DirectX12Backend() override;

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
    
    void setViewport(int x, int y, int width, int height) override;
    void setClearColor(float r, float g, float b, float a) override;
    void clear() override;
    
    void beginShadowPass() override;
    void beginMainPass() override;
    void beginPostProcessPass() override;
    
    void* getNativeHandle() override;
    BackendType getType() const override { return BackendType::DirectX12; }
    
    bool supportsFeature(const std::string& featureName) const override;
    std::string getRendererInfo() const override;

    // DirectX 12-specific methods - to be called by DirectX resources
    ID3D12Device* getDevice() const { return m_device; }
    ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue; }
    ID3D12GraphicsCommandList* getCommandList() const { return m_commandList; }
    ID3D12DescriptorHeap* getRTVDescriptorHeap() const { return m_rtvDescriptorHeap; }
    ID3D12DescriptorHeap* getDSVDescriptorHeap() const { return m_dsvDescriptorHeap; }
    ID3D12DescriptorHeap* getSRVDescriptorHeap() const { return m_srvDescriptorHeap; }
    unsigned int getRTVDescriptorSize() const { return m_rtvDescriptorSize; }
    unsigned int getDSVDescriptorSize() const { return m_dsvDescriptorSize; }
    unsigned int getSRVDescriptorSize() const { return m_srvDescriptorSize; }
    
    // Resource allocation helpers
    unsigned int allocateRTVDescriptor();
    unsigned int allocateDSVDescriptor();
    unsigned int allocateSRVDescriptor();
    void createBuffer(size_t size, bool isUploadHeap, ID3D12Resource** resource);
    void uploadDataToBuffer(ID3D12Resource* destination, const void* data, size_t size);
    void executeCommandList();
    void waitForGPU();

private:
    // DirectX 12 core objects
    IDXGIFactory4* m_factory;
    IDXGIAdapter1* m_adapter;
    ID3D12Device* m_device;
    ID3D12CommandQueue* m_commandQueue;
    IDXGISwapChain3* m_swapChain;
    ID3D12CommandAllocator* m_commandAllocator;
    ID3D12GraphicsCommandList* m_commandList;
    ID3D12Fence* m_fence;
    UINT64 m_fenceValue;
    HANDLE m_fenceEvent;
    
    // Descriptor heaps
    ID3D12DescriptorHeap* m_rtvDescriptorHeap;
    ID3D12DescriptorHeap* m_dsvDescriptorHeap;
    ID3D12DescriptorHeap* m_srvDescriptorHeap;
    unsigned int m_rtvDescriptorSize;
    unsigned int m_dsvDescriptorSize;
    unsigned int m_srvDescriptorSize;
    unsigned int m_currentRTVDescriptor;
    unsigned int m_currentDSVDescriptor;
    unsigned int m_currentSRVDescriptor;
    
    // Swapchain resources
    static const UINT FRAME_COUNT = 2; // Double buffering
    ID3D12Resource* m_renderTargets[FRAME_COUNT];
    UINT m_frameIndex;
    
    // Rendering resources
    ID3D12Resource* m_depthStencilBuffer;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    
    // Resource management
    std::shared_ptr<RenderTarget> m_shadowMapTarget;
    std::shared_ptr<RenderTarget> m_mainRenderTarget;
    std::shared_ptr<Buffer> m_fullscreenQuadVertexBuffer;
    std::shared_ptr<Buffer> m_fullscreenQuadIndexBuffer;
    
    // Current state
    std::shared_ptr<Shader> m_currentShader;
    std::array<float, 4> m_clearColor;
    
    // Initialization methods
    bool createDevice();
    bool createCommandQueue();
    bool createSwapChain();
    bool createDescriptorHeaps();
    bool createFrameResources();
    bool createRenderTargetViews();
    bool createDepthStencilBuffer();
    bool createCommandList();
    bool createSyncObjects();
    bool createFullscreenQuad();
    
    // Helper methods
    bool findSupportedAdapter();
    void waitForPreviousFrame();
};

} // namespace PixelPhys