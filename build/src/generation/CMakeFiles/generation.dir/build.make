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
include src/generation/CMakeFiles/generation.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/generation/CMakeFiles/generation.dir/compiler_depend.make

# Include the progress variables for this target.
include src/generation/CMakeFiles/generation.dir/progress.make

# Include the compile flags for this target's objects.
include src/generation/CMakeFiles/generation.dir/flags.make

src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/flags.make
src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o: /home/bhayward/Dyg/src/generation/NoiseGenerator.cpp
src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o -MF CMakeFiles/generation.dir/NoiseGenerator.cpp.o.d -o CMakeFiles/generation.dir/NoiseGenerator.cpp.o -c /home/bhayward/Dyg/src/generation/NoiseGenerator.cpp

src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/generation.dir/NoiseGenerator.cpp.i"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/generation/NoiseGenerator.cpp > CMakeFiles/generation.dir/NoiseGenerator.cpp.i

src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/generation.dir/NoiseGenerator.cpp.s"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/generation/NoiseGenerator.cpp -o CMakeFiles/generation.dir/NoiseGenerator.cpp.s

src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/flags.make
src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o: /home/bhayward/Dyg/src/generation/TerrainGenerator.cpp
src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o -MF CMakeFiles/generation.dir/TerrainGenerator.cpp.o.d -o CMakeFiles/generation.dir/TerrainGenerator.cpp.o -c /home/bhayward/Dyg/src/generation/TerrainGenerator.cpp

src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/generation.dir/TerrainGenerator.cpp.i"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/generation/TerrainGenerator.cpp > CMakeFiles/generation.dir/TerrainGenerator.cpp.i

src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/generation.dir/TerrainGenerator.cpp.s"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/generation/TerrainGenerator.cpp -o CMakeFiles/generation.dir/TerrainGenerator.cpp.s

src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/flags.make
src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o: /home/bhayward/Dyg/src/generation/CaveGenerator.cpp
src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o -MF CMakeFiles/generation.dir/CaveGenerator.cpp.o.d -o CMakeFiles/generation.dir/CaveGenerator.cpp.o -c /home/bhayward/Dyg/src/generation/CaveGenerator.cpp

src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/generation.dir/CaveGenerator.cpp.i"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/generation/CaveGenerator.cpp > CMakeFiles/generation.dir/CaveGenerator.cpp.i

src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/generation.dir/CaveGenerator.cpp.s"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/generation/CaveGenerator.cpp -o CMakeFiles/generation.dir/CaveGenerator.cpp.s

src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/flags.make
src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o: /home/bhayward/Dyg/src/generation/BiomeGenerator.cpp
src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o -MF CMakeFiles/generation.dir/BiomeGenerator.cpp.o.d -o CMakeFiles/generation.dir/BiomeGenerator.cpp.o -c /home/bhayward/Dyg/src/generation/BiomeGenerator.cpp

src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/generation.dir/BiomeGenerator.cpp.i"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/generation/BiomeGenerator.cpp > CMakeFiles/generation.dir/BiomeGenerator.cpp.i

src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/generation.dir/BiomeGenerator.cpp.s"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/generation/BiomeGenerator.cpp -o CMakeFiles/generation.dir/BiomeGenerator.cpp.s

src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/flags.make
src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o: /home/bhayward/Dyg/src/generation/StructureGenerator.cpp
src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o: src/generation/CMakeFiles/generation.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o -MF CMakeFiles/generation.dir/StructureGenerator.cpp.o.d -o CMakeFiles/generation.dir/StructureGenerator.cpp.o -c /home/bhayward/Dyg/src/generation/StructureGenerator.cpp

src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/generation.dir/StructureGenerator.cpp.i"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/bhayward/Dyg/src/generation/StructureGenerator.cpp > CMakeFiles/generation.dir/StructureGenerator.cpp.i

src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/generation.dir/StructureGenerator.cpp.s"
	cd /home/bhayward/Dyg/build/src/generation && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/bhayward/Dyg/src/generation/StructureGenerator.cpp -o CMakeFiles/generation.dir/StructureGenerator.cpp.s

# Object files for target generation
generation_OBJECTS = \
"CMakeFiles/generation.dir/NoiseGenerator.cpp.o" \
"CMakeFiles/generation.dir/TerrainGenerator.cpp.o" \
"CMakeFiles/generation.dir/CaveGenerator.cpp.o" \
"CMakeFiles/generation.dir/BiomeGenerator.cpp.o" \
"CMakeFiles/generation.dir/StructureGenerator.cpp.o"

# External object files for target generation
generation_EXTERNAL_OBJECTS =

src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/NoiseGenerator.cpp.o
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/TerrainGenerator.cpp.o
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/CaveGenerator.cpp.o
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/BiomeGenerator.cpp.o
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/StructureGenerator.cpp.o
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/build.make
src/generation/libgeneration.a: src/generation/CMakeFiles/generation.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/bhayward/Dyg/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Linking CXX static library libgeneration.a"
	cd /home/bhayward/Dyg/build/src/generation && $(CMAKE_COMMAND) -P CMakeFiles/generation.dir/cmake_clean_target.cmake
	cd /home/bhayward/Dyg/build/src/generation && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/generation.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/generation/CMakeFiles/generation.dir/build: src/generation/libgeneration.a
.PHONY : src/generation/CMakeFiles/generation.dir/build

src/generation/CMakeFiles/generation.dir/clean:
	cd /home/bhayward/Dyg/build/src/generation && $(CMAKE_COMMAND) -P CMakeFiles/generation.dir/cmake_clean.cmake
.PHONY : src/generation/CMakeFiles/generation.dir/clean

src/generation/CMakeFiles/generation.dir/depend:
	cd /home/bhayward/Dyg/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/bhayward/Dyg /home/bhayward/Dyg/src/generation /home/bhayward/Dyg/build /home/bhayward/Dyg/build/src/generation /home/bhayward/Dyg/build/src/generation/CMakeFiles/generation.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/generation/CMakeFiles/generation.dir/depend

