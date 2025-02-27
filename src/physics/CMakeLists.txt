add_library(physics
    PhysicsSimulator.cpp
)

# Make sure all files exist
file(TOUCH PhysicsSimulator.cpp)

target_include_directories(physics PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(physics PRIVATE core util)