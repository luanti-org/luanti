file(GLOB mapgen_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(mapgen_SRCS
	${mapgen_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/cavegen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/dungeongen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_carpathian.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_flat.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_fractal.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_singlenode.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_v5.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_v6.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_v7.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mapgen_valleys.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mg_biome.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mg_decoration.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mg_ore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mg_schematic.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/treegen.cpp
	PARENT_SCOPE
)
