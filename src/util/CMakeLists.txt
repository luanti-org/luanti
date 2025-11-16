file(GLOB util_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(util_SRCS
	${util_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/areastore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/auth.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base64.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/colorize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/directiontables.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/enriched_string.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/guid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/hashing.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ieee_float.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/metricsbackend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/numeric.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pointedthing.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pointabilities.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/quicktune.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/serialize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sha1.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/string.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/srp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/timetaker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/png.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/enum_string.cpp
	PARENT_SCOPE)
