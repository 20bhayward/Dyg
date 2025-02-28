# PixelPhys2D Development Guidelines

## Build Commands (Linux)
- Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j$(nproc)`
- Debug Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . -j$(nproc)`
- Quick rebuild: `cd build && make -j$(nproc)`
- Run: `./build/PixelPhys2D`
- Clean: `rm -rf build && mkdir -p build`

## Build Commands (Windows)
- Build environment: Visual Studio with C++ workload or MinGW
- Dependencies: Install SDL2 and GLEW manually or with vcpkg
- Build: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --config Release`
- Run: `.\build\Release\PixelPhys2D.exe`
- Note: On Windows, place SDL2.dll and glew32.dll in the same directory as the executable

## Dependencies
- Linux: `sudo apt-get install build-essential cmake libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev`
- Windows: Install SDL2 and GLEW development libraries (download from official websites)
- Optional flags: Add `-DUSE_OPENVDB=ON` for volumetric lighting (requires OpenVDB)

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
- Error handling: Check return values, use assertions for invariants, avoid exceptions
- Line length: Maximum 100 characters
- Memory management: Use smart pointers (std::unique_ptr/std::shared_ptr) over raw pointers

## Architecture
- Split physics simulation into chunks for multithreading
- Use checkerboard pattern for cell updates to avoid race conditions
- Mark dirty regions to optimize active simulation areas
- Follow data-oriented design for better cache performance