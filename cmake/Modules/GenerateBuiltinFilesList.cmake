# Compute sha256 digests of builtin files

set(BUILTIN_SHA256S "")
foreach(P_REL ${BUILTIN_SRCS})
	set(P_ABS "${BUILTIN_BASE_PATH}/${P_REL}")

	file(SHA256 ${P_ABS} H)

	list(APPEND BUILTIN_SHA256S "{\"${P_REL}\", \"${H}\"}")
endforeach()

list(JOIN BUILTIN_SHA256S ",\n" BUILTIN_SHA256S_INITIALIZER_LIST)

configure_file(
	"${PROJECT_SOURCE_DIR}/builtin_files.cpp.in"
	"${PROJECT_BINARY_DIR}/builtin_files.cpp"
)
# touch it, because configure_file doesn't if it doesn't change
file(TOUCH_NOCREATE "${PROJECT_BINARY_DIR}/builtin_files.cpp")
