cmake_minimum_required(VERSION 3.15)
project(PixelPhys2D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Find required packages
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include 
    ${SDL2_INCLUDE_DIRS}
)
if(GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIRS})
endif()

# Add executable
add_executable(PixelPhys2D 
    src/main.cpp
    src/World.cpp
    src/Renderer.cpp
    src/UI.cpp        # Added UI implementation
)

# Link libraries
target_link_libraries(PixelPhys2D 
    ${SDL2_LIBRARIES} 
    ${OPENGL_LIBRARIES} 
    ${GLEW_LIBRARIES} 
    pthread
)
add_definitions(-DUSE_GLEW)