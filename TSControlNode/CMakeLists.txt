# CMakeList.txt : CMake project for TSControlNode, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable(TSControlNode 
    main.c
	framebuffer.c
	touch_io.c
)
target_link_libraries(TSControlNode PRIVATE ts)
