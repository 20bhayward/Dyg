# Dyg - Voxel World Generator & Physics Simulator

Dyg is a command-line voxel world generator and physics simulator built in C++17. It focuses on procedural terrain generation with integrated physics simulation for voxel-based environments.

## Features

- **Procedural World Generation**: Generate infinite worlds using Perlin noise algorithms
- **Chunk-Based Architecture**: Efficient streaming of world chunks around a focal point
- **Multi-Threaded Processing**: Parallel chunk generation and physics updates
- **Voxel Physics**: Simulate granular materials (sand) and fluids (water, lava)
- **Cave Generation**: Create natural cave networks using cellular automata
- **Ore Distribution**: Place ores throughout the world with depth-based distribution
- **Saving/Loading**: Persistent worlds with efficient binary storage format

## Building from Source

### Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+, or MSVC 19.14+)
- CMake 3.14 or higher
- Make (or alternative build tool)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/dyg.git
cd dyg

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run the application
./dyg
```

## Usage

```bash
# Basic usage with default settings
./dyg

# Specify a seed for world generation
./dyg --seed 12345

# Set view distance (in chunks)
./dyg --view-distance 8

# Specify number of worker threads
./dyg --threads 4

# Disable chunk compression
./dyg --no-compression

# Get help
./dyg --help
```

## Project Structure

- `core/`: Core world representation (chunks, voxels)
- `generation/`: Procedural generation algorithms
- `physics/`: Physics simulation for voxels
- `util/`: Utility classes (config, vectors, file I/O)

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Inspired by games like Noita, Minecraft, and Terraria
- Built from scratch without using a game engine