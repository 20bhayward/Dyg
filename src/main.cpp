#include "core/World.h"
#include "physics/PhysicsSimulator.h"
#include "util/Config.h"
#include "util/ThreadPool.h"
#include "util/Vector3.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

using namespace dyg;

// Debug visualization in the terminal
void renderDebugView(const core::World& world, const util::Vector3& playerPos) {
    std::cout << "World Simulation (Seed: " << world.getSeed() << ")" << std::endl;
    std::cout << "Player Position: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << ")" << std::endl;
    
    // Get active chunks
    auto chunks = world.getActiveChunks();
    std::cout << "Active Chunks: " << chunks.size() << std::endl;
    
    // Show a basic ASCII representation of the world slice (top-down view)
    const int size = 40;
    const int half = size / 2;
    
    std::vector<std::string> grid(size, std::string(size, ' '));
    
    // Mark player position
    int px = half + 0;
    int pz = half + 0;
    if (px >= 0 && px < size && pz >= 0 && pz < size) {
        grid[pz][px] = '@';
    }
    
    // Mark chunk positions
    for (const auto& chunk : chunks) {
        const auto& pos = chunk->getPosition();
        int cx = half + pos.x;
        int cz = half + pos.z;
        if (cx >= 0 && cx < size && cz >= 0 && cz < size) {
            grid[cz][cx] = '#';
        }
    }
    
    // Print the grid
    std::cout << std::string(size + 2, '-') << std::endl;
    for (const auto& row : grid) {
        std::cout << "|" << row << "|" << std::endl;
    }
    std::cout << std::string(size + 2, '-') << std::endl;
}

// Parse arguments and create Config instance
util::Config parseArguments(int argc, char* argv[]) {
    util::Config config;
    
    // Parse any command line arguments
    if (!config.parseArguments(argc, argv)) {
        config.printHelp();
        exit(1);
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    std::cout << "Dyg: Voxel World Generator & Physics Simulator" << std::endl;
    std::cout << "==============================================" << std::endl;
    
    // Parse command line arguments
    util::Config config = parseArguments(argc, argv);
    
    // Initialize world
    core::World world(config);
    
    // Initialize physics simulator
    physics::PhysicsSimulator physics(config);
    
    // Create thread pool for parallel processing
    util::ThreadPool threadPool(config.numThreads);
    
    // Virtual player position
    util::Vector3 playerPos(0, 128, 0);
    
    // Simulation loop
    bool running = true;
    int frame = 0;
    
    std::cout << "Starting simulation..." << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (running && frame < 1000) { // Limit to 1000 frames for this example
        // Update player position (simulate movement)
        if (frame % 10 == 0) {
            playerPos.x += 1;
            playerPos.z += 1;
        }
        
        // Update chunks based on player position
        auto chunkUpdateStart = std::chrono::high_resolution_clock::now();
        world.updateChunks(playerPos, threadPool);
        auto chunkUpdateEnd = std::chrono::high_resolution_clock::now();
        
        // Process completed chunk generation tasks
        auto integrateStart = std::chrono::high_resolution_clock::now();
        int completedChunks = world.integrateCompletedChunks();
        auto integrateEnd = std::chrono::high_resolution_clock::now();
        
        // Run physics simulation on active chunks
        auto physicsStart = std::chrono::high_resolution_clock::now();
        int updatedVoxels = physics.update(world.getActiveChunks(), threadPool);
        auto physicsEnd = std::chrono::high_resolution_clock::now();
        
        // Print statistics every 10 frames
        if (frame % 10 == 0) {
            // Calculate timings
            auto chunkUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(chunkUpdateEnd - chunkUpdateStart).count();
            auto integrateTime = std::chrono::duration_cast<std::chrono::milliseconds>(integrateEnd - integrateStart).count();
            auto physicsTime = std::chrono::duration_cast<std::chrono::milliseconds>(physicsEnd - physicsStart).count();
            auto totalFrameTime = chunkUpdateTime + integrateTime + physicsTime;
            
            // Display debug info
            std::cout << "\nFrame " << frame << ":" << std::endl;
            std::cout << "  Chunk Update: " << chunkUpdateTime << "ms" << std::endl;
            std::cout << "  Chunk Integration: " << integrateTime << "ms (" << completedChunks << " chunks)" << std::endl;
            std::cout << "  Physics Update: " << physicsTime << "ms (" << updatedVoxels << " voxels)" << std::endl;
            std::cout << "  Total Frame Time: " << totalFrameTime << "ms" << std::endl;
            
            // Render debug view
            renderDebugView(world, playerPos);
        }
        
        // Delay to limit CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(config.frameDelay));
        
        frame++;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    std::cout << "\nSimulation completed." << std::endl;
    std::cout << "Ran " << frame << " frames in " << totalTime << " seconds." << std::endl;
    std::cout << "Average FPS: " << static_cast<float>(frame) / totalTime << std::endl;
    
    // Save the world
    std::cout << "Saving world... ";
    if (world.save()) {
        std::cout << "done." << std::endl;
    } else {
        std::cout << "failed!" << std::endl;
    }
    
    return 0;
}