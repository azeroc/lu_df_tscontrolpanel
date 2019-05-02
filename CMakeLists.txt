# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("TSControlPanel")

# Global setup
find_package( Threads )
include_directories(${CMAKE_SOURCE_DIR}/include)

# Include sub-projects.
add_subdirectory("TSControlNode")
add_subdirectory("TSControlServer")
