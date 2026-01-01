file(GLOB quadsphere_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(quadsphere_SRCS
	${quadsphere_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/quadsphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planet_config.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planet_settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sphere_block_pos.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/face_seams.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sphere_mesh_transform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sphere_terrain.cpp
	PARENT_SCOPE)
