cmake_minimum_required(VERSION 3.15)
project(PixelPhys2D VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard and compiler options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Custom Vulkan SDK path for Windows
if(WIN32 AND EXISTS "C:/VulkanSDK/1.4.304.1/")
    set(VULKAN_SDK "C:/VulkanSDK/1.4.304.1/" CACHE PATH "Path to Vulkan SDK")
    message(STATUS "Using custom Vulkan SDK path: ${VULKAN_SDK}")
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "${VULKAN_SDK}")
endif()

# Check for glslc
find_program(GLSLC_EXECUTABLE glslc)
if(NOT GLSLC_EXECUTABLE)
    message(STATUS "glslc not found, shaders will need to be compiled manually")
endif()

# Check for glslangValidator
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)
if(NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(STATUS "glslangValidator not found. Shaders will need to be compiled manually.")
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Find dependencies
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(Vulkan REQUIRED)

# Configuration options
option(USE_VULKAN "Use Vulkan rendering backend" ON)
option(SIMPLIFIED_SKY "Use simplified sky rendering" ON)
option(USE_OPENVDB "Use OpenVDB for volumetric lighting" OFF)

# Vulkan configuration
if(USE_VULKAN)
    if(Vulkan_FOUND)
        message(STATUS "Building with Vulkan backend")
        add_definitions(-DUSE_VULKAN)
        include_directories(${Vulkan_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "Vulkan not found but requested")
    endif()
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main_vulkan.cpp
    src/World.cpp
    src/Renderer.cpp
    src/VulkanBackend.cpp
    src/RenderBackend.cpp
)

# Create executable
add_executable(PixelPhys2D ${SOURCES})

# Link libraries
target_link_libraries(PixelPhys2D
    SDL2::SDL2
    ${GLEW_LIBRARIES}
)

# Vulkan specific linking
if(USE_VULKAN)
    target_link_libraries(PixelPhys2D ${Vulkan_LIBRARIES})
endif()

# Windows-specific configuration
if(WIN32)
    target_link_libraries(PixelPhys2D 
        glu32
        SDL2::SDL2main
    )
    
    target_compile_definitions(PixelPhys2D PRIVATE -DSDL_MAIN_HANDLED)
    
    # Copy the SDL2 DLL into the build folder after building
    add_custom_command(TARGET PixelPhys2D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/../bin/SDL2.dll"
        $<TARGET_FILE_DIR:PixelPhys2D>
    )
    
    # Create run script
    file(WRITE ${CMAKE_BINARY_DIR}/run_game.bat 
        "@echo off\n"
        "PixelPhys2D.exe\n"
        "if ERRORLEVEL 1 pause\n")
endif()

# Linux/macOS configuration
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(PixelPhys2D ${CMAKE_THREAD_LIBS_INIT})
elseif(APPLE)
    find_library(COCOA Cocoa)
    find_library(IOKIT IOKit)
    find_library(COREVIDEO CoreVideo)
    target_link_libraries(PixelPhys2D ${COCOA} ${IOKIT} ${COREVIDEO})
endif()
