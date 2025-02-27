cmake_minimum_required(VERSION 3.14)
project(Dyg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(include)

# Add subdirectories
add_subdirectory(src)

# Main executable
add_executable(dyg src/main.cpp)
target_link_libraries(dyg PRIVATE core generation physics util)

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(dyg PRIVATE Threads::Threads)