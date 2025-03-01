# PixelPhys2D Development Guidelines

## Build Commands
- **Preferred build method**: `./build_with_shaders.sh` (compiles shaders and builds with Vulkan)
- Standard build: `./build.sh -v` (Vulkan only)
- Debug build: `./build.sh -v -d` (Vulkan debug)
- Clean build: `./build.sh -v -c` (Vulkan clean)
- Build and run: `./build.sh -v -r` (Vulkan run)
- Shader compilation: `./compile_shaders.sh` (converts GLSL to SPIR-V)
- Windows: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_VULKAN=ON && cmake --build . --config Release`

## Dependencies
- Linux: `sudo apt-get install build-essential cmake libsdl2-dev libvulkan-dev vulkan-tools`
- Windows: Install SDL2 and Vulkan SDK from LunarG

## Vulkan Implementation Strategy

### Overview
The project uses a modular backend system with a common RenderBackend interface implemented by both OpenGLBackend and VulkanBackend. This allows the engine to choose the best available graphics API at runtime.

### Vulkan Initialization Flow
1. **Instance Creation**: 
   - Check for validation layers
   - Get extensions from SDL2
   - Create debug messenger in debug builds

2. **Device Selection**:
   - Find physical device with graphics and present queues
   - Check for swapchain support
   - Create logical device with required extensions

3. **Surface and Swapchain**:
   - Create window surface via SDL2 (SDL_Vulkan_CreateSurface)
   - Configure swapchain for proper presenting: triple buffering if supported
   - Create image views for swapchain images

4. **Render Passes**:
   - Create default render pass for swapchain rendering
   - Create specialized render passes for shadow mapping, post-processing
   - Configure proper attachments, dependencies and subpasses

5. **Command Buffers**:
   - Create command pool for graphics queue
   - Allocate primary command buffers for each frame in flight (2)
   - Setup synchronization objects (semaphores and fences)

### Multi-Pass Rendering Architecture
The Vulkan backend implements a multi-pass rendering system with:

1. **Shadow Pass**: 
   - Renders scene depth to shadow map
   - Uses depth-only optimization
   - Stores in dedicated render target

2. **Main Pass**:
   - Renders main scene color and depth
   - Writes to emissive texture for bloom/glow
   - Uses material properties for rendering

3. **Post-Processing Pass**:
   - Takes results from shadow and main passes
   - Applies effects (bloom, volumetric lighting)
   - Renders to swapchain for display

### Resource Management
- **Buffers**: Properly managed with staging buffers for optimal transfer 
  - Vertex buffers in device-local memory
  - Uniform buffers in host-visible memory
  - Index buffers in device-local memory
  
- **Textures**: Created with proper format and optimal memory
  - Uses staging buffers for updates
  - Handles appropriate image layout transitions
  - Uses proper filtering and addressing modes

- **Memory Types**: Uses findMemoryType to ensure proper memory allocation
  - Device-local for performance-critical resources
  - Host-visible for CPU-writable resources
  - Host-coherent to avoid explicit flushing

### Render Pass State Management
- Track active render pass state with m_renderPassInProgress flag
- Always check state before beginning/ending render passes
- Handle transitions between passes correctly
- Use separate class member for tracking state (m_renderPassInProgress)

### Key Vulkan Classes
- **VulkanBackend**: Main implementation of RenderBackend interface
- **VulkanBuffer**: Implementation of Buffer for vertex, index, uniform data
- **VulkanTexture**: Implementation of Texture with proper image handling
- **VulkanShader**: Implementation of Shader with pipeline and descriptor set management
- **VulkanRenderTarget**: Implementation of RenderTarget with framebuffer and render pass

### Synchronization
- Use MAX_FRAMES_IN_FLIGHT = 2 for double/triple buffering
- Image acquisition synchronized with imageAvailableSemaphores
- Rendering completion signaled with renderFinishedSemaphores
- CPU-GPU sync with inFlightFences
- Track per-image synchronization with imagesInFlight

### Command Recording Flow
1. Wait for previous frame's fence to be signaled
2. Acquire next swapchain image with semaphore
3. Begin command buffer recording
4. Execute render passes (shadow/main/post) as needed
5. End command buffer and submit to queue
6. Present swapchain image when rendering is complete

### Vulkan-Specific Error Handling
- Check VkResult for all Vulkan API calls
- Handle swapchain recreation for window resize/minimize
- Proper cleanup in destructors and cleanup() methods
- Debug validation layers in debug builds

### Debugging Vulkan
- Enable validation layers in debug builds
- Use VK_EXT_debug_utils for detailed error reports
- Custom debug callback for helpful validation messages
- Check for `vkCmdEndRenderPass()` without corresponding `vkCmdBeginRenderPass()`
- Track command buffer state carefully

### Shader Management
For GLSL shader compilation:
1. Read GLSL source files
2. Compile to SPIR-V using glslang
3. Create shader modules from SPIR-V
4. Create pipeline with proper state

For pre-compiled SPIR-V:
1. Load SPIR-V binary directly
2. Create shader modules
3. Create pipeline with proper state

## Styling Guidelines
- Use C++17 standard with all core features (smart pointers, std::optional, etc.)
- Class names: PascalCase (e.g., `World`, `Renderer`)
- Methods/functions: camelCase (e.g., `updatePlayer()`)
- Member variables: m_camelCase prefix (e.g., `m_isDirty`)
- Constants: UPPER_CASE (e.g., `TARGET_FPS`)
- Namespaces: Use `PixelPhys` namespace for all core components
- Indentation: 4 spaces (no tabs)
- Include guards: #pragma once (preferred over #ifndef guards)
- Imports: Group standard library, then third-party, then project includes
- Error handling: Check VkResult for all Vulkan API calls, use assertions for invariants
- Line length: Maximum 100 characters
- Memory management: Use smart pointers (std::unique_ptr/std::shared_ptr) over raw pointers
- Vulkan rendering: Always track render pass state with m_renderPassInProgress flag
- Buffer updates: Use proper staging buffers for GPU-visible memory
- Shader management: Use SPIR-V shaders in shaders/spirv/ directory

## Architecture
- Split physics simulation into chunks for multithreading
- Use checkerboard pattern for cell updates to avoid race conditions
- Mark dirty regions to optimize active simulation areas
- Follow data-oriented design for better cache performance
- Abstract rendering API behind RenderBackend interface
- Support both OpenGL and Vulkan backends
- Multi-pass rendering for advanced effects