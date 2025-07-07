# Compute sha256 digests of builtin files

string(LENGTH "${BUILTIN_BASE_PATH}" BUILTIN_BASE_PATH_LEN)

set(BUILTIN_SHA256S "")
foreach(P ${BUILTIN_SRCS})
	file(SHA256 ${P} H)

	string(SUBSTRING "${P}" 0 ${BUILTIN_BASE_PATH_LEN} BP)
	if(NOT (BP STREQUAL BUILTIN_BASE_PATH))
		message(FATAL_ERROR "Expected ${P} to be a subpath of ${BUILTIN_BASE_PATH}")
	endif()
	string(SUBSTRING "${P}" ${BUILTIN_BASE_PATH_LEN} -1 RP)

	list(APPEND BUILTIN_SHA256S "{\"${RP}\", \"${H}\"}")
endforeach()

list(JOIN BUILTIN_SHA256S ",\n" BUILTIN_SHA256S_INITIALIZER_LIST)

configure_file(
	"${PROJECT_SOURCE_DIR}/builtin_files.cpp.in"
	"${PROJECT_BINARY_DIR}/builtin_files.cpp"
)
# touch it, because configure_file doesn't if it doesn't change
file(TOUCH_NOCREATE "${PROJECT_BINARY_DIR}/builtin_files.cpp")
