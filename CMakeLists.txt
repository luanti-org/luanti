cmake_minimum_required(VERSION 3.12)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

# This can be read from ${PROJECT_NAME} after project() is called
project(luanti)
set(PROJECT_NAME_CAPITALIZED "Luanti")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(GCC_MINIMUM_VERSION "7.5")
set(CLANG_MINIMUM_VERSION "7.0.1")

# You should not need to edit these manually, use util/bump_version.sh
set(VERSION_MAJOR 5)
set(VERSION_MINOR 14)
set(VERSION_PATCH 0)
set(VERSION_EXTRA "" CACHE STRING "Stuff to append to version string")

# Change to false for releases
set(DEVELOPMENT_BUILD FALSE)

set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
if(VERSION_EXTRA)
	set(VERSION_STRING "${VERSION_STRING}-${VERSION_EXTRA}")
elseif(DEVELOPMENT_BUILD)
	set(VERSION_STRING "${VERSION_STRING}-dev")
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Append "-debug" to version string
	set(VERSION_STRING "${VERSION_STRING}-debug")
endif()

# Configuration options
set(BUILD_CLIENT TRUE CACHE BOOL "Build client")
set(BUILD_SERVER FALSE CACHE BOOL "Build server")
set(BUILD_UNITTESTS TRUE CACHE BOOL "Build unittests")
set(BUILD_BENCHMARKS FALSE CACHE BOOL "Build benchmarks")
set(BUILD_DOCUMENTATION TRUE CACHE BOOL "Build documentation")

set(DEFAULT_ENABLE_LTO TRUE)
# by default don't enable on Debug builds to get faster builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DEFAULT_ENABLE_LTO FALSE)
endif()
#### LTO testing list ####
# - Linux: seems to work always
# - win32/msvc: works
# - win32/gcc: fails to link
# - win32/clang: works
# - macOS on x86: seems to be fine
# - macOS on ARM: crashes, see <https://github.com/luanti-org/luanti/issues/14397>
# Note: since CMake has no easy architecture detection disabling for Mac entirely
#### ####
if((WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR APPLE)
	set(DEFAULT_ENABLE_LTO FALSE)
endif()
set(ENABLE_LTO ${DEFAULT_ENABLE_LTO} CACHE BOOL "Use Link Time Optimization")

set(BUILD_WITH_TRACY FALSE CACHE BOOL
	"Fetch and build with the Tracy profiler client")
set(FETCH_TRACY_GIT_TAG "master" CACHE STRING
	"Git tag for fetching Tracy client. Match with your server (gui) version")

set(DEFAULT_RUN_IN_PLACE FALSE)
if(WIN32)
	set(DEFAULT_RUN_IN_PLACE TRUE)
endif()
set(RUN_IN_PLACE ${DEFAULT_RUN_IN_PLACE} CACHE BOOL
	"Run directly in source directory structure")

message(STATUS "*** Will build version ${VERSION_STRING} ***")
message(STATUS "BUILD_CLIENT: " ${BUILD_CLIENT})
message(STATUS "BUILD_SERVER: " ${BUILD_SERVER})
message(STATUS "BUILD_UNITTESTS: " ${BUILD_UNITTESTS})
message(STATUS "BUILD_BENCHMARKS: " ${BUILD_BENCHMARKS})
message(STATUS "BUILD_DOCUMENTATION: " ${BUILD_DOCUMENTATION})
message(STATUS "RUN_IN_PLACE: " ${RUN_IN_PLACE})

set(WARN_ALL TRUE CACHE BOOL "Enable -Wall for Release build")

if(NOT CMAKE_BUILD_TYPE)
	# Default to release
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type: Debug or Release" FORCE)
endif()

set(ENABLE_UPDATE_CHECKER (NOT ${DEVELOPMENT_BUILD}) CACHE BOOL
	"Whether to enable update checks by default")

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Load default options for Android
if(ANDROID)
	cmake_minimum_required(VERSION 3.20)
	include(AndroidLibs)
endif()


if(TRUE)
	message(STATUS "Using imported IrrlichtMt at subdirectory 'irr'")
	if(BUILD_CLIENT)
		add_subdirectory(irr EXCLUDE_FROM_ALL)

		if(NOT TARGET IrrlichtMt)
			message(FATAL_ERROR "IrrlichtMt project is missing a CMake target?!")
		endif()
	else()
		add_library(IrrlichtMt::IrrlichtMt INTERFACE IMPORTED)
		set_target_properties(IrrlichtMt::IrrlichtMt PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/irr/include")
	endif()
endif()

if (ENABLE_LTO OR CMAKE_INTERPROCEDURAL_OPTIMIZATION)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT lto_supported OUTPUT lto_output)
	if(lto_supported)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
		message(STATUS "LTO/IPO is enabled")
	else()
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
		message(STATUS "LTO/IPO was requested but is not supported by the compiler: ${lto_output}")
	endif()
else()
	message(STATUS "LTO/IPO is not enabled")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${GCC_MINIMUM_VERSION}")
		message(FATAL_ERROR "Insufficient gcc version, found ${CMAKE_CXX_COMPILER_VERSION}. "
			"Version ${GCC_MINIMUM_VERSION} or higher is required.")
	endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${CLANG_MINIMUM_VERSION}")
		message(FATAL_ERROR "Insufficient clang version, found ${CMAKE_CXX_COMPILER_VERSION}. "
			"Version ${CLANG_MINIMUM_VERSION} or higher is required.")
	endif()
endif()

# Installation

if(WIN32)
	set(SHAREDIR ".")
	set(BINDIR "bin")
	set(DOCDIR "doc")
	set(EXAMPLE_CONF_DIR ".")
	set(LOCALEDIR "locale")
elseif(APPLE)
	set(BUNDLE_NAME ${PROJECT_NAME}.app)
	set(BUNDLE_PATH "${BUNDLE_NAME}")
	set(BINDIR ${BUNDLE_NAME}/Contents/MacOS)
	set(SHAREDIR ${BUNDLE_NAME}/Contents/Resources)
	set(DOCDIR "${SHAREDIR}/${PROJECT_NAME}")
	set(EXAMPLE_CONF_DIR ${DOCDIR})
	set(LOCALEDIR "${SHAREDIR}/locale")
elseif(UNIX) # Linux, BSD etc
	if(RUN_IN_PLACE)
		set(SHAREDIR ".")
		set(BINDIR "bin")
		set(DOCDIR "doc")
		set(EXAMPLE_CONF_DIR ".")
		set(MANDIR "unix/man")
		set(XDG_APPS_DIR "unix/applications")
		set(METAINFODIR "unix/metainfo")
		set(ICONDIR "unix/icons")
		set(LOCALEDIR "locale")
	else()
		include(GNUInstallDirs)
		set(SHAREDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
		set(BINDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
		set(DOCDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR}")
		set(MANDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}")
		set(EXAMPLE_CONF_DIR ${DOCDIR})
		set(XDG_APPS_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications")
		set(METAINFODIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/metainfo")
		set(ICONDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons")
		set(LOCALEDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALEDIR}")
	endif()
endif()

set(CUSTOM_SHAREDIR "" CACHE STRING "Directory to install data files into")
if(NOT CUSTOM_SHAREDIR STREQUAL "")
	set(SHAREDIR "${CUSTOM_SHAREDIR}")
	message(STATUS "Using SHAREDIR=${SHAREDIR}")
endif()

set(CUSTOM_BINDIR "" CACHE STRING "Directory to install binaries into")
if(NOT CUSTOM_BINDIR STREQUAL "")
	set(BINDIR "${CUSTOM_BINDIR}")
	message(STATUS "Using BINDIR=${BINDIR}")
endif()

set(CUSTOM_DOCDIR "" CACHE STRING "Directory to install documentation into")
if(NOT CUSTOM_DOCDIR STREQUAL "")
	set(DOCDIR "${CUSTOM_DOCDIR}")
	if(NOT RUN_IN_PLACE)
		set(EXAMPLE_CONF_DIR ${DOCDIR})
	endif()
	message(STATUS "Using DOCDIR=${DOCDIR}")
endif()

set(CUSTOM_MANDIR "" CACHE STRING "Directory to install manpages into")
if(NOT CUSTOM_MANDIR STREQUAL "")
	set(MANDIR "${CUSTOM_MANDIR}")
	message(STATUS "Using MANDIR=${MANDIR}")
endif()

set(CUSTOM_EXAMPLE_CONF_DIR "" CACHE STRING "Directory to install example config file into")
if(NOT CUSTOM_EXAMPLE_CONF_DIR STREQUAL "")
	set(EXAMPLE_CONF_DIR "${CUSTOM_EXAMPLE_CONF_DIR}")
	message(STATUS "Using EXAMPLE_CONF_DIR=${EXAMPLE_CONF_DIR}")
endif()

set(CUSTOM_XDG_APPS_DIR "" CACHE STRING "Directory to install .desktop files into")
if(NOT CUSTOM_XDG_APPS_DIR STREQUAL "")
	set(XDG_APPS_DIR "${CUSTOM_XDG_APPS_DIR}")
	message(STATUS "Using XDG_APPS_DIR=${XDG_APPS_DIR}")
endif()

set(CUSTOM_ICONDIR "" CACHE STRING "Directory to install icons into")
if(NOT CUSTOM_ICONDIR STREQUAL "")
	set(ICONDIR "${CUSTOM_ICONDIR}")
	message(STATUS "Using ICONDIR=${ICONDIR}")
endif()

set(CUSTOM_LOCALEDIR "" CACHE STRING "Directory to install l10n files into")
if(NOT CUSTOM_LOCALEDIR STREQUAL "")
	set(LOCALEDIR "${CUSTOM_LOCALEDIR}")
	message(STATUS "Using LOCALEDIR=${LOCALEDIR}")
endif()


install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/builtin" DESTINATION "${SHAREDIR}")
if(RUN_IN_PLACE)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/mods/mods_here.txt" DESTINATION "${SHAREDIR}/mods")
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/textures/texture_packs_here.txt" DESTINATION "${SHAREDIR}/textures")
endif()

set(INSTALL_DEVTEST FALSE CACHE BOOL "Install Development Test")

if(INSTALL_DEVTEST)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/games/devtest" DESTINATION "${SHAREDIR}/games/"
		PATTERN ".git*" EXCLUDE )
endif()

if(BUILD_CLIENT)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/client/shaders" DESTINATION "${SHAREDIR}/client")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/textures/base/pack" DESTINATION "${SHAREDIR}/textures/base")
	if(RUN_IN_PLACE)
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/clientmods" DESTINATION "${SHAREDIR}")
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/client/serverlist" DESTINATION "${SHAREDIR}/client")
	endif()
endif()

install(FILES "README.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/client_lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/menu_lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/texture_packs.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/world_format.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "minetest.conf.example" DESTINATION "${EXAMPLE_CONF_DIR}")

if(UNIX AND NOT APPLE)
	install(FILES "doc/luanti.6" "doc/luantiserver.6" DESTINATION "${MANDIR}/man6")
	install(FILES "misc/org.luanti.luanti.desktop" DESTINATION "${XDG_APPS_DIR}")
	install(FILES "misc/org.luanti.luanti.metainfo.xml" DESTINATION "${METAINFODIR}")
	install(FILES "misc/luanti.svg" DESTINATION "${ICONDIR}/hicolor/scalable/apps")
	install(FILES "misc/luanti-xorg-icon-128.png"
		DESTINATION "${ICONDIR}/hicolor/128x128/apps"
		RENAME "luanti.png")
endif()

if(APPLE)
	install(FILES "misc/luanti-icon.icns" DESTINATION "${SHAREDIR}")
	install(FILES "${CMAKE_BINARY_DIR}/Info.plist" DESTINATION "${BUNDLE_PATH}/Contents")
endif()

if(CMAKE_GENERATOR STREQUAL "Xcode")
	set(client_RESOURCES "${CMAKE_SOURCE_DIR}/misc/luanti-icon.icns")
endif()

# Library pack
find_package(GMP REQUIRED)
find_package(Json 1.0.0 REQUIRED)
find_package(Lua REQUIRED)
if(NOT USE_LUAJIT)
	add_subdirectory(lib/bitop)
endif()
add_subdirectory(lib/sha256)

if(BUILD_UNITTESTS OR BUILD_BENCHMARKS)
	add_subdirectory(lib/catch2)
endif()

add_subdirectory(lib/tiniergltf)

# Subdirectories
# Be sure to add all relevant definitions above this
add_subdirectory(src)

# CPack

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A free open-source voxel game engine with easy modding and game creation.")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR "celeron55")
set(CPACK_PACKAGE_CONTACT "Perttu Ahola <celeron55@gmail.com>")

include(CPackComponent)

cpack_add_component(Docs
	DISPLAY_NAME "Documentation"
	DESCRIPTION "Documentation about ${PROJECT_NAME_CAPITALIZED} and ${PROJECT_NAME_CAPITALIZED} modding"
)

if(WIN32)
	# Include all dynamically linked runtime libraries such as MSVCRxxx.dll
	include(InstallRequiredSystemLibraries)

	if(RUN_IN_PLACE)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-win64")
		else()
			set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-win32")
		endif()

		set(CPACK_GENERATOR ZIP)

	else()
		set(CPACK_GENERATOR WIX)
		set(CPACK_PACKAGE_NAME "${PROJECT_NAME_CAPITALIZED}")
		set(CPACK_PACKAGE_INSTALL_DIRECTORY ".")
		set(CPACK_PACKAGE_EXECUTABLES ${PROJECT_NAME} "${PROJECT_NAME_CAPITALIZED}")
		set(CPACK_CREATE_DESKTOP_LINKS ${PROJECT_NAME})
		set(CPACK_PACKAGING_INSTALL_PREFIX "/${PROJECT_NAME_CAPITALIZED}")

		set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/misc/luanti-icon.ico")
		# Supported languages can be found at
		# http://wixtoolset.org/documentation/manual/v3/wixui/wixui_localization.html
		#set(CPACK_WIX_CULTURES "ar-SA,bg-BG,ca-ES,hr-HR,cs-CZ,da-DK,nl-NL,en-US,et-EE,fi-FI,fr-FR,de-DE")
		set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/misc/CPACK_WIX_UI_BANNER.BMP")
		set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/misc/CPACK_WIX_UI_DIALOG.BMP")

		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/doc/lgpl-2.1.txt")

		# The correct way would be to include both x32 and x64 into one installer
		# and install the appropriate one.
		# CMake does not support that, so there are two separate GUID's
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(CPACK_WIX_UPGRADE_GUID "745A0FB3-5552-44CA-A587-A91C397CCC56")
		else()
			set(CPACK_WIX_UPGRADE_GUID "814A2E2D-2779-4BBD-9ACD-FC3BD51FBBA2")
		endif()
	endif()
elseif(APPLE)
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-osx")
	set(CPACK_GENERATOR ZIP)
else()
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-linux")
	set(CPACK_GENERATOR TGZ)
	set(CPACK_SOURCE_GENERATOR TGZ)
endif()

include(CPack)


# Add a target to generate API documentation with Doxygen
if(BUILD_DOCUMENTATION)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
				${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile @ONLY)
		add_custom_target(doc
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc
			COMMENT "Generating API documentation with Doxygen" VERBATIM
		)
	endif()
endif()

# Fetch Tracy
if(BUILD_WITH_TRACY)
	include(FetchContent)

	message(STATUS "Fetching Tracy (${FETCH_TRACY_GIT_TAG})...")
	FetchContent_Declare(
		tracy
		GIT_REPOSITORY https://github.com/wolfpld/tracy.git
		GIT_TAG ${FETCH_TRACY_GIT_TAG}
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
	)
	FetchContent_MakeAvailable(tracy)
	message(STATUS "Fetching Tracy - done")
endif()
