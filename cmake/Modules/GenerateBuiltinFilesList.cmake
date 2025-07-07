message(WARNING "hi!!!!!!!!!!!!!!!!!!!!!!!!!!")

string(LENGTH "${BUILTIN_BASE_PATH}" BUILTIN_BASE_PATH_LEN)
message(WARNING "BASE_PATH: ${BUILTIN_BASE_PATH}")

set(builtin_SHA256S "")
foreach(P ${builtin_SRCS})
	# execute_process(COMMAND ${CMAKE_COMMAND} -E sha256sum ${P} OUTPUT_VARIABLE H)
	file(SHA256 ${P} H)
	# message(WARNING "${BUILTIN_BASE_PATH_LEN} - -1 ${P} -> ${RP}")
	string(SUBSTRING "${P}" ${BUILTIN_BASE_PATH_LEN} -1 RP)
	string(SUBSTRING "${P}" 0 ${BUILTIN_BASE_PATH_LEN} BP)
	if(NOT (BP STREQUAL BUILTIN_BASE_PATH))
		message(FATAL_ERROR "eh")
	endif()
	list(APPEND builtin_SHA256S "{\"${RP}\", \"${H}\"}")
endforeach()

list(JOIN builtin_SHA256S ",\n" builtin_SHA256S_INITIALIZER_LIST)

configure_file(
	"${PROJECT_SOURCE_DIR}/builtin_files.cpp.in"
	"${PROJECT_BINARY_DIR}/builtin_files.cpp"
)
# touch it, because configure_file doesn't if it doesn't change
file(TOUCH_NOCREATE "${PROJECT_BINARY_DIR}/builtin_files.cpp")
