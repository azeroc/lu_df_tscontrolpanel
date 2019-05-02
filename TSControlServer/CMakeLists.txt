# CMakeList.txt : CMake project for TSControlNode, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# === C GLUE API example ===
add_executable(tstCAPIGlue
	vbox_api/samples/tstCAPIGlue.c
	
	# VBox API
	vbox_api/glue/VBoxCAPIGlue.c
	vbox_api/xpcom/lib/VirtualBox_i.c
)
target_include_directories(tstCAPIGlue
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/glue
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/include
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/xpcom/include
)
target_link_libraries(tstCAPIGlue dl ${CMAKE_THREAD_LIBS_INIT})

# Add source to this project's executable.
add_executable(TSControlServer 
    main.c
	server.c
	vbox.c
	vbox_event.c
	vbox_mouse.c

	# VBox API
	vbox_api/glue/VBoxCAPIGlue.c
	vbox_api/xpcom/lib/VirtualBox_i.c
)
target_include_directories(TSControlServer
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/glue
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/include
	PRIVATE ${CMAKE_SOURCE_DIR}/TSControlServer/vbox_api/xpcom/include
)
target_link_libraries(TSControlServer m dl ${CMAKE_THREAD_LIBS_INIT})

# === Group targets ===
add_custom_target(server_targets)
add_dependencies(server_targets tstCAPIGlue TSControlServer)