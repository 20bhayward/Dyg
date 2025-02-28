# PixelPhys2D Development Guidelines

## Build Commands
- Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j$(nproc)`
- Debug Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . -j$(nproc)`
- Run: `./build/PixelPhys2D`
- Clean: `rm -rf build && mkdir -p build`
- Build with OpenVDB (volumetric lighting): Add `-DUSE_OPENVDB=ON` to cmake command

## Dependencies (Linux/Ubuntu)
```bash
sudo apt-get install build-essential cmake libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev
# For improved lighting performance: sudo apt-get install libopenvdb-dev
```

## Styling Guidelines
- Use C++17 standard
- Class names: PascalCase (e.g., `World`, `Renderer`)
- Methods/functions: camelCase (e.g., `updatePlayer()`)
- Member variables: m_camelCase prefix (e.g., `m_isDirty`)
- Constants: UPPER_CASE (e.g., `TARGET_FPS`)
- Namespaces: Use `PixelPhys` namespace for all core components
- Indentation: 4 spaces (no tabs)
- Include guards: #pragma once
- Error handling: Check return values and use assertions for invariants
- Line length: Maximum 100 characters
- File organization: Related declarations grouped together

## Architecture
- Split physics simulation into chunks for multithreading
- Use checkerboard pattern for cell updates to avoid race conditions
- Mark dirty regions to optimize active simulation areas
- Follow data-oriented design for better cache performance
- Material system: Check MATERIALS.md for material properties and interactions
- Rendering pipeline: OpenGL with shader-based lighting effects