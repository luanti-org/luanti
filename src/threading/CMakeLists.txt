file(GLOB threading_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(threading_SRCS
	${threading_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/thread.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/semaphore.cpp
	PARENT_SCOPE)

