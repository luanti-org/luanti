mark_as_advanced(ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

message(STATUS "ZSTD_LIBRARY: ${ZSTD_LIBRARY}")
message(STATUS "ZSTD_INCLUDE_DIR: ${ZSTD_INCLUDE_DIR}")

find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)

find_library(ZSTD_LIBRARY NAMES zstd)

if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
	# Check that the API we use exists
	include(CheckSymbolExists)
	unset(HAVE_ZSTD_INITCSTREAM CACHE)
	set(CMAKE_REQUIRED_INCLUDES ${ZSTD_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${ZSTD_LIBRARY})
	check_symbol_exists(ZSTD_initCStream zstd.h HAVE_ZSTD_INITCSTREAM)
	unset(CMAKE_REQUIRED_INCLUDES)
	unset(CMAKE_REQUIRED_LIBRARIES)

	if(NOT HAVE_ZSTD_INITCSTREAM)
		unset(ZSTD_INCLUDE_DIR CACHE)
		unset(ZSTD_LIBRARY CACHE)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zstd DEFAULT_MSG ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

