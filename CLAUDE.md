# CLAUDE.MD: Architecture for a Command-Line Voxel World Generator & Physics Simulator

This document details a robust architecture for a command-line application that procedurally generates a voxel world with integrated physics (think “Noita” meets a voxel sandbox). The focus is on performance and scalability, with future extensibility for real-time physics simulation. This solution is designed to run without a game engine – everything is implemented from scratch using command-line tools and libraries.

---

## 1. Overview

The architecture is split into several key modules:
- **World Generation Pipeline**: A layered approach that builds terrain, subterranean features, biomes, and optional structures.
- **Chunk Management & Streaming**: Dynamically load/unload chunks based on a virtual “player” position.
- **Voxel Data Structures**: Use dense 3D arrays (with possible palette compression) for fast neighbor queries and updates.
- **Procedural Generation Algorithms**: Utilize noise functions (e.g. Perlin or Simplex) and cellular automata for natural terrain and cave formation.
- **Parallelization & Multithreading**: Employ multi-threaded chunk generation and physics updates using C++ standard threads (or async/Thread Pools).
- **Physics Simulation Module**: Prepare the data structure for future integration of physics—supporting dynamic updates, collision, and fluid/granular simulations.
- **File I/O & Persistence**: Save and load chunk data using binary file formats with lightweight compression (e.g., run-length encoding).

---

## 2. Language and Tooling

**Primary Language:** C++ (C++17 or newer recommended)  
**Optional Modules:** Rust can be used for parts of the performance-critical pipeline (e.g., noise generation or meshing)  
**Build Tools:** Make/CMake (command-line based)  
**Libraries/Dependencies:**
- **Noise Generation:** Use FastNoise or a custom Perlin/Simplex noise library.
- **Threading:** Utilize the C++ Standard Library (`<thread>`, `<future>`, `<mutex>`) for parallel tasks.
- **Compression:** zlib or a custom lightweight run-length encoder.
- **CLI Utilities:** Standard I/O for configuration and logging; optionally, a minimal rendering of debug output in the terminal.

---

## 3. World Generation Pipeline

The world is generated in multiple layers (passes) to separate concerns and optimize performance.

### 3.1 Base Terrain Generation
- **Input:** Chunk coordinates, seed value.
- **Algorithm:** Use 2D noise (Perlin/Simplex) to generate a heightmap.
- **Output:** A 3D array for each chunk where each voxel below the heightmap is marked as “solid” (e.g., stone/soil) and above as “air.”
- **Command-Line Query (AI Prompt):**
  > "Generate a chunk heightmap using layered Perlin noise for a given seed and chunk coordinates."

### 3.2 Subterranean Features: Caves and Ores
- **Cave Carving:** Use cellular automata on the base terrain to remove voxels and form cave networks.
- **Ore Distribution:** Overlay additional noise maps or random scatter algorithms to place ores at specific depths.
- **Command-Line Query (AI Prompt):**
  > "Implement a CA-based cave generation pass on the pre-generated terrain and overlay an ore distribution algorithm."

### 3.3 Surface and Biome Assignment
- **Surface Layer:** Replace the top-most blocks with biome-specific materials (e.g., grass, sand, snow).
- **Biome Map:** Optionally, generate a separate biome map via combined noise functions.
- **Command-Line Query (AI Prompt):**
  > "Design a biome layer that maps terrain height and temperature to assign surface materials."

### 3.4 Optional Structure/Decoration Layer
- **Additional Pass:** Insert structures, vegetation, or decorations based on biome rules.
- **Command-Line Query (AI Prompt):**
  > "Create an optional structure pass that stamps predefined patterns into the world based on local biome data."

---

## 4. Chunk Management & Streaming

### 4.1 Chunk Structure
- **Size:** Fixed-size 3D arrays (e.g., 16×16×256 voxels per chunk).
- **Data Organization:** Use contiguous memory for fast access.
- **Key:** Each chunk is identified by its (x, y, z) coordinate in the world grid.

### 4.2 Dynamic Loading & Unloading
- **Active Set:** Maintain a configurable “view distance” (in chunks) around the virtual player.
- **Prefetching:** Load chunks in a spiral or concentric-ring order based on proximity.
- **Asynchronous Processing:** Spawn separate threads to generate and load chunks in the background.
- **Command-Line Query (AI Prompt):**
  > "Implement a chunk manager that streams chunks in/out around a moving point, using a thread pool for asynchronous generation."

---

## 5. Voxel Data Structures

### 5.1 Dense 3D Arrays
- **Implementation:** Each chunk uses a 3D array (or a flat array indexed in 3 dimensions) for fast O(1) access.
- **Memory Consideration:** Use palette compression if most voxels are the same type.

### 5.2 Data Access & Updates
- **Neighbor Queries:** Optimize indexing for fast neighbor lookups—critical for both world generation and physics.
- **Mutation:** Allow efficient modifications (e.g., for explosions or physics updates).
- **Command-Line Query (AI Prompt):**
  > "Design a voxel data structure optimized for neighbor access and efficient updates, with optional palette compression."

---

## 6. Procedural Generation Algorithms

### 6.1 Noise Functions
- **Usage:** Generate heightmaps and biome maps using Perlin/Simplex noise.
- **Optimization:** Cache noise values per chunk; consider SIMD or GPU-based implementations if needed.
- **Command-Line Query (AI Prompt):**
  > "Implement a noise generation module in C++ to produce 2D and 3D noise fields optimized for chunk-based terrain."

### 6.2 Cellular Automata (CA)
- **Usage:** Create natural cave systems and simulate erosion.
- **Performance:** Limit CA iterations to a local area for performance.
- **Command-Line Query (AI Prompt):**
  > "Develop a cellular automata module that refines terrain to create cave structures with a configurable number of iterations."

---

## 7. Parallelization & Multithreading

### 7.1 Multi-Threaded Chunk Generation
- **Thread Pool:** Create a pool of worker threads to handle chunk generation tasks.
- **Task Queue:** Each new chunk is enqueued as a task. Workers process tasks asynchronously.
- **Synchronization:** Use thread-safe queues (with mutexes or lock-free structures) to communicate completed chunks to the main thread.
- **Command-Line Query (AI Prompt):**
  > "Design a thread pool and task queue system in C++ to generate voxel chunks asynchronously."

### 7.2 Physics Simulation Parallelism
- **Concurrent Updates:** Run physics updates (e.g., falling sand, fluid simulation) in parallel across chunks.
- **Data Safety:** Ensure each physics update is confined to its chunk or a limited neighborhood to minimize locking overhead.
- **Command-Line Query (AI Prompt):**
  > "Implement a multi-threaded physics simulation that updates voxel states (like falling sand) in parallel across active chunks."

---

## 8. Physics Simulation Module

### 8.1 Physics Data Integration
- **Direct Grid Access:** Use the same voxel grid for physics queries (neighbor checks, connectivity, etc.).
- **Collision & Dynamics:** Plan for converting clusters of voxels into dynamic objects when physics events occur.
- **Command-Line Query (AI Prompt):**
  > "Design a physics module that can quickly update voxel states and, when necessary, group voxels into dynamic objects for collision simulation."

### 8.2 Custom Physics vs. Engine Physics
- **Custom Solvers:** Write custom physics solvers for granular and fluid behaviors without relying on external engines.
- **Data Reuse:** Keep the voxel data immutable for rendering while applying physics changes to a secondary simulation buffer.
- **Command-Line Query (AI Prompt):**
  > "Implement a custom physics solver optimized for voxel-based granular and fluid dynamics with minimal overhead."

---

## 9. File I/O & Persistence

### 9.1 Chunk Saving & Loading
- **Binary Format:** Save each chunk as a binary file with versioning.
- **Compression:** Use run-length encoding (RLE) or zlib for compressing voxel data.
- **Asynchronous I/O:** Offload disk operations to background threads to avoid blocking the main generation loop.
- **Command-Line Query (AI Prompt):**
  > "Develop a module that efficiently serializes and deserializes chunk data to and from disk using binary formats and lightweight compression."

---

## 10. Putting It All Together: Command-Line Workflow

### 10.1 Startup & Configuration
- **CLI Options:** Parse command-line arguments for seed, view distance, chunk size, and physics simulation options.
- **Initialization:** Initialize thread pools, load configuration files, and set up logging.

### 10.2 Main Loop (Pseudo-Code)
```cpp
int main(int argc, char* argv[]) {
    // Parse CLI arguments (seed, view distance, etc.)
    Config config = parseArguments(argc, argv);
    
    // Initialize world manager, thread pools, and physics simulator
    WorldManager world(config);
    PhysicsSimulator physics;
    ThreadPool chunkPool(config.numThreads);
    
    while (running) {
        // Determine player position (or simulation focal point)
        Vector3 playerPos = getPlayerPosition();
        
        // Update chunk manager: stream in new chunks asynchronously
        world.updateChunks(playerPos, chunkPool);
        
        // Process completed chunk tasks and integrate into world
        world.integrateCompletedChunks();
        
        // Run physics update in parallel over active chunks
        physics.update(world.getActiveChunks());
        
        // Optionally, output current simulation state to console for debugging
        renderDebugView(world);
        
        // Sleep or wait for next frame tick to limit CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(config.frameDelay));
    }
    
    return 0;
}
