add_library(generation
    NoiseGenerator.cpp
    TerrainGenerator.cpp
    CaveGenerator.cpp
    BiomeGenerator.cpp
    StructureGenerator.cpp
)

# Make sure all files exist
file(TOUCH NoiseGenerator.cpp TerrainGenerator.cpp CaveGenerator.cpp BiomeGenerator.cpp StructureGenerator.cpp)

target_include_directories(generation PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(generation PRIVATE core util)