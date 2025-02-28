# CMake configuration for cross-platform compatibility 
cmake_minimum_required(VERSION 3.10)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Include directories
include_directories(${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})

# Add all source files
file(GLOB SOURCES "*.cpp")

# Create the executable
add_executable(PixelPhys2D ${SOURCES})

# Link libraries
target_link_libraries(PixelPhys2D ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    target_link_libraries(PixelPhys2D opengl32 glu32)
    
    # Copy SDL2.dll to the output directory (for Windows)
    if(SDL2_LIBRARIES)
        string(REGEX REPLACE "(.*)/(.*)/.*" "\\1/\\2" SDL2_DLL_DIR ${SDL2_LIBRARIES})
        file(GLOB SDL2_DLL "${SDL2_DLL_DIR}/SDL2.dll")
        if(SDL2_DLL)
            add_custom_command(TARGET PixelPhys2D POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SDL2_DLL}
                $<TARGET_FILE_DIR:PixelPhys2D>)
        endif()
    endif()
elseif(APPLE)
    # macOS-specific settings
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    target_link_libraries(PixelPhys2D ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
endif()

# Set optimization flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Set debugging flags for Debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")