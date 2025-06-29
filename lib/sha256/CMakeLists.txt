project(sha256 C)

add_library(sha256 STATIC sha256.c my_sha256.h)

target_include_directories(sha256 INTERFACE .)

INCLUDE(CheckIncludeFiles)
check_include_files(endian.h HAVE_ENDIAN_H)

configure_file(
	"${PROJECT_SOURCE_DIR}/cmake_config.h.in"
	"${PROJECT_BINARY_DIR}/cmake_config.h"
)
target_include_directories(sha256 PRIVATE "${PROJECT_BINARY_DIR}")
