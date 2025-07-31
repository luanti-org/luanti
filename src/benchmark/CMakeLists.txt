set (BENCHMARK_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark.h

	${CMAKE_CURRENT_SOURCE_DIR}/benchmark.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_activeobjectmgr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_lighting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_serialize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_mapblock.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_map.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_mapmodify.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/benchmark_sha.cpp
	PARENT_SCOPE)

set (BENCHMARK_CLIENT_SRCS
	PARENT_SCOPE)
