
# This is only one working solution I found to be working for normal and Archive builds under Xcode 15.4
# I expect higger sensitivity to Xcode version.

if(DEFINED ENV{INSTALL_ROOT} AND EXISTS "$ENV{INSTALL_ROOT}")
	set(RESOURCES_DIR "$ENV{INSTALL_ROOT}/Applications/$ENV{PRODUCT_NAME}.app/Contents/Resources")
else()
	set(RESOURCES_DIR "$ENV{TARGET_BUILD_DIR}/$ENV{UNLOCALIZED_RESOURCES_FOLDER_PATH}")
endif()

# Write debug information to a file
#file(WRITE "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "INSTALL_ROOT: $ENV{INSTALL_ROOT}\n")
#file(APPEND "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "RESOURCES_DIR: ${RESOURCES_DIR}\n")
#file(APPEND "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "TARGET_BUILD_DIR: $ENV{TARGET_BUILD_DIR}\n")
#file(APPEND "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "BUILT_PRODUCTS_DIR: $ENV{BUILT_PRODUCTS_DIR}\n")
#file(APPEND "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "SOURCE_ROOT: $ENV{SOURCE_ROOT}\n")
#file(APPEND "$ENV{PROJECT_FILE_PATH}/../debug_output.txt" "PRODUCT_NAME: $ENV{PRODUCT_NAME}\n")

execute_process(
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"$ENV{SOURCE_ROOT}/builtin"
	"${RESOURCES_DIR}/builtin"
)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"$ENV{SOURCE_ROOT}/client/shaders"
	"${RESOURCES_DIR}/client/shaders"
)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"$ENV{SOURCE_ROOT}/fonts"
	"${RESOURCES_DIR}/fonts"
)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"$ENV{PROJECT_FILE_PATH}/../locale"
	"${RESOURCES_DIR}/locale"
)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory
	"${RESOURCES_DIR}/$ENV{PRODUCT_NAME}"
)
set(RESOURCE_LUANTI_FILES
	"$ENV{SOURCE_ROOT}/README.md"
	"$ENV{SOURCE_ROOT}/doc/client_lua_api.md"
	"$ENV{SOURCE_ROOT}/doc/lua_api.md"
	"$ENV{SOURCE_ROOT}/doc/menu_lua_api.md"
	"$ENV{SOURCE_ROOT}/minetest.conf.example"
	"$ENV{SOURCE_ROOT}/doc/texture_packs.md"
	"$ENV{SOURCE_ROOT}/doc/world_format.md"
)
foreach (file ${RESOURCE_LUANTI_FILES})
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy
		"${file}"
		"${RESOURCES_DIR}/$ENV{PRODUCT_NAME}/"
	)
endforeach()
execute_process(
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"$ENV{SOURCE_ROOT}/textures/base/pack"
	"${RESOURCES_DIR}/textures/base/pack"
)
