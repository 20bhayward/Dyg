# Running on Windows with Vulkan Support

This guide explains how to build and run PixelPhys2D with Vulkan support on Windows.

## Option 1: Download Pre-built Version (Recommended)

1. Go to the GitHub repository
2. Click on the "Actions" tab
3. Look for the latest successful "Windows Build" workflow run
4. Download the "PixelPhys2D-Windows" artifact
5. Extract the ZIP file
6. Run `run_game.bat` to start the application

## Option 2: Building Manually

### Prerequisites

1. **Visual Studio 2019 or 2022** with C++ workload
2. **CMake** (3.15 or higher)
3. **SDL2 Development Libraries** for Windows
   - Download from [SDL2 website](https://www.libsdl.org/download-2.0.php)
   - Extract to a known location
4. **GLEW** for Windows
   - Download from [GLEW website](http://glew.sourceforge.net/)
   - Extract to a known location
5. **Vulkan SDK**
   - Download and install from [LunarG](https://vulkan.lunarg.com/sdk/home#windows)
   - Make sure the VULKAN_SDK environment variable is set

### Build Steps

1. Clone the repository:
   ```
   git clone https://github.com/yourusername/PixelPhys2D.git
   cd PixelPhys2D
   ```

2. Create a build directory:
   ```
   mkdir build
   cd build
   ```

3. Configure with CMake (replace paths with your actual SDL2 and GLEW locations):
   ```
   cmake .. -DCMAKE_PREFIX_PATH="C:/path/to/SDL2;C:/path/to/GLEW" -DUSE_VULKAN=ON
   ```

4. Build the project:
   ```
   cmake --build . --config Release
   ```

5. Copy required DLLs to the output directory:
   - Copy `SDL2.dll` from your SDL2 installation to the `build/Release` directory
   - Copy `glew32.dll` from your GLEW installation to the `build/Release` directory
   - Copy `vulkan-1.dll` from your Vulkan SDK's `Bin` directory to the `build/Release` directory

6. Run the application:
   ```
   cd Release
   PixelPhys2D.exe
   ```

## Troubleshooting

### Vulkan Runtime Issues

If you see errors related to Vulkan initialization:

1. Make sure you have a graphics card that supports Vulkan
2. Ensure you have the latest graphics drivers installed
3. Verify the Vulkan SDK is installed correctly
4. Check that vulkan-1.dll is in the same directory as the executable

### Missing DLL Errors

If you see errors about missing DLLs:

1. Make sure all required DLLs (SDL2.dll, glew32.dll, vulkan-1.dll) are in the same directory as the executable
2. If running from Visual Studio, copy the DLLs to both the `Debug` and `Release` directories

### Application Crashes

If the application crashes during shader pipeline creation:

1. This is a known issue we're working on
2. Try running with OpenGL backend as an alternative (-DUSE_VULKAN=OFF when building)