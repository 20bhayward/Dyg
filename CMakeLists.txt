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
find_package(OpenVDB QUIET)

# Optional packages
option(USE_OPENVDB "Use OpenVDB for volumetric lighting" OFF)
if(USE_OPENVDB AND OpenVDB_FOUND)
    message(STATUS "Building with OpenVDB support")
    add_definitions(-DUSE_OPENVDB)
endif()

# Sky rendering options
option(SIMPLIFIED_SKY "Use simplified sky rendering for better performance" ON)
if(SIMPLIFIED_SKY)
    message(STATUS "Building with simplified sky rendering for better performance")
    add_definitions(-DSIMPLIFIED_SKY)
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include 
)

# SDL2 include directories (support both modern and old-style)
if(TARGET SDL2::SDL2)
    get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
endif()
if(SDL2_INCLUDE_DIRS)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

if(GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIRS})
endif()
if(USE_OPENVDB AND OpenVDB_FOUND)
    include_directories(${OpenVDB_INCLUDE_DIRS})
endif()

# Add executable
add_executable(PixelPhys2D 
    src/main.cpp
    src/World.cpp
    src/Renderer.cpp
    src/UI.cpp        # Added UI implementation
)

# Common libraries for all platforms
target_link_libraries(PixelPhys2D 
    # Support both modern target-based and old variable-based SDL2
    $<$<TARGET_EXISTS:SDL2::SDL2>:SDL2::SDL2>
    $<$<NOT:$<TARGET_EXISTS:SDL2::SDL2>>:${SDL2_LIBRARIES}>
    ${OPENGL_LIBRARIES} 
    ${GLEW_LIBRARIES}
)

if(USE_OPENVDB AND OpenVDB_FOUND)
    target_link_libraries(PixelPhys2D ${OpenVDB_LIBRARIES})
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings and libraries
    target_link_libraries(PixelPhys2D opengl32 glu32)
    
    # Windows does not need pthread - skip that library
    message(STATUS "Building for Windows - skipping pthread")
    
    # Setup for automatic dll copying
    set(REQUIRED_DLLS "")
    
    # Find and copy SDL2.dll
    if(TARGET SDL2::SDL2)
        # Modern CMake target - get the DLL location
        get_target_property(SDL2_LOCATION SDL2::SDL2 LOCATION)
        if(SDL2_LOCATION)
            get_filename_component(SDL2_LIB_DIR ${SDL2_LOCATION} DIRECTORY)
            # Look in standard locations - both regular path and bin directory
            file(GLOB SDL2_DLL 
                "${SDL2_LIB_DIR}/SDL2.dll"
                "${SDL2_LIB_DIR}/../bin/SDL2.dll"
                "${SDL2_LIB_DIR}/../../bin/SDL2.dll"
            )
            if(SDL2_DLL)
                list(APPEND REQUIRED_DLLS ${SDL2_DLL})
            else()
                message(STATUS "SDL2.dll not found automatically. You may need to copy it manually.")
            endif()
        endif()
    elseif(SDL2_LIBRARIES)
        # Old CMake variables style
        get_filename_component(SDL2_LIB_DIR ${SDL2_LIBRARIES} DIRECTORY)
        # Look in standard locations - both regular path and bin directory
        file(GLOB SDL2_DLL 
            "${SDL2_LIB_DIR}/SDL2.dll"
            "${SDL2_LIB_DIR}/../bin/SDL2.dll"
            "${SDL2_LIB_DIR}/../../bin/SDL2.dll"
        )
        if(SDL2_DLL)
            list(APPEND REQUIRED_DLLS ${SDL2_DLL})
        else()
            message(STATUS "SDL2.dll not found automatically. You may need to copy it manually.")
        endif()
    endif()
    
    # Find and copy GLEW.dll - simplified approach
    # Look in standard locations where GLEW DLLs might be
    file(GLOB GLEW_DLL 
        "${CMAKE_PREFIX_PATH}/bin/glew32.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/glew32.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/glew32.dll"
        "${GLEW_INCLUDE_DIRS}/../bin/glew32.dll"
        "${GLEW_INCLUDE_DIRS}/../../bin/glew32.dll"
    )
    if(GLEW_DLL)
        list(APPEND REQUIRED_DLLS ${GLEW_DLL})
        message(STATUS "Found GLEW DLL: ${GLEW_DLL}")
    else()
        message(STATUS "glew32.dll not found automatically. You may need to copy it manually.")
    endif()
    
    # Copy all required DLLs to build directory
    foreach(DLL_FILE ${REQUIRED_DLLS})
        add_custom_command(TARGET PixelPhys2D POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DLL_FILE}
            $<TARGET_FILE_DIR:PixelPhys2D>)
    endforeach()
    
    # Create a batch file to run the game easily
    file(WRITE ${CMAKE_BINARY_DIR}/run_game.bat 
         "@echo off\r\n"
         "echo Starting PixelPhys2D...\r\n"
         "PixelPhys2D.exe\r\n"
         "if ERRORLEVEL 1 pause\r\n")
    
    # Windows build instructions for README.md
    message(STATUS "Windows build configured. After building, run 'run_game.bat' in the build folder.")
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    find_package(Threads REQUIRED)
    target_link_libraries(PixelPhys2D ${CMAKE_THREAD_LIBS_INIT})
    message(STATUS "Building for Linux - linking with ${CMAKE_THREAD_LIBS_INIT}")
elseif(APPLE)
    # macOS-specific settings
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    target_link_libraries(PixelPhys2D ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
    
    # macOS also uses pthreads
    find_package(Threads REQUIRED)
    target_link_libraries(PixelPhys2D ${CMAKE_THREAD_LIBS_INIT})
endif()

add_definitions(-DUSE_GLEW)