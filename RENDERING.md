Create a Swapchain: With VK_KHR_swapchain enabled, fill out a VkSwapchainCreateInfoKHR. Provide the surface, the desired image format (choose one of the surface’s supported formats, e.g., VK_FORMAT_B8G8R8A8_UNORM with sRGB color space), the swapchain image count (commonly 2 or 3), image usage flags (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT for rendering). Choose a present mode – for no v-sync and low latency use VK_PRESENT_MODE_MAILBOX_KHR or IMMEDIATE_KHR if available, or FIFO_KHR for guaranteed vsync. If using separate graphics and present queues, include their indices and set the image sharing mode to CONCURRENT (or use EXCLUSIVE and handle ownership transfer explicitly). Call vkCreateSwapchainKHR to create it. This gives you a set of swapchain images in GPU memory.



Retrieve Swapchain Images and Create Image Views: Use vkGetSwapchainImagesKHR to get the array of VkImage handles for the swapchain. For each image, create a VkImageView (with vkCreateImageView) to describe how the image will be used. Typically, you create them with the swapchain image format and VK_IMAGE_ASPECT_COLOR_BIT aspect. These image views will be used later in framebuffers as color attachments.


Create a Depth Buffer (if needed): If your renderer uses depth testing, pick a supported depth format (like VK_FORMAT_D32_SFLOAT or D24_UNORM_S8_UINT if stencil is needed). Create a VkImage with that format, extent equal to the window size, usage flags including DEPTH_STENCIL_ATTACHMENT. Allocate memory for it (Vulkan requires explicit memory allocation with vkAllocateMemory and vkBindImageMemory). Then create a VkImageView with aspect DEPTH|STENCIL for this depth image.


Create a Render Pass: A Vulkan render pass defines the attachments (color, depth) and how they are used and transitioned. Define a color attachment (format must match the swapchain image format) with load/store ops (e.g., clear at load, store at end for presenting) and initial/final layouts (e.g., INITIAL_LAYOUT_UNDEFINED to FINAL_LAYOUT_PRESENT_SRC_KHR for the swapchain image). Define a depth attachment if used (with load op clear, store op maybe dont-care, and appropriate layouts). Set up a subpass that uses these attachments (a single subpass for basic rendering). Create the render pass via vkCreateRenderPass. This encapsulates the rendering workflow for one frame.


Create Framebuffers: For each swapchain image view, create a VkFramebuffer that ties together that image view with the depth buffer view (if any) for the render pass. Each framebuffer represents the set of attachments for one rendered frame. Use vkCreateFramebuffer with the render pass, an array of attachments (swapchain image view and depth view), and the framebuffer size (width/height equal to the swapchain images).


Allocate Command Buffers: Create a command pool (vkCreateCommandPool) for the graphics queue (and possibly one for transfer if you will copy data). Allocate command buffers (vkAllocateCommandBuffers) from this pool. You may allocate one primary command buffer per frame in flight (e.g., 2-3 of them). These will be recorded each frame with drawing commands.


Set Up Synchronization Objects: Create at least one VkSemaphore for image acquisition (swapchain signal) and one for rendering completion (to signal before present). Also create one VkFence per frame for CPU-GPU sync (to ensure you don’t start a new frame while the previous one is still processing). Vulkan’s sync primitives differ from DirectX 12: semaphores are GPU-GPU sync (not used for CPU waits), and fences are binary GPU-CPU sync objects (reset after being signaled)​


GAMEDEV.NET


. Initialize them with vkCreateSemaphore and vkCreateFence (fences should be created in signaled state if you want your first vkWaitForFences to pass without waiting).


With these steps, the Vulkan setup is complete. You have a Vulkan device and queue, a swapchain with images and framebuffers, command buffers, and sync objects. Next, you will record drawing commands into the command buffers and present frames, which we will integrate into the rendering loop after configuring the pipelines and shaders.Note: Vulkan requires careful matching of resource lifetimes. All created objects (instance, device, swapchain, etc.) should be destroyed with corresponding vkDestroy* calls at shutdown. Also, remember that Vulkan does no implicit error checking—always check VkResult returns and use the validation layer during development to catch mistakes.


3. Refactoring the Renderer Architecture for Multiple APIs


Adapting an OpenGL engine to support both DirectX 12 and Vulkan will likely require introducing an abstraction in your rendering module. The key is to isolate API-specific code behind a common interface or set of classes, while maintaining high performance by not over-abstracting critical paths. All APIs share the same goal (rendering images from GPU data), but their concepts and objects differ in details. Design a clear separation where the engine can invoke rendering commands in a generic way, and the implementation for each API does the equivalent work.Choose an Abstraction Level: Decide at what level you want to abstract your rendering. For example, you might create an interface for high-level operations like Renderer::DrawMesh(mesh, material) or lower-level like GraphicsAPI::SetVertexBuffer and DrawIndexed. A higher-level abstraction (e.g., DrawModel) means each backend will have more complex code to implement that whole draw call, whereas a lower-level one requires mapping many smaller calls (which can be harder to align with how Vulkan/D3D12 expect state set up)​


REDDIT.COM


. A common approach is to lean toward a moderately high level – e.g., implement Draw calls or material binding per API – to minimize redundant code while still letting each API handle its unique requirements. For instance, you might have:


A base Renderer class with abstract methods like Initialize(), CreateBuffer(), CreateTexture(), CreateShaderPipeline(), DrawMesh(), etc.


Concrete classes D3D12Renderer and VulkanRenderer that implement those methods using D3D12 and Vulkan calls respectively.


The game or engine code calls only the base interface. At compile or runtime, you decide which backend is active (e.g., compile the engine separately for Windows vs Linux, or select via a command-line flag).


Avoiding Overhead: You can implement the abstraction without virtual calls in inner loops by resolving the function pointers once. For example, if using a single compiled binary for both (which might happen only on Windows if you optionally support Vulkan there too), you could set function pointers for pRenderer->DrawMesh = DrawMeshD3D12 or similar at init. If you have separate builds (Windows vs Linux), the abstraction can be compile-time (e.g., compile with USE_VULKAN or USE_D3D12 macros). The idea is to not pay a heavy cost per draw call for dynamic dispatch. Many games with multi-API support choose the backend at launch and then call directly into that backend’s functions.Resource Management Abstraction: Provide common representations for buffers, textures, and shaders, but realize they will be implemented differently. For instance, you might have an abstract GpuBuffer class with methods like Map(), Unmap(), UpdateData() and then have D3D12Buffer and VulkanBuffer subclasses. However, because D3D12 and Vulkan both require explicit memory management (unlike OpenGL which hid it), your abstraction should expose needed controls (like usage flags, memory visibility, etc.). It’s okay if the implementations are substantially different internally – the goal is the rest of your engine (scene management, etc.) doesn’t need to know the API specifics.Decouple from OpenGL State Assumptions: OpenGL’s API allowed changing global state (bind a texture, set a uniform) and it would affect all subsequent draws until changed. DirectX 12 and Vulkan instead use objects that encapsulate state (Pipeline State Objects, Descriptor Sets, etc.), which are configured upfront. To maintain performance and correctness:


Stop relying on global state in your engine logic. For example, instead of “bind shader, bind texture, draw, then change texture, draw” as you might in OpenGL, you will likely prepare a command list of all draws, each with its own state. The order of operations might remain, but you must ensure each drawcall in Vulkan/D3D12 explicitly binds the correct resources (nothing carries over implicitly as in OpenGL’s pipeline).


Batch state changes where possible. This was true in OpenGL (to reduce glBind calls), and remains true but manifests differently in Vulkan/D3D12. For instance, sort your draws by material/shader so that you can reuse pipeline state objects and descriptor sets without rebinding too often​


GPUOPEN.COM


​


GPUOPEN.COM


.


Multi-threading Considerations: One big advantage of explicit APIs is the ability to prepare command lists on multiple CPU threads. OpenGL was largely single-threaded (calls from multiple threads needed careful context management and usually serialized in the driver, providing little gain)​


COMMUNITY.ARM.COM


. With Vulkan and D3D12, you can record commands in parallel and then submit them to the GPU, which can drastically improve CPU-side performance for large scenes. To leverage this:


Structure your renderer to allow multiple threads (worker threads) to record rendering work. For example, you can partition your scene and let each thread record a secondary command buffer (Vulkan) or command list (D3D12) for its set of objects. Then combine them on the main thread (execute them on the main command buffer/queue). This requires that your abstraction and design of command recording is thread-safe and well-synchronized.


In DirectX 12, you can create multiple command allocators and command lists (one per thread) and use ExecuteCommandLists to submit them in one go. In Vulkan, you can use secondary command buffers to record subpasses in parallel, or record independent primary command buffers if using multiple queues. The architecture should allow building command buffers outside of the main thread easily (e.g., no reliance on global static states).


Compatibility of Data and Coordinates: There are a few coordinate system differences to handle when reusing an engine designed for OpenGL:


Depth Range: OpenGL’s default NDC depth range is [-1, +1], whereas DirectX 12 and Vulkan use [0, +1]. This means your projection matrices or depth manipulations need to be adjusted. The simplest fix is to modify the projection matrix generation to output depth in 0..1 range (e.g., switch to the DirectX convention for the projection matrix formula). If not, you will experience depth buffering issues (everything might appear at one depth). Many engines switch to 0-1 depth range as part of porting to Vulkan/Direct3D. Alternatively, Vulkan offers an extension (or in core 1.0 with a trick) to use a negative viewport height to flip Y and adjust depth to 0-1. In practice, updating the math for projection is straightforward and recommended.


Coordinate Origin (Y Axis): In Vulkan, the coordinate system for the screen by default has (0,0) at the top-left (like DirectX), whereas OpenGL’s viewport puts (0,0) at bottom-left. If your engine doesn’t explicitly handle this, you might find your image appearing upside-down or flipped vertically in Vulkan compared to OpenGL. There are a couple of ways to fix it: one is to invert the Y coordinate in your projection matrix (multiply the y-scale by -1) for Vulkan so that the content lines up. Another convenient Vulkan feature is that you can specify a negative height in the VkViewport struct to flip the rendering orientation​


HACKSOFLIFE.BLOGSPOT.COM


. For example, set viewport.height = -FramebufferHeight and viewport.y = FramebufferHeight to flip the viewport transform. This trick has Vulkan render with (0,0) at bottom-left, matching OpenGL convention, if that’s easier for your shader logic. Choose one approach and apply it so that your rendered scene isn’t inverted on one of the APIs.


Winding Order and Culling: By default, OpenGL considers counter-clockwise vertices as front-facing (CCW), whereas DirectX uses clockwise (CW) as front-facing. Vulkan allows you to specify the front face in the pipeline state (you can choose either). Make sure your rasterizer state (D3D12) or pipeline rasterization state (Vulkan) is configured to cull the intended faces. If you notice all faces culled, you might just need to flip the winding in the state or reverse your index order.


By refactoring with these abstractions and adjustments in mind, the core engine logic (like scene update, entity management, etc.) can remain mostly unchanged. You essentially provide two implementations of the rendering layer. This approach is commonly used in engines that support multiple APIs: multiple render backends are written to a common interface, with each taking advantage of its API’s strengths​


STACKOVERFLOW.COM


. It is extra work to maintain, but it gives flexibility and performance on each platform. Next, we’ll focus on implementing the specifics of each backend within this new structure.


4. Implementing the DirectX 12 Rendering Backend


With the D3D12 device and swapchain initialized (from section 1), focus on translating your rendering code to DirectX 12 calls. The major pieces are creating pipeline state objects and descriptor heaps for resources, recording command lists for each frame, and handling GPU synchronization. Key sub-tasks include: (a) translating shader code to HLSL and compiling pipeline state objects, (b) setting up descriptor heaps and root signatures for resource binding, (c) updating buffers and textures with Direct3D’s memory model, and (d) writing the rendering loop with command list recording and execution.a. Create Root Signature and Pipeline State Objects (PSOs): In DirectX 12, a Pipeline State Object encapsulates almost all GPU state for a draw call (shader bytecode, input layouts, blend, rasterizer and depth states, etc.). You cannot bind shader programs or change most state on the fly as in OpenGL; instead you pre-create PSOs for each combination of shaders and fixed state you need​


GPUOPEN.COM


. Start by defining a root signature – this is the D3D12 object that defines what resources (descriptor tables, constants) the shaders will access. For example, if your shader uses a constant buffer at register b0 and a texture at t0, you might create a root signature with two parameters: one descriptor table for a CBV at b0, and one descriptor table for an SRV at t0 (or you could put them in one table with ranges). Often, the root signature is created in serialized form by D3D utility (e.g., using D3D12SerializeRootSignature), then CreateRootSignature. Tip: Keep the root signature as small as possible (few parameters) – every root parameter costs GPU time to set​


GPUOPEN.COM


​


GPUOPEN.COM


. After that, for each shader or material, create a PSO via ID3D12Device::CreateGraphicsPipelineState. Provide a D3D12_GRAPHICS_PIPELINE_STATE_DESC with pointers to the compiled vertex and pixel shader bytecode (and others if used), the rasterizer/blend/depth configurations, the input layout (vertex format), and the root signature. This is analogous to linking a program in OpenGL, but it also locks in the fixed-function states. Expect PSO creation to be an expensive operation (several milliseconds or more)​


GPUOPEN.COM


, so do it at load time, not at runtime during rendering. You might generate multiple PSOs for variations of your rendering (lighting vs depth-only passes, etc.). Using PSO caching is recommended: D3D12 can give you a blob of compiled PSO data to save and load on next run to avoid shader recompilation overhead.b. Set Up Descriptor Heaps for Resources: Unlike OpenGL where you bind textures or buffers directly by ID, D3D12 requires you to create views (SRV – Shader Resource View, CBV – Constant Buffer View, UAV – Unordered Access View, etc.) into descriptor heaps and then bind those via the root signature. You already created an RTV heap for the framebuffer. Similarly, create a CBV/SRV descriptor heap large enough to hold all the resource views you’ll use in a frame (or use multiple heaps as needed). You might use one large heap as a shader-visible descriptor heap for all shader resources (textures, constant buffers) that you will bind, and another heap for samplers if needed (D3D12 separates sampler heaps). Populate the heap with descriptors for each resource at load time (e.g., when you create a texture, also create an SRV in the heap and store its handle). At draw time, you can then use SetGraphicsRootDescriptorTable to point the GPU to the correct descriptor in the heap for the resource. For dynamic resources that change per frame (like per-object constant buffers), you might use a small ring buffer in a heap or use root constants (which bypass the heap but limited in size). Platform-specific tip: In D3D12, placing data in the root signature (as root constants or root descriptors) is fast, but large arrays of resources should go in descriptor tables to avoid bloating the root signature​


GPUOPEN.COM


​


GPUOPEN.COM


.c. Buffer and Texture Updates (Memory Management): Porting from OpenGL’s memory model (where glBufferData or glTexImage implicitly allocate GPU memory) to D3D12 means you need to manage buffer memory. For vertex and index buffers: create a ID3D12Resource in the default heap (GPU local memory) with the appropriate size and D3D12_RESOURCE_FLAG_NONE. To initialize or update it, you cannot directly map default heap resources (unless they have UPLOAD heap type). Instead, do one of:


Use an upload heap resource: Create a resource in an upload heap (CPU-accessible) and memcpy your data into it, then use UpdateSubresources or schedule a copy via command list to transfer data into the default heap buffer.


Or create the buffer with D3D12_RESOURCE_FLAG_NONE in an upload heap (for dynamic data that changes always, like constants) and use it directly, knowing it resides in slower memory. For large static assets, you’ll use the copy method to move them to GPU local memory once. For textures, a similar pattern: you likely load image data into a CPU memory buffer, then create a default heap texture resource, then create an intermediate upload heap resource for the texture data, and use ID3D12GraphicsCommandList::CopyTextureRegion or UpdateSubresources to copy from upload to the GPU texture. Finish with a ResourceBarrier to transition the texture resource to PIXEL_SHADER_RESOURCE state for shader use. Because D3D12 doesn’t automatically handle layout transitions and memory timing, you must insert appropriate resource barrier calls when you use resources for different purposes (render target -> shader resource, etc.). Batch barriers together where possible for efficiency​


GPUOPEN.COM


.


d. Rendering Loop (Recording and Submitting Command Lists): With setup done, the render loop for DirectX 12 goes roughly:


Acquire Next Back Buffer Index: You can get the current back buffer index via IDXGISwapChain3::GetCurrentBackBufferIndex (for flip-model swapchains) or keep track by rotating an index each frame.


Reset Allocator and Command List: For the frame, reset the command allocator (ID3D12CommandAllocator::Reset) and then reset the command list (ID3D12GraphicsCommandList::Reset) with the allocator and an initial PSO (or null).


Set State and Write Commands: Set the necessary state on the command list:


Bind the root signature (SetGraphicsRootSignature).


Set descriptor heaps (SetDescriptorHeaps) for your shader resource and sampler heaps.


Set viewport and scissor (RSSetViewports, RSSetScissorRects).


Transition the back buffer to render target state with ResourceBarrier (from PRESENT to RENDER_TARGET).


Bind the back buffer RTV and depth buffer DSV (OMSetRenderTargets).


Clear the render target (and depth buffer) as needed using ClearRenderTargetView and ClearDepthStencilView.


For each object to draw: bind the appropriate pipeline state (SetPipelineState) and descriptor tables or root constants for its resources (SetGraphicsRootDescriptorTable etc.), bind the vertex/index buffers (IASetVertexBuffers, IASetIndexBuffer), set the primitive topology (IASetPrimitiveTopology), and issue the draw call (DrawInstanced or DrawIndexedInstanced). This is where the higher-level DrawMesh() in your abstraction translates to these low-level bindings and a draw.


After drawing all objects, transition the back buffer image to PRESENT state using another ResourceBarrier. (Proper state transitions ensure the GPU and display can use the image without conflict.)


Close and Execute Command List: Once recording is done, call Close() on the command list. Then execute it by calling ID3D12CommandQueue::ExecuteCommandLists with the list.


Present the Frame: Call IDXGISwapChain::Present to display the back buffer. If using vsync (or the only mode available is vsync FIFO), this will sync to the refresh.


Signal Fence for Frame Completion: Immediately after presenting (or before, depending on desired latency), signal the ID3D12Fence on the command queue with a new fence value (e.g., increment a frame counter)​


GAMEDEV.NET


. This tags the commands up to this point with an ID.


CPU Synchronization: Wait for the fence value from two frames ago (if using a frame buffer count of 2) or the oldest frame in flight, to ensure those resources (like command allocator or back buffer) are free to reuse. You can use WaitForSingleObject on the fence’s event after calling SetEventOnCompletion(fenceValue), or use Fence->GetCompletedValue in a loop.


Loop: Move to the next frame’s index, and repeat the reset, record, execute, present, sync sequence.


Performance considerations for D3D12: Keep as much as possible on the GPU timeline. For example, allow 2-3 frames in flight to cover CPU-GPU latency (but beware of latency impact). Use multiple command lists if parallel recording yields benefit (submit them together to the queue). Avoid performing resource creation or shader compilation during this loop – those should be done in loading screens or background threads, then integrated via prepared PSOs. Also, make liberal use of D3D12 debug layer and tools like PIX (Performance Investigator for Xbox/Windows) to debug and optimize commands. DirectX 12’s API won’t warn you of improper usage beyond critical failures, so validation tools are your friend during development.


5. Implementing the Vulkan Rendering Backend


Implementing the Vulkan backend will mirror the structure of the DirectX 12 backend in many ways, but requires handling Vulkan-specific conventions. We will use the objects created in section 2 (device, swapchain, etc.) to record command buffers for each frame and submit them. Key tasks include: (a) creating Vulkan pipeline state (VkPipeline and VkPipelineLayout, plus descriptor sets for resources), (b) updating buffers and images via explicit memory transfers, and (c) writing the render loop with command buffer submission and synchronization.a. Create Descriptor Set Layouts and Pipeline Layout: In Vulkan, resource bindings are described by descriptor sets. Start by defining VkDescriptorSetLayout(s) to match your shader resource usage. For example, if your shader expects a uniform buffer object (UBO) and an image sampler, you create a descriptor set layout with two bindings (one for UBO, one for combined image sampler) at binding 0 and 1 respectively. Then create a pipeline layout (VkPipelineLayout) by combining one or more descriptor set layouts and pushing constant ranges (if you use push constants). This pipeline layout is analogous to D3D12’s root signature, defining what types of resources the pipeline can access​


ALAIN.XYZ


​


ALAIN.XYZ


.Create Shader Modules: Take your shader code (likely written in GLSL or HLSL cross-compiled to SPIR-V) and create VkShaderModule objects using vkCreateShaderModule. You need one module for each stage (vertex, fragment, etc.). If you have GLSL from OpenGL, you can compile it to SPIR-V with glslang or shaderc. If you opted for HLSL as a common language, use DXC to compile HLSL to SPIR-V​


STACKOVERFLOW.COM


. Ensure the shader’s binding layout matches the descriptor set layouts you created. (E.g., GLSL layout(set=0, binding=0) uniform Foo corresponds to set 0 binding 0 in your descriptor layout. HLSL’s registers can be mapped similarly by using register(b0, space0) for set=0 binding=0 in Vulkan.)Create the Graphics Pipeline: Fill out a VkGraphicsPipelineCreateInfo. This includes many sub-structures:


Shader Stages: An array of VkPipelineShaderStageCreateInfo for each stage (vertex, fragment, etc.), pointing to the shader modules and entry point names (usually "main").


Fixed-function states: Vulkan requires specifying vertex input descriptions (even if using vertex buffers only, you define VkPipelineVertexInputStateCreateInfo with binding stride and attribute formats similar to an OpenGL VAO setup). Also specify the input assembly state (primitive topology, e.g., triangle list), rasterization state (fill mode, cull mode, front face winding), multisample state, depth/stencil state (enable depth test/write, compare op, etc.), and color blend state (blend modes per attachment).


Render Pass: Provide the VkRenderPass that this pipeline will be used with, and a subpass index (usually 0 if you have one subpass). This ties the pipeline to the format of the render targets.


Pipeline Layout: Attach the VkPipelineLayout created earlier, which tells the pipeline what resource interfaces to expect.


Dynamic State: Optionally, you can mark certain states as dynamic (to be set via commands at draw time). Common dynamic states are viewport and scissor, which you then set with vkCmdSetViewport/vkCmdSetScissor instead of baking into the pipeline.


Create the pipeline with vkCreateGraphicsPipelines. Like D3D12 PSO creation, this can be slow, so do it at initialization or offline. Also consider using a pipeline cache: initialize a VkPipelineCache (possibly loaded from disk) and pass it to pipeline creation to speed up and later serialize the cache to disk for future runs​


GAMEDEV.STACKEXCHANGE.COM


.b. Allocate Descriptor Sets for Resources: Vulkan requires you to allocate descriptor sets from a descriptor pool. Create a VkDescriptorPool with pool sizes sufficient for the max descriptors needed (e.g., X uniform buffers, Y combined image samplers, etc., across all frames). Then allocate one or more VkDescriptorSet from it, based on the descriptor set layouts. You might allocate one descriptor set per “shader binding scope” (for example, per material or per frame). A simple strategy: allocate a descriptor set for each frame that holds all per-frame resources (like the camera UBO and some common textures), and another descriptor set per material for material-specific textures/parameters. Update the descriptor sets with vkUpdateDescriptorSets: specify the buffer info for UBOs (with the VkBuffer and offset/size) and image info for textures (with VkImageView and VkSampler). In Vulkan, updating descriptors is akin to glUniform/glBindTexture in OpenGL, but done upfront once or when resources change, rather than every draw. Performance tip: Try to minimize how often you update descriptor sets each frame; if you can reuse descriptor sets for multiple objects (with the same material, for instance), do so​


GPUOPEN.COM


. Also, group frequently used resources in the same set to avoid binding multiple sets.Memory Transfers for Buffers/Textures: For vertex and index buffers in Vulkan, you’ll create VkBuffer objects. Allocate device memory with vkAllocateMemory (find a memory type that is DEVICE_LOCAL for optimal GPU access; you may need a staging buffer in HOST_VISIBLE memory to upload data). Typically:


Create a VkBuffer with usage VERTEX_BUFFER (and possibly TRANSFER_DST if you will copy data in).


Allocate memory for it. If the memory type for device local is not host visible, allocate a second buffer in host visible memory as a staging buffer.


Use vkMapMemory on the staging buffer, copy vertex data into it, then vkUnmapMemory.


Record a vkCmdCopyBuffer from staging to the GPU buffer (on a command buffer).


Submit that on a queue (graphics or transfer) and use a fence or wait idle to ensure completion.


Then free the staging buffer and its memory. This explicit workflow replaces glBufferData. You can simplify this by using the coherent memory type (memory visible to host and device) for dynamic data, at the cost of slower access. For textures:


Create a VkImage with usage SAMPLED (and TRANSFER_DST).


Allocate device-local memory for it.


Create a staging buffer and copy image data into it (row-aligned, etc.).


Use vkCmdCopyBufferToImage to transfer the data to the image.


Use vkCmdPipelineBarrier to transition the image layout from transfer-dst to shader-read (layout transitions are explicit in Vulkan, unlike OpenGL). E.g., from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, with pipeline stage sync from transfer-write to fragment-shader-read.


c. Vulkan Rendering Loop: With pipelines and descriptor sets ready, render each frame as follows:


Wait for available Frame: Use vkWaitForFences to wait until the previous frame’s GPU work is done (just like waiting on a D3D12 fence). Then vkResetFences to reuse the fence.


Acquire Swapchain Image: Call vkAcquireNextImageKHR with the swapchain, a semaphore (let’s call it imageAvailableSemaphore) and an optional fence (we often use semaphore for this). This gives an index of the swapchain image to render to. If the swapchain is out of date (window resized, etc.), you’ll need to recreate it, but that’s outside this core loop.


Record Command Buffer: Begin your command buffer (vkBeginCommandBuffer). Begin the render pass with vkCmdBeginRenderPass, specifying the render pass, the framebuffer for the acquired image, and clear values for attachments. Once inside the render pass, bind the graphics pipeline (vkCmdBindPipeline). Set the viewport and scissor (vkCmdSetViewport/Scissor) if they are dynamic. Bind descriptor sets with vkCmdBindDescriptorSets – you’ll provide the pipeline layout, the descriptor set(s) (frame descriptor set, material/texture descriptor set, etc.) and an array of dynamic offsets if you use dynamic UBOs. Then for each object:


Bind its vertex buffer (vkCmdBindVertexBuffers) and index buffer (vkCmdBindIndexBuffer if indexed).


Push any constants if using push constant for model matrices, via vkCmdPushConstants (this updates a small shader parameter directly, avoiding a descriptor).


Call vkCmdDrawIndexed (or vkCmdDraw). These correspond to drawing the object with the bound pipeline and descriptor resources. After drawing all, end the render pass with vkCmdEndRenderPass. Then end the command buffer (vkEndCommandBuffer).


Submit to Queue: Prepare a VkSubmitInfo for the graphics queue. It will specify:


Wait semaphore: the imageAvailableSemaphore (from acquire) with stage COLOR_ATTACHMENT_OUTPUT (to wait until the image is ready before starting color output).


Command buffer to execute: the one we just recorded.


Signal semaphore: a renderFinishedSemaphore that will signal when command buffer finishes (for the present step). Call vkQueueSubmit on the graphics queue with this info and a fence (one per frame) to signal when done.


Present the Image: Use vkQueuePresentKHR on the present queue (which might be the same as graphics queue). Pass the renderFinishedSemaphore in the wait list so that presentation waits until rendering is done. If vsync is enabled via present mode, this call will queue the image for display (and may block internally until the next refresh, or return and block on the semaphore).


Loop: Move on to next frame (and next command buffer, semaphore, and fence in a round-robin fashion for 2-3 frames in flight).


Throughout this loop, handle Vulkan’s results: e.g., if vkAcquireNextImageKHR returns SUBOPTIMAL or ERROR_OUT_OF_DATE_KHR, the swapchain needs to be recreated (window resize or alt-tab occurred). This involves a careful teardown and re-creation of swapchain images, framebuffers, etc., which should be handled in your Vulkan backend (usually by checking the return and triggering a resize handler that waits for GPU idle, destroys swapchain, creates a new one with new size, and updates framebuffers).Debugging and Validation: Use VK_LAYER_KHRONOS_validation during development. It will catch incorrect usage such as missing barriers, descriptor set mismatches, etc., that would otherwise crash or render incorrectly. Tools like RenderDoc are extremely useful for Vulkan to inspect frames and ensure that your pipeline and descriptor sets are correctly set.Note on Similarities: If you look at the above loops for D3D12 and Vulkan, they are conceptually similar: both explicitly manage a pool of command buffers, both use semaphores/fences for sync (though with different APIs), and both require you to set all state for each draw (since there is no hidden global state). This explicit model is why the performance is more predictable and often better – but it comes at the cost of verbose code and more responsibility on the engine side. The next sections will cover how to optimize each backend and handle advanced rendering techniques.


6. Shader Compilation and Asset Pipeline Adjustments


Supporting two different rendering APIs often means dealing with two shading languages and potentially different asset formats for GPU resources. To maintain a streamlined workflow, it’s best to unify your shader source code or generation as much as possible, and adjust your asset pipeline to produce API-specific outputs (like compiled shaders or texture formats) as needed.Shader Language and Cross-Compilation: OpenGL shaders are written in GLSL, DirectX 12 expects HLSL bytecode (DXIL), and Vulkan accepts SPIR-V binary (which can be compiled from GLSL or HLSL). Maintaining completely separate shader code for HLSL and GLSL can double your work, so consider using one language and cross-compile:


One approach is to use HLSL for all shaders. You can compile HLSL to DXIL for DirectX 12 and also compile the same HLSL to SPIR-V for Vulkan using Microsoft’s DXC compiler​


STACKOVERFLOW.COM


. DXC has a flag (-spirv) that enables SPIR-V code generation, allowing HLSL to serve as a single source. You will need to follow Vulkan’s rules in the HLSL (for example, ensure you use [[vk::binding(set, binding)]] semantics or space registers to separate descriptor sets, and be mindful of supported features).


Another approach is to use GLSL or a neutral high-level language and compile to both SPIR-V and DXIL. There are tools like SPIRV-Cross that can translate SPIR-V to HLSL, or you could write GLSL and use glslang to get SPIR-V, then use a separate HLSL for DirectX if translation isn’t perfect. However, many developers find HLSL-to-SPIRV via DXC more mature at this point, making HLSL a good “one source” solution.


Whichever route, automate your shader compilation as part of the build or asset pipeline. For example, have a script or build step that takes each .hlsl file and runs DXC twice – once for DirectX (targeting e.g. vs_6_0, ps_6_0) and once for Vulkan (targeting SPIR-V 1.3). The output could be *.cso DXIL files and *.spv SPIR-V files. At runtime, your engine can load the appropriate one based on the backend in use.


Shader Reflection and Resource Binding: With two APIs, you need to reconcile how shader resources (uniforms, textures) are bound:


In HLSL for DirectX, resources are referenced by register slots (b0, t0, s0, etc. for CBVs, SRVs, Samplers). In Vulkan (GLSL or HLSL compiled to SPIR-V), they are by descriptor set and binding number. You should decide on a consistent scheme so that the same shader code can be used in both. For example, you might designate: register b0 in HLSL is set=0,binding=0 in Vulkan (for a per-frame uniform buffer), t0 register is set=0,binding=1 (for a base color texture), etc. By using space in HLSL registers, you can emulate Vulkan’s sets (e.g., register(b0, space1) could mean set 1 binding 0). DXC’s SPIR-V backend will translate HLSL register semantics to SPIR-V binding decorations​


STACKOVERFLOW.COM


, so you can line up the two. It’s a good idea to document or centralize this mapping to avoid confusion.


If you have existing GLSL code from OpenGL, note that in OpenGL you might not have used explicit binding locations (you might rely on the program querying uniform names). In Vulkan, you must have bindings declared or specified via VkDescriptorSetLayout. So you may need to modify your GLSL with layout(binding=X, set=Y) annotations or use reflection to assign them. It’s worth the effort to explicitly define all shader resource bindings in the code for clarity.


Update your engine’s material or shader loading logic: instead of glGetUniformLocation and glUniform*, you will now create descriptor sets (as done in sections above) and update them with buffer data. This is more involved but also more efficient for many updates (you can update many descriptors in one call).


Asset Formats (Textures, Models): Most model data (vertices, indices) can be used as-is across APIs. Just ensure the vertex attribute definitions in your Vulkan pipeline and D3D12 input layout match the data layout coming from your models. If you used any OpenGL-specific formats (like maybe you assumed GL_UNSIGNED_SHORT for indices – which is fine since both D3D12 and Vulkan support 16-bit indices), just ensure consistency.For textures:


If you used DDS files or other DirectX-friendly formats, D3D12 can use those directly (BCn compressed formats, etc.), and Vulkan also supports most of those block-compressed formats (BC1-BC7) under different names (called ASTC or BC in Vulkan depending on GPU support). You might not need to change the format of your textures, but be aware of swizzling differences (GL’s notion of internal format vs Vulkan’s VkFormat). For example, if you had an OpenGL texture in SRGB, in Vulkan you must use a corresponding SRGB format for the image and the image view (e.g., VK_FORMAT_R8G8B8A8_SRGB).


Mipmaps: With OpenGL you might call glGenerateMipmap. In Vulkan, you’d either generate mipmaps in a compute shader or when loading (offline), or use vkCmdBlitImage with SRC_OPTIMAL/ DST_OPTIMAL layouts in a loop to generate them. It’s simplest to generate mipmaps offline when building assets to avoid runtime cost.


sRGB Sampling: Both APIs support sRGB formats, just ensure your pipeline state in D3D12 is set to do gamma-correct blending if needed and in Vulkan, the image format and framebuffer can be SRGB so that it converts automatically.


Content Pipeline Changes: Update your content pipeline to export multiple shader binaries (as mentioned) and any API-specific metadata. For example, MonoGame noted that supporting new APIs required updating the content pipeline for new shader formats​


GITHUB.COM


. In practice, that means if you had a step that took GLSL and produced an OpenGL program binary or so, you now add steps to produce SPIR-V and DXIL. You might ship separate bundles of shader binaries for each platform or one unified package containing both. If your engine can load shaders at runtime, it could select the appropriate blob.Also consider pre-compiling PSOs on DirectX 12: D3D allows you to serialize PSOs to a blob after creation (or use the newer Pipeline Library API to store compiled shader code). Similarly, you can save Vulkan pipeline cache data with vkGetPipelineCacheData. These can be stored as part of assets to avoid long load times on first run. Just be cautious to handle if the cache is invalid (e.g., new driver version might invalidate pipeline cache, so you need to fall back to clean compile if so).Material and Effect System: If your engine had a high-level concept of a material or effect that in OpenGL would set some uniforms and bind a shader program, now you need to split that logic:


For D3D12: the material might correspond to a PSO (or a combination of a PSO and descriptor data). Possibly group materials by PSO and just vary descriptor tables for textures.


For Vulkan: a material could correspond to a pipeline + descriptor set. It may be beneficial to create a small system to manage these. For example, a Shader Manager that given a material name will return a ready PSO or pipeline and a Descriptor Manager that provides the descriptor sets with the correct resources. This way, the higher-level code can just say “activate material X” and under the hood the engine binds the appropriate pipeline and sets.


Handling GLSL features not in Vulkan: If you had advanced GLSL features (geometry shaders, tessellation) – both D3D12 and Vulkan support those (Vulkan via extensions or core in later versions). If you used older GLSL tricks or built-in variables (like gl_FragCoord, etc.), you have their equivalents (Vulkan uses builtin variables too, just ensure your SPIR-V is generated correctly).Testing Shaders: It’s highly recommended to test your shaders on both backends early. The same shader might produce different results if you made assumptions that differ (like if you forget the coordinate adjustment differences we handled above). RenderDoc is useful for Vulkan, and the D3D12 Debug Layer and PIX can help diagnose issues on Windows.By adjusting the shader and asset pipeline, you ensure that your engine can feed both rendering backends with the data they need in the format they expect. This unification reduces duplicate effort and potential inconsistencies. Next, we focus on platform-specific performance optimizations to ensure we maintain (or exceed) the performance of the original OpenGL version.


7. Platform-Specific Optimizations for DirectX 12 and Vulkan


To achieve high performance and utilize advanced features on each API, you should tailor some parts of your renderer to the strengths and quirks of DirectX 12 and Vulkan. Both APIs give low-level control, but the optimal usage patterns can differ. Here we outline optimizations for each:


7.1 DirectX 12 Optimizations


Command List Multithreading: Leverage multiple threads to build command lists in parallel. For example, you can have worker threads record ID3D12GraphicsCommandList on their own ID3D12CommandAllocator for different parts of the scene, then execute them all on the main command queue. This can drastically reduce CPU time per frame if you have many draw calls. Microsoft’s design allows each command list to be recorded without internal locks, so scaling across cores is easier than in OpenGL.


Descriptor Heap Management: Use large descriptor heaps and sub-allocate from them to reduce calls to create descriptor heaps or handle lots of small descriptor tables. For instance, create one big shader-visible heap for all textures used in a level. You can then just change the descriptor table pointer (root descriptor table) to point at the offset of the texture you need for a draw. This is effectively manual bindless resource management. Keep frequently used descriptors contiguous in the heap to maximize GPU cache hits​


GPUOPEN.COM


. For dynamic resources like per-frame constant buffers, consider using a ring buffer allocator in an upload heap for constant data and a corresponding section of the descriptor heap that you update each frame (you can copy a block of descriptors with CopyDescriptorsSimple which is efficient​


GPUOPEN.COM


).


Minimize Root Signature Size: As mentioned, every root parameter has a cost. Try to keep root signature parameters to under 4 or 5 if possible​


GPUOPEN.COM


. Use descriptor tables for bulk resources (they cost 1 DWORD on the GPU to set, regardless of number of descriptors)​


GPUOPEN.COM


. Only use root constants or root descriptors for data that changes every single draw (e.g., a transformation matrix) to avoid the overhead of descriptor heap usage for those and because they are quick to set​


GPUOPEN.COM


. In other words, put per-draw constants in root constants, but textures and per-material buffers in descriptor tables.


Pipeline State Object (PSO) Management: Group draw calls by PSO to reduce expensive pipeline switches​


GPUOPEN.COM


. While switching a PSO in D3D12 is not as costly as changing a dozen states in OpenGL, it’s still not free – especially if the new PSO has different shader or topology that the GPU pipeline needs to flush. Sorting by PSO (or material/shader) will help. Also, make good use of PSO caching as described: D3D12 can create Pipeline Libraries (if you use the DXIL library mechanism) which allow you to compile shaders once and reuse partial PSOs quickly​


GPUOPEN.COM


. This can be advanced, but at minimum, store the compiled shader bytecode and reuse it for multiple PSOs (e.g., if you have the same vertex shader across multiple materials, compile it once and use the bytecode in multiple PSO creates).


Use Async Compute for Parallelism: If your application has heavy compute work (e.g., GPU particle simulation, post-processing effects, AI on GPU, etc.), consider using a Compute queue. D3D12 allows multiple command queues (Direct/Graphics, Compute, Copy) and if your GPU supports concurrency, a Compute queue can run in parallel with the Graphics queue. For example, shadow map rendering and scene rendering might be parallelized if one is on compute (using a compute shader for culling or lighting) and the other on graphics, or post-processing tone-map done on compute overlapping with graphics queue doing next frame. It requires careful synchronization with fences between queues. Use ID3D12CommandQueue::Signal and Wait to coordinate (you can treat a fence value as a cross-queue semaphore by signaling on one queue and waiting on another). Async compute is particularly useful for global illumination or post-processing effects that can be offloaded while the main rendering is in progress.


Tweak Memory Allocation Strategy: D3D12 gives you control of resource allocation. Using committed resources (the straightforward path we used above) is simple but can be less efficient for many small resources. Advanced optimization: allocate large ID3D12Heap objects (heaps of GPU memory) and then create placed resources inside them. This way, you manage memory similarly to Vulkan (which always does this). Libraries like DirectX Memory Allocator (by Microsoft) can help manage this. It’s an optimization to consider if you profile and find heap allocation overhead or want more control over memory residency.


Frame Persistence and Descriptor Reuse: If certain descriptor sets or tables remain the same for many draws (e.g., a table of global textures), set it once at the start of a frame and don’t change it. D3D12 allows setting descriptor tables that remain valid until changed. This saves repeated setting overhead. Similarly, reuse command allocators in a ring (we covered that) and avoid recreating command lists every frame – reset and record anew.


Use Modern DX12 Features if Available: For example, Variable Rate Shading (Tier 1 and 2) can be used to reduce pixel shader workload in low-detail areas (if targeting newer hardware). Mesh Shaders (available via DX12 Ultimate) could replace parts of your pipeline for GPU-driven culling and LOD if you choose to go that far (advanced topic beyond classic OpenGL parity). DirectStorage (if loading lots of texture data) could improve streaming. These are optional and depend on how cutting-edge you want to go – they’re not necessary for initial porting but worth noting.


Profiling on Windows: Use tools like PIX which can capture GPU and CPU timelines for D3D12, so you can see if your GPU is idle, or if you have sync issues, or if certain draws are slow. Optimize based on bottlenecks – e.g., if you’re CPU bound building command lists, then multithreading is the answer; if you’re GPU bound on shading, maybe optimize shaders or use better culling.


7.2 Vulkan Optimizations


Command Buffer Reuse and Multithreading: Similar to D3D12, record command buffers on multiple threads if the scene is large. Vulkan’s model encourages secondary command buffers: you can have worker threads each record a secondary command buffer for a portion of the scene (using VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT for one-off usage to allow driver to optimize​


GPUOPEN.COM


), then in the main thread, vkCmdExecuteCommands to execute those secondaries in the primary command buffer. This avoids thread contention on a single command buffer. If your scene is simple, one thread might suffice, but be ready to scale out for complex scenes.


Avoid Synchronized Command Buffer Reuse: By default, Vulkan command buffers cannot be in-flight more than once unless you explicitly allow simultaneous use (which adds overhead)​


GPUOPEN.COM


. A common pattern is to use a pool of command buffers equal to the number of frames in flight and record new commands every frame. Resetting command buffers is cheap when using vkResetCommandPool between uses (this frees all buffers for reuse). Design your engine such that you don’t wait on GPU mid-frame to reuse a command buffer – instead, allocate enough that you can have, say, 3 frames worth and reuse one only after its frame has finished (like we did with fences).


Descriptor Set Management: In Vulkan, frequent allocation and freeing of descriptor sets can be costly. Prefer to allocate descriptor sets once (or pool them) and update them in place if content changes. If you have truly dynamic data that varies per draw (like per-object UBOs), one technique is to use dynamic descriptors (descriptor type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC), which allow you to bind a single descriptor for a whole array of object transforms but provide an offset at draw time for the particular object. Then you only need one descriptor set for all objects’ matrices, and you vary the offset via vkCmdBindDescriptorSets with dynamic offset. This avoids creating one descriptor per object. It’s similar to D3D12’s concept of a constant buffer view heap where you choose an offset for each object. However, note that dynamic descriptors have a slight higher cost per use (it adds an offset addition on GPU)​


GPUOPEN.COM


. For many cases, allocating say 1000 descriptor sets for 1000 objects is fine if done up front, but updating 1000 of them per frame is not – so use dynamic offsets or push constants for small bits of data.


Pipeline Layout and Descriptor Set Binding Strategy: Try to keep the number of descriptor set bindings minimal in the draw loop. If you can put most frequently used resources in one set (set 0), and per-material in set 1, then your draw loop might only bind a new set 1 when material changes, and only bind set 0 once at start of frame​


GPUOPEN.COM


. Group draw calls by material to avoid rebinding sets every object. The idea is similar to D3D12: reduce “bind churn”. If you find yourself binding many different descriptor sets each draw, consider consolidating them or using arrays of descriptors (bindless-like techniques via the VK_EXT_descriptor_indexing extension, which allows shaders to index into large descriptor arrays).


Use Pipeline Barriers Wisely: Vulkan gives you fine control over memory barriers (via vkCmdPipelineBarrier). Too many barriers or incorrect ones can stall the GPU. General tips: batch multiple resource transitions in one vkCmdPipelineBarrier call if possible​


GPUOPEN.COM


, and only use pipeline stages that are necessary (don’t conservatively use top-of-pipe or bottom-of-pipe for everything; use specific stages like fragment shader, transfer, etc., to allow maximum parallelism). Also, avoid transitioning layouts back and forth unnecessarily. For example, you might leave a G-buffer texture in SHADER_READ_ONLY_OPTIMAL after using it, if you will sample from it next frame, rather than transitioning to GENERAL and back. The fewer barriers per frame, the better, as they incur driver overhead and GPU sync cost.


Memory Allocation: Managing VkDeviceMemory can be complex. Use the Vulkan Memory Allocator (VMA) library or a similar custom allocator to group resources. Allocating one big chunk and suballocating (like for many buffers) reduces driver calls. Also prefer using host-coherent memory for dynamic data so you don’t need to call vkFlushMappedMemoryRanges (coherent memory auto flushes writes but might have lower performance). For static geometry, allocate in device-local and stage it, as we described. On some platforms (integrated GPUs), device-local might also be host-visible (e.g., on an AMD APU or Intel iGPU, memory is unified)​


GITHUB.COM


, so you can directly access it – you can detect this and simplify without copies in those cases.


Swapchain Image Count and Present Mode: If low latency is a goal, use VK_PRESENT_MODE_MAILBOX_KHR with 3 images if available – it allows GPU to render ahead and always have an image ready, reducing stutter. If power or vsync is a concern, FIFO_KHR is always available but adds latency. Tweak the image count: having 3 images can allow the GPU to not stall if one image is being displayed while you render to another and maybe a third is queued. This is like triple buffering – higher throughput at cost of latency. If you want to strictly match OpenGL’s behavior, ensure vsync is configured similarly.


Asynchronous Compute: Vulkan also supports multiple queues. If your GPU has a separate compute queue, you can offload computation similar to D3D12. The synchronization is done via semaphores and fences (timeline semaphores in Vulkan 1.2+ can simplify it by using a counter like D3D12’s fence approach). Example: you could do post-processing on a compute queue while the graphics queue starts on the next frame’s drawing; when compute is done, a semaphore signals and the graphics queue can use the result. This is advanced, but the performance gain can be worth it for heavy full-screen effects or GPU particle systems.


Descriptor Indexing (Bindless): If targeting modern Vulkan (and GPUs that support Descriptor Indexing extension), you can create very large descriptor arrays (like bindless textures) and index them in shaders by an ID, rather than binding each individually. This way, you bind once a descriptor set containing all your textures, and each draw just passes an index to the shader to pick the right texture. This approach mimics OpenGL’s bindless or DX12’s descriptor tables with offsets. It can reduce CPU overhead greatly when you have many small resources (like thousands of textures) at the cost of some complexity in setup and potential memory usage. If your use-case (like a scene with many material textures) demands it, consider this to push performance further.


Profiling on Vulkan: Use RenderDoc for debugging and basic GPU timing. For more detailed GPU performance, use vendor-specific tools: e.g., NVIDIA Nsight Graphics, AMD Radeon GPU Profiler (RGP). AMD RGP can give detailed timeline of Vulkan events and how the GPU is occupied​


DSOGAMING.COM


, which is great for identifying bubbles in the pipeline or inefficient synchronization.


Finally, keep in mind that some optimizations are universal (e.g., frustum culling to reduce draw calls, LODs to reduce vertex count, etc.) – those engine-level optimizations remain important and apply to both APIs.


8. Advanced Rendering Techniques – Lighting, Shadows, and Global Illumination


High-end rendering features such as complex lighting models, dynamic shadows, global illumination (GI), and volumetric effects can greatly benefit from the explicit control and performance of DirectX 12 and Vulkan. Transitioning them from OpenGL requires both adapting to new APIs and potentially updating algorithms to better suit modern GPU pipelines. Below are considerations and steps for each major feature:Dynamic Shadows: If your OpenGL renderer uses shadow mapping (common technique), you’ll implement it similarly but can optimize it more:


Rendering Shadow Maps: In both D3D12 and Vulkan, rendering a shadow map is just another render pass or command list. You might create a separate PSO/Pipeline for depth-only rendering of casters (use a simplified vertex shader and no pixel shader if using depth-only output). Switch to that pipeline and render from the light’s viewpoint to populate the shadow texture. On Vulkan, you can use a second subpass or a separate render pass for the shadow map; on D3D12, likely a separate command list or at least separate section with a different viewport and render target (the shadow depth buffer).


Resource transitions: After rendering the shadow map, transition it to a shader-readable state. In Vulkan, this is done with a pipeline barrier; in D3D12, a ResourceBarrier from DEPTH_WRITE to PIXEL_SHADER_RESOURCE. Then in the main rendering pass, bind the shadow map as a texture (descriptor).


Cascaded Shadow Maps: If you have cascaded shadow maps (CSM) for large scenes, you’ll generate multiple shadow textures (for different depth ranges). You can exploit multithreading by rendering different cascades in parallel if you have multiple command buffers and if the GPU has capacity. For example, on a CPU with 4 threads, record 4 command lists for 4 cascades simultaneously. On GPU, they might still execute serially on one queue unless you use multiple queues, but you at least prepare them faster. Another optimization: use the geometry shader or instanced draw to render all cascades in one pass (layered rendering) if supported, but this is advanced and not universally available.


Filtering: Soft shadow techniques like PCF or PCSS (percentage-closer soft shadows) will work as in OpenGL but you may utilize new features: e.g., Vulkan can use subpass inputs if you had a scenario of reading depth in a deferred renderer, or you can use specialized hardware filtering (some GPUs support “Depth Compare” sampler that does PCF in hardware).


Fences between passes: If shadow map generation is done on the same command list before main pass, no need for GPU sync beyond barriers. If you decide to schedule shadow pass on a compute queue (some do this by computing shadow maps in a compute shader for variance shadow maps, etc.), you’d need a semaphore to synchronize with the graphics pass.


Advanced Lighting (PBR, Multiple Lights): Modern engines often use physically-based rendering (PBR) with many lights. In Vulkan/D3D12, you have the ability to use GPU-driven techniques to handle many lights:


If you had forward rendering with many lights in OpenGL, you might have been limited by uniform array sizes or frequent state changes. With Vulkan/D3D12, consider using storage buffers (aka shader storage buffer objects in GL) or structured buffers in HLSL to pass an array of light data to the GPU. There’s virtually no hard limit on size (beyond memory) for these, unlike some older GLSL limits. This allows handling hundreds or thousands of lights more easily. You can then use compute shaders for culling lights per tile (tiled forward rendering) or use a technique like clustered shading.


Compute for Lighting: Vulkan and D3D12 both let you dispatch compute shaders freely. You could implement tiled lighting (compute shader divides screen into tiles, culls lights per tile, outputs light indices to a buffer, then your pixel shader reads that). This can drastically improve performance with many lights compared to naive per-pixel loops.


Bindless Light Lists: Using descriptor indexing (in Vulkan) or heap indexing (D3D12), you can put light shadow maps or other resources in arrays and index them by light ID in the shader, instead of binding each. This way, even if 100 lights cast shadows, you can sample the correct shadow map by index without 100 separate descriptors bound.


Global Illumination (GI): Real-time global illumination is challenging, but Vulkan and D3D12 open up new possibilities:


Screen-Space GI: Techniques like screen-space diffuse global illumination (SSDGI or SSRTGI) use ray-marching in screen space (similar to SSAO or reflections) to approximate indirect light. These algorithms can be implemented in compute shaders for efficiency. For example, you can use a compute shader to ray-cast from each pixel’s position into a screen-space depth buffer to gather light. D3D12 and Vulkan handle this easily – you create a compute PSO/pipeline and dispatch. They allow more threads and better control than GL’s compute in some cases.


Voxel Cone Tracing: This is a technique where the scene is voxelized, and cone-tracing (approximate ray tracing) is performed to gather multi-bounce lighting. Engines like CryEngine adopted a form of this for GI​


DSOGAMING.COM


. With Vulkan/D3D12, you can implement voxelization using geometry shaders or compute to build a 3D voxel texture (or an octree structure in memory), then have a compute shader cast cones (multiple sample rays) through this structure to accumulate indirect light. The explicit control over memory and GPU sync helps – e.g., you voxelize (compute job), use a barrier, then run cone tracing (compute or pixel shader), then use result in lighting. CryEngine’s technique, SVOGI (sparse voxel octree GI), is complex but they mention running an async job to voxelize and then a lighting pass to trace, providing real GI on current GPUs at usable frame rates​


DSOGAMING.COM


. You could aim for a simpler version if full GI is needed – perhaps one bounce GI with voxel grids or light propagation volumes.


Ray Tracing (DXR/VKray): If hardware allows, both DirectX 12 (via DXR) and Vulkan (via VK_KHR_ray_tracing_pipeline) can do ray tracing for global illumination or reflections. Implementing DXR means writing HLSL raygen/miss/hit shaders and using the DirectX Raytracing API; in Vulkan it means similar SPIR-V shaders. This is a large undertaking but yields very high quality GI (at cost of performance, often need denoising). If GI is a priority and you have DXR-level GPUs, you could consider at least hybrid approaches (ray-trace only large probes or partial lighting and mix with rasterization). This likely goes beyond simply porting your existing code – it’s an enhancement.


Volumetric Effects: These include volumetric fog, light shafts (“God rays”), and volumetric lighting:


Volume Textures and Raymarching: In Vulkan or D3D12, you can create 3D textures or volume buffers and have compute shaders fill them. For example, one method for volumetric fog is to create a 3D grid (aligned to camera frustum) where each cell stores fog density and light. You then raymarch through that in a pixel shader or compute shader to render fog. With explicit APIs, you have more control on how to synchronize lighting updates to the volume (for example, you can compute scattering in the volume via compute, then sample it in the final pass).


Performance Tricks: Volumetric effects can be expensive. Consider rendering them at a lower resolution (many engines do volumetric lighting at half or quarter resolution and upsample with temporal filtering). You can take advantage of descriptor indexing to sample multiple shadow maps or lights within the volumetric shader if needed, without binding each, similarly to the lights discussion.


Async Compute for Volumetrics: Volumetric fog calculations (like a compute shader doing ray marching) can often be run on a compute queue in parallel with the main rendering, since typically they sample depth texture (which might be ready once opaque pass is done) but can run before transparents and UI. Both APIs let you overlap this work on a separate queue if the GPU supports it, reducing impact on frame time.


Precomputed Lookup: If using techniques like Epipolar sampling for god rays or precomputed scattering for atmosphere, you can utilize constant buffers or textures to store those and simply evaluate in shaders, which both APIs handle easily.


General Performance Tuning: For all advanced effects, profile and adjust quality/performance trade-offs. Vulkan and D3D12 make it easier to profile GPU work as you can insert timestamp queries (vkCmdWriteTimestamp or EndQuery in D3D12) around passes to measure how long GI or volumetric passes take. Use that data to decide, for instance, how many rays to cast for GI, or how low-res the volumetric grid can be without losing too much quality.Example – Putting it together: Let’s say you want to implement a new voxel GI system in your engine. With the dual rendering architecture:


On both backends, you’d create a voxel buffer (3D texture or structured buffer).


On D3D12, you might create a compute PSO for voxelization and anothers for tracing. On Vulkan, compute pipelines similarly.


You schedule voxelization (perhaps per frame or one every few frames) as a compute dispatch. On Vulkan, you might use a separate compute queue with a semaphore to signal completion. On DX12, use the direct queue or a compute queue and a fence.


Then you run a fullscreen pass where each pixel’s indirect lighting is fetched via sampling that voxel structure (could be done in a pixel shader within the main render pass, or as compute that writes to a lighting buffer).


Because you have explicit sync, you ensure the voxel data is ready before sampling (a vkCmdPipelineBarrier or D3D12 UAV barrier if using unordered access).


This GI then adds to your lighting equation in the shader, giving diffuse bounce light. CryEngine’s implementation is similar, doing it continuously as an “async job”​


DSOGAMING.COM


, likely meaning a separate thread/queue updating the structure.


Maintaining Parity and Scalability: One challenge is to keep the output of Vulkan and DirectX rendering the same visually. Differences in driver and hardware can cause slight variation, but if your shaders and pipeline states are equivalent, they should look the same. Use reference screenshots and perhaps an automated image comparison to ensure both backends output similar results under identical scenes.Finally, advanced features often come with advanced GPU requirements. Ensure you provide fallbacks or quality settings (e.g., if global illumination is too slow on a lower-end GPU, allow it to be turned off or simplified). One benefit of writing for D3D12/Vulkan is you can more easily scale down by disabling certain passes or using half resolution rendering for specific effects, since you control exactly what the GPU does.


9. Handling API Differences and Synchronization Challenges


When one engine runs on two APIs, it’s important to handle the subtle (and not so subtle) differences in how they deal with resources, pipeline creation, and synchronization. We’ve touched on many throughout the guide; here we summarize the key differences and how to handle them:


Memory Management: OpenGL automated a lot of memory allocation and management. In Vulkan and D3D12, you manage memory. Vulkan requires you to allocate VkDeviceMemory and bind it to buffers/images. D3D12 lets you use committed resources (which behind the scenes allocate memory) or create heaps and place resources. Embrace this by designing a memory allocator in your engine that can suballocate large chunks for small resources to avoid fragmentation. Also manage residency if needed (for extremely large data, you might have to manually implement swapping resources in/out of GPU memory – though on modern GPUs with enough VRAM and Windows’ DXGI managing residency, you often can get by without manual eviction). The main point: allocate large and reuse, instead of many tiny allocations.


Resource Binding and Descriptors: DirectX 12 uses descriptor heaps and root signatures; Vulkan uses descriptor sets and pipeline layouts. They are analogous but not identical:


A D3D12 root signature might correspond to one or two Vulkan descriptor sets. For example, you might split global resources and per-draw resources.


Strategy: You can write a wrapper in your engine that, say, given a set of engine-level binding points (like “frame constant buffer”, “material textures”, etc.), it will do if D3D12: set root parameter X vs if Vulkan: bind descriptor set Y. This mapping is manual but straightforward if you’ve designed the binding strategy clearly.


There is no direct runtime API to share descriptors between D3D12 and Vulkan, but you can ensure they get the same data by populating them from the same source (e.g., updating a uniform buffer in Vulkan and a constant buffer in D3D12 with the same struct data).


Spilling and Binding Costs: Vulkan drivers might “spill” descriptor sets to memory if there are too many, and D3D12 will spill root descriptors if you go beyond hardware limits. The advice given earlier about keeping them small is crucial on both​


GPUOPEN.COM


. If you hit performance issues around binding, consider simplifying your scheme (e.g., combine more into one set/table so you bind less often).


Pipeline Creation and Permutations: Both Vulkan and D3D12 encourage compiling all shader states into pipelines up front. This can lead to a combinatorial explosion if you’re not careful (as one StackExchange question lamented having to create 5×300×5 PSOs for permutations of mesh layout, material, framebuffer​


GAMEDEV.STACKEXCHANGE.COM


). The solution is to reduce combinations or use more flexible shaders:


Use shader constants or branching for minor variations instead of separate shaders (i.e., an “uber-shader” that can handle multiple cases, so you only need one PSO).


Use specialization constants in Vulkan (or HLSL static const branches) to tweak shader behavior at pipeline compile time without needing separate source code, which still gives one pipeline per variant but lets driver optimize out unused branches.


If you truly need many PSOs, compile them on worker threads so your loading doesn’t stall​


GPUOPEN.COM


. And utilize VkPipelineCache or DX12 Pipeline Library to avoid recompiling on subsequent runs​


GPUOPEN.COM


.


In runtime, switching pipelines is not as terrible as in OpenGL (where changing a program could cause heavy shader reconfiguration). In modern APIs, pipeline changes are relatively quick, but still minimize state changes like pipeline and descriptor set switches between draws​


GPUOPEN.COM


.


Synchronization Differences: Perhaps the trickiest difference is how CPU-GPU and GPU-GPU synchronization is expressed:


DirectX 12: It uses fences as the primary sync primitive. A fence in DX12 is a timeline value that the GPU increments (or sets) and CPU waits on​


GAMEDEV.NET


. It doesn’t have separate binary semaphores for GPU-GPU; instead cross-queue sync is done by signaling a fence from one queue and making the other queue wait on that fence value (or using an Event object for finer GPU->CPU signaling). This means you typically use one fence per frame for CPU, and also possibly fences to coordinate multiple queues.


Vulkan: It has fences (for CPU wait of GPU), semaphores (GPU->GPU sync), and events (GPU->GPU finer-grained sync within command buffers). Fences are binary (signaled/unsignaled)​


GAMEDEV.NET


 unless you use the newer timeline semaphores extension. You’ll likely use one fence per frame (to signal when frame done, CPU waits) and at least two semaphores per frame (image acquired, render finished for present). The mapping to DX12: an image-acquire semaphore is like DXGI’s internal mechanism when you call Present (DX12 hides that somewhat), and a render-finish semaphore is like the point you signal the DX12 fence.


Strategy: In your abstract frame scheduler, you might hide these differences. For example, have an abstraction like BeginFrame() and EndFrame() that under the hood does:


Vulkan: vkAcquireNextImage; wait for last frame’s fence; reset command buffers; record; submit graphics queue with wait on acquire semaphore and signal render semaphore & fence; present with wait on render semaphore.


D3D12: wait for last frame’s fence (CPU wait via Event); reset allocators/command list; record and execute command list; Present(); signal fence on command queue; (present is asynchronous with a flip model, the fence is the main sync).


When dealing with multiple GPU queues (compute, etc.), for Vulkan use semaphores between queue submissions (or in modern Vulkan 1.2, timeline semaphores which act like DX12 fences and can be waited cross-queue with a specific value​


GITHUB.COM


). For DX12, use fence values and Wait calls between queues. For instance, signal fence at value N on compute queue after GI pass, then on graphics queue use Wait(fence, N) before using GI results.


Event vs Barrier: Vulkan’s vkCmdSetEvent/vkCmdWaitEvents can be used for more fine-grained ordering (like say after you finish filling a part of a buffer, you want to unlock it for use before command buffer ends). D3D12 doesn’t have an exact equivalent within a command list (it has BeginEvent/EndEvent for PIX markers, not for sync). In D3D12 you’d likely split into two command lists or use a UAV barrier which acts as a memory barrier within a command list for unordered access views. Recognize these differences and ensure that if using such features, you wrap them appropriately or redesign slightly (many engines avoid fine-grained events for portability and just use pipelines barriers and multiple command buffers).


Error Handling: In OpenGL, an error might not crash the program but just glGetError would return something. In D3D12/Vulkan, an error (like using a freed resource or writing out of bounds in a shader) can cause a device lost/reset. Always check the return codes (HRESULT in D3D12 and VkResult in Vulkan) of crucial operations. Especially check for VK_ERROR_DEVICE_LOST which indicates a GPU hang – often due to an out-of-bounds memory access or improper sync. Use the debug layers – they will tell you if, say, you forgot to transition a resource or if you have a GPU-CPU sync issue.


GPU Feature Levels and Extensions: DirectX 12 has feature levels (though if your engine was working on DX11 Feature Level 11, a DX12 device will be FL 11_0 or above on any modern GPU). Vulkan has feature queries and a lot of extensions. When targeting Vulkan, decide a baseline (e.g., Vulkan 1.1 with certain extensions). Ensure not to use features not present on the user’s GPU. For example, if you use descriptor indexing, check VkPhysicalDeviceDescriptorIndexingFeatures before enabling it. On D3D12, certain features like Raytracing or VRS need checking via D3D12FeatureData* queries. Designing with flexibility to disable optional features is good.


Tools and Debugging Differences: Getting used to new tools is part of porting. D3D12’s debug layer is extremely helpful for catching mistakes (enable via D3D12GetDebugInterface as noted, and also use the GPU-based validation mode if chasing a tricky bug). Vulkan’s validation layers do similar checks. RenderDoc works for both APIs (though possibly more fully featured for Vulkan early on). Make debugging builds that compile shaders with debug info (DXC can embed source and debug info in DXIL/SPIR-V) to get better error messages or to step through shaders in tools.


Public Resources for Help: The good news is both Vulkan and DirectX communities have abundant resources. If you hit a specific problem (like “why is my pipeline layout incompatible” or “how to handle transient attachments”), chances are others have asked on forums or Stack Overflow. The official docs from Khronos and Microsoft, and tutorials like Vulkan Tutorial or DirectX 12 by Microsoft are great references for specifics.


Finally, keep in mind OpenGL and the new APIs can coexist for a while. You might not drop OpenGL entirely on day one of this transition. It could be beneficial to have a fallback (especially for older hardware or platforms where Vulkan/DX12 isn’t available). Your abstraction could even include an OpenGL backend if you choose, though maintaining three backends is effort. Many engines eventually drop the old API once the new ones are stable and offering equal or better performance.By carefully handling the differences and playing to each API’s strengths, you will have a robust dual-rendering engine. The result is an engine that can run optimized graphics on Windows (DirectX 12) and Linux (Vulkan), with advanced visual features like high-quality lighting, shadows, and GI all supported. This not only improves performance (thanks to reduced driver overhead and better multi-threading) but also future-proofs the engine for new graphics features and platforms.


References:


ALAIN.XYZ


 Modern APIs (DirectX 12/Vulkan) differ from OpenGL’s state machine design, giving more control to developers.


REDDIT.COM


 When supporting multiple graphics APIs, pick an abstraction level (e.g., DrawModel) and implement it per API, rather than duplicating all low-level calls.


LEARN.MICROSOFT.COM


​


LEARN.MICROSOFT.COM


 Direct3D 12 initialization involves creating the device, command queue, swap chain, and descriptor heaps for render targets (similar to D3D11’s process).


COMMUNITY.ARM.COM


 Legacy APIs like OpenGL couldn’t effectively use multi-threading due to driver limitations, whereas Vulkan addresses this by design, allowing better multi-threaded performance.


COMMUNITY.ARM.COM


 In OpenGL, memory allocation for resources is hidden from the programmer, whereas Vulkan gives explicit control – the driver no longer decides when to allocate or free memory, the developer does.


STACKOVERFLOW.COM


 Microsoft’s DXC compiler can target SPIR-V from HLSL, enabling a unified shader code base (HLSL) for both DirectX 12 and Vulkan backends.


GITHUB.COM


 Adding Vulkan and DirectX 12 support often requires extending the content pipeline to handle new shader formats (e.g., SPIR-V binaries, DXIL blobs).


GPUOPEN.COM


 Pipeline State Objects (PSOs) in modern APIs bundle all the fixed state and shaders, avoiding runtime shader compilation stutters by moving that work to load time.


GPUOPEN.COM


 For efficient descriptors: avoid overly large descriptor layouts; group resources that are used together in one set to minimize how often descriptor sets are updated or switched.


GAMEDEV.NET


​


GAMEDEV.NET


 DirectX 12 uses timeline fences (monotonically increasing values) for synchronization, while Vulkan uses binary semaphores and fences – requiring different strategies for coordinating GPU work.


HACKSOFLIFE.BLOGSPOT.COM


 Vulkan 1.1 allows a negative viewport height to flip the Y-axis orientation, which helps match OpenGL’s coordinate system if needed.


DSOGAMING.COM


 CryEngine’s real-time Global Illumination uses voxel tracing: continuously voxelize scene geometry (async) and trace rays against it for indirect lighting – achieving GI with good performance on modern hardware.
