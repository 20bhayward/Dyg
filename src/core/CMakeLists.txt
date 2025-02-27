add_library(core
    VoxelData.cpp
    Chunk.cpp
    World.cpp
    ChunkManager.cpp
)

# Make sure all files exist
file(TOUCH VoxelData.cpp Chunk.cpp World.cpp ChunkManager.cpp)

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(core PRIVATE util)