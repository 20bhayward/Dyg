# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/bhayward/Dyg

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/bhayward/Dyg/build

# Include any dependencies generated for this target.
include CMakeFiles/PixelPhys2D.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/PixelPhys2D.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/PixelPhys2D.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/PixelPhys2D.dir/flags.make

CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o: /home/bhayward/Dyg/src/main_vulkan.cpp
CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o -c /home/bhayward/Dyg/src/main_vulkan.cpp

CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/main_vulkan.cpp > CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.i

CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/main_vulkan.cpp -o CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.s

CMakeFiles/PixelPhys2D.dir/src/World.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/World.cpp.o: /home/bhayward/Dyg/src/World.cpp
CMakeFiles/PixelPhys2D.dir/src/World.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/World.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/World.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/World.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/World.cpp.o -c /home/bhayward/Dyg/src/World.cpp

CMakeFiles/PixelPhys2D.dir/src/World.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/World.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/World.cpp > CMakeFiles/PixelPhys2D.dir/src/World.cpp.i

CMakeFiles/PixelPhys2D.dir/src/World.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/World.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/World.cpp -o CMakeFiles/PixelPhys2D.dir/src/World.cpp.s

CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o: /home/bhayward/Dyg/src/Renderer.cpp
CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o -c /home/bhayward/Dyg/src/Renderer.cpp

CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/Renderer.cpp > CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.i

CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/Renderer.cpp -o CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.s

CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o: /home/bhayward/Dyg/src/VulkanBackend.cpp
CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o -c /home/bhayward/Dyg/src/VulkanBackend.cpp

CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/VulkanBackend.cpp > CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.i

CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/VulkanBackend.cpp -o CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.s

CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o: /home/bhayward/Dyg/src/RenderBackend.cpp
CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o -c /home/bhayward/Dyg/src/RenderBackend.cpp

CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/RenderBackend.cpp > CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.i

CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/RenderBackend.cpp -o CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.s

CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o: /home/bhayward/Dyg/src/ChunkManager.cpp
CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o -c /home/bhayward/Dyg/src/ChunkManager.cpp

CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/ChunkManager.cpp > CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.i

CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/ChunkManager.cpp -o CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.s

CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o: CMakeFiles/PixelPhys2D.dir/flags.make
CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o: /home/bhayward/Dyg/src/Character.cpp
CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o: CMakeFiles/PixelPhys2D.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o -MF CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o.d -o CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o -c /home/bhayward/Dyg/src/Character.cpp

CMakeFiles/PixelPhys2D.dir/src/Character.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/PixelPhys2D.dir/src/Character.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/Character.cpp > CMakeFiles/PixelPhys2D.dir/src/Character.cpp.i

CMakeFiles/PixelPhys2D.dir/src/Character.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/PixelPhys2D.dir/src/Character.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/Character.cpp -o CMakeFiles/PixelPhys2D.dir/src/Character.cpp.s

# Object files for target PixelPhys2D
PixelPhys2D_OBJECTS = \
"CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/World.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o" \
"CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o"

# External object files for target PixelPhys2D
PixelPhys2D_EXTERNAL_OBJECTS =

PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/main_vulkan.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/World.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/Renderer.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/VulkanBackend.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/RenderBackend.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/ChunkManager.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/src/Character.cpp.o
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/build.make
PixelPhys2D: /usr/lib/x86_64-linux-gnu/libSDL2.so
PixelPhys2D: /usr/lib/x86_64-linux-gnu/libGLEW.so
PixelPhys2D: /usr/lib/x86_64-linux-gnu/libvulkan.so
PixelPhys2D: CMakeFiles/PixelPhys2D.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX executable PixelPhys2D"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/PixelPhys2D.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/PixelPhys2D.dir/build: PixelPhys2D
.PHONY : CMakeFiles/PixelPhys2D.dir/build

CMakeFiles/PixelPhys2D.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/PixelPhys2D.dir/cmake_clean.cmake
.PHONY : CMakeFiles/PixelPhys2D.dir/clean

CMakeFiles/PixelPhys2D.dir/depend:
	cd /home/bhayward/Dyg/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/bhayward/Dyg /home/bhayward/Dyg /home/bhayward/Dyg/build /home/bhayward/Dyg/build /home/bhayward/Dyg/build/CMakeFiles/PixelPhys2D.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/PixelPhys2D.dir/depend

