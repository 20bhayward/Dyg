2D Pixel Physics Sidescroller – Technical Overview and Recommendations
Every pixel in the game world will be an interactive simulated element, much like in Noita, where materials like sand, water, oil, and gas obey physical rules​
80.LV
​
EN.WIKIPEDIA.ORG
. Achieving this in a performant way requires careful choice of technology and architecture. Below we break down the key considerations and optimizations for building a Noita-like sandbox game, ensuring it runs smoothly on Linux and can be developed efficiently.

# PixelPhys2D

A 2D Sidescrolling Pixel Physics Sandbox Game with advanced lighting effects.

## Overview

PixelPhys2D is a 2D side-scrolling sandbox game where every pixel of the world is simulated. Inspired by games like Noita, Terraria, and featuring lighting similar to Valheim and Teardown, the game includes falling sand-style granular physics, fluids, gases, and fire, all interacting in a procedurally generated world with photorealistic lighting. 

Terrain is fully destructible – you can burn wood, dissolve rock in acid, or blow up structures and watch the debris fall. The world is generated anew each run with diverse biomes (e.g. mines, ice caves, lava caverns) and dynamic events, ensuring a unique experience every time. The engine is optimized for performance with multi-threading and hardware acceleration so that the simulation runs smoothly even with tens of thousands of interacting pixels.

## Key Features

- **Advanced Pixel Simulation:** Every pixel is an element (stone, sand, water, oil, lava, gas, etc.) that follows physical rules (sand falls, water flows, gases rise, fire spreads)
- **Physically-Based Lighting:** Featuring global illumination, volumetric lighting, god rays, dynamic shadows, and HDR rendering similar to AAA games
- **Material Interactions:** Combustible materials and chemical reactions allow liquids and powders to ignite or explode based on interactions
- **Rigid Body Physics:** Large objects like chunks of terrain or props break off and tumble using Box2D physics for realistic movement
- **Procedural Generation:** Diverse world with multiple biomes and cave systems using advanced algorithms for natural-looking terrain
- **Performance Optimized:** Multi-threaded engine and GPU-accelerated rendering for smooth performance on multicore systems
- **Cross-Platform:** Developed with OpenGL for compatibility across Windows, Linux, and macOS

## System Requirements

### Minimum Requirements
- **OS:** Windows 10/11, Ubuntu 22.04+, or macOS 12+
- **CPU:** Quad-core processor @ 2.5GHz
- **GPU:** Graphics card with OpenGL 3.3+ support (Intel HD 5000+, NVIDIA GTX 700+, AMD Radeon R7+)
- **RAM:** 4GB
- **Storage:** 500MB

### Recommended Requirements
- **OS:** Windows 10/11, Ubuntu 22.04+, or macOS 12+
- **CPU:** 6+ core processor @ 3.0GHz+
- **GPU:** Dedicated graphics card with OpenGL 4.5+ support (NVIDIA GTX 1060+ / AMD RX 570+)
- **RAM:** 8GB+
- **Storage:** 1GB

## Installation

### Prerequisites

#### Linux (Ubuntu/Debian)
```bash
# Install required dependencies
sudo apt-get install build-essential cmake libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev

# Optional: For hardware-accelerated graphics (NVIDIA)
sudo apt-get install nvidia-driver-535  # or latest available driver

# Optional: For improved lighting performance
sudo apt-get install libopenvdb-dev
```

#### Windows
1. Install Visual Studio 2019 or newer with C++ development tools
2. Install CMake (https://cmake.org/download/)
3. Install vcpkg and the required libraries:
```batch
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install sdl2:x64-windows glew:x64-windows box2d:x64-windows opengl:x64-windows
vcpkg integrate install
```

### Building and Running

#### Linux
```bash
git clone https://github.com/yourusername/PixelPhys2D.git
cd PixelPhys2D
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./PixelPhys2D
```

#### Windows

**Option 1: Using CMake GUI**
1. Clone the repository
2. Open CMake GUI
3. Set source directory to the root of the repository
4. Set build directory to `build` folder within repository
5. Click Configure, select your Visual Studio version
6. Click Generate
7. Click Open Project to open in Visual Studio
8. Build the solution in Visual Studio (Release mode recommended)
9. Run the `run_game.bat` file in the build folder

**Option 2: Using Command Line**
```batch
git clone https://github.com/yourusername/PixelPhys2D.git
cd PixelPhys2D
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
run_game.bat
```
NOTE: Replace `C:/path/to/vcpkg` with your actual vcpkg installation path.

#### macOS
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake sdl2 glew box2d

# Build the project
git clone https://github.com/yourusername/PixelPhys2D.git
cd PixelPhys2D
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./PixelPhys2D
```

## Troubleshooting

### Common Issues

#### Windows
- **DLL Missing Errors**: If you get errors about missing DLLs when running the game, make sure you built the project with vcpkg integration. The build process should automatically copy required DLLs to the output directory.
- **OpenGL Issues**: If you see graphics glitches or black screens, update your graphics card drivers.
- **Build Failures**: Make sure Visual Studio has C++ desktop development workload installed.

#### Linux
- **SDL2 Not Found**: Ensure libsdl2-dev is properly installed. On some distributions, you may need to set SDL2_DIR environment variable.
- **OpenGL Performance**: For better performance, install proprietary graphics drivers if using NVIDIA or AMD GPUs.
- **Multi-threading Issues**: If you experience crashes related to threading, verify your system has pthread support installed.

### Platform-Specific Notes

#### Windows Performance Tips
- Running in dedicated GPU mode will significantly improve performance (right-click the executable and select "Run with graphics processor" if available)
- The simplified sky rendering mode can be toggled by setting `-DSIMPLIFIED_SKY=ON/OFF` during CMake configuration

#### Linux Integration
- For distribution packaging, the game looks for assets in the current directory by default, which can be overridden by setting the `PIXELPHYS_ASSETS` environment variable
- If running from a desktop shortcut, specify the working directory as the directory containing the assets folder

## Controls

- **WASD/Arrow Keys**: Move character
- **Space**: Jump
- **Left Mouse Button**: Place selected material or dig terrain (when close enough)
- **Right Mouse Button**: Erase (place empty)
- **1-0 Keys**: Select different materials
- **C**: Toggle between free camera and follow mode
- **Mouse Wheel**: Zoom in/out
- **F11**: Toggle fullscreen
- **ESC**: Quit

## Contributing

Contributions to PixelPhys2D are welcome! Please feel free to submit pull requests or open issues on GitHub.

When submitting changes:
1. Follow the existing code style
2. Add tests for new functionality
3. Ensure the code builds on all supported platforms
4. Update documentation as needed

## License

This project is licensed under the MIT License - see the LICENSE file for details.