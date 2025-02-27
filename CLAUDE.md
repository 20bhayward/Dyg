# PixelPhys2D Development Guidelines

## Build Commands
- Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j$(nproc)`
- Debug Build: `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . -j$(nproc)`
- Run: `./build/PixelPhys2D`

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

## Architecture
- Split physics simulation into chunks for multithreading
- Use checkerboard pattern for cell updates to avoid race conditions
- Mark dirty regions to optimize active simulation areas
- Follow data-oriented design for better cache performance