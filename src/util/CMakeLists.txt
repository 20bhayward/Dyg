add_library(util
    Config.cpp
    ThreadPool.cpp
    Vector3.cpp
    FileIO.cpp
)

# Make sure all files exist
file(TOUCH Config.cpp ThreadPool.cpp Vector3.cpp FileIO.cpp)

target_include_directories(util PUBLIC ${CMAKE_SOURCE_DIR}/include)