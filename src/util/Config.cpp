#include "util/Config.h"
#include <iostream>
#include <cstring>
#include <ctime>
#include <random>
#include <thread>

namespace dyg {
namespace util {

Config::Config() {
    // Set defaults
    seed = static_cast<uint32_t>(time(nullptr));
    viewDistance = 5;
    chunkSize = 16;
    worldHeight = 256;
    
    numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    frameDelay = 50;
    
    baseNoiseScale = 0.01f;
    detailNoiseScale = 0.05f;
    caveIterations = 3;
    oreDensity = 0.05f;
    
    temperatureScale = 0.002f;
    humidityScale = 0.002f;
    
    saveDirectory = "saves";
    useCompression = true;
}

Config::Config(int argc, char* argv[]) : Config() {
    parseArguments(argc, argv);
}

bool Config::parseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp();
            return false;
        } else if (strcmp(argv[i], "--seed") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                seed = static_cast<uint32_t>(std::stoul(argv[++i]));
            } else {
                std::cerr << "Missing value for seed" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--view-distance") == 0 || strcmp(argv[i], "-vd") == 0) {
            if (i + 1 < argc) {
                viewDistance = std::stoi(argv[++i]);
            } else {
                std::cerr << "Missing value for view distance" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--chunk-size") == 0 || strcmp(argv[i], "-cs") == 0) {
            if (i + 1 < argc) {
                chunkSize = std::stoi(argv[++i]);
            } else {
                std::cerr << "Missing value for chunk size" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--threads") == 0 || strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                numThreads = std::stoi(argv[++i]);
            } else {
                std::cerr << "Missing value for threads" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--save-dir") == 0 || strcmp(argv[i], "-sd") == 0) {
            if (i + 1 < argc) {
                saveDirectory = argv[++i];
            } else {
                std::cerr << "Missing value for save directory" << std::endl;
                return false;
            }
        } else if (strcmp(argv[i], "--no-compression") == 0 || strcmp(argv[i], "-nc") == 0) {
            useCompression = false;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            return false;
        }
    }
    
    return true;
}

void Config::printHelp() const {
    std::cout << "Dyg: Voxel World Generator & Physics Simulator" << std::endl;
    std::cout << "Usage: dyg [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help                 Show this help message" << std::endl;
    std::cout << "  -s, --seed <value>         Set the random seed (default: current time)" << std::endl;
    std::cout << "  -vd, --view-distance <n>   Set the view distance in chunks (default: 5)" << std::endl;
    std::cout << "  -cs, --chunk-size <n>      Set the chunk size (default: 16)" << std::endl;
    std::cout << "  -t, --threads <n>          Set the number of worker threads (default: cores-1)" << std::endl;
    std::cout << "  -sd, --save-dir <path>     Set the save directory (default: 'saves')" << std::endl;
    std::cout << "  -nc, --no-compression      Disable chunk compression (default: enabled)" << std::endl;
}

} // namespace util
} // namespace dyg