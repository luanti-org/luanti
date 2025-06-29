set(database_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/database.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-dummy.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-files.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-leveldb.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-postgresql.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-redis.h
	${CMAKE_CURRENT_SOURCE_DIR}/database-sqlite3.h

	${CMAKE_CURRENT_SOURCE_DIR}/database.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-dummy.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-files.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-leveldb.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-postgresql.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-redis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database-sqlite3.cpp
	PARENT_SCOPE
)
