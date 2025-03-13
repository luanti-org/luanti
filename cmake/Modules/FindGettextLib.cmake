# This module find everything related to Gettext:
# * development tools (msgfmt)
# * libintl for runtime usage

find_program(GETTEXT_MSGFMT
	NAMES msgfmt
	DOC "Path to Gettext msgfmt")

if(GETTEXT_INCLUDE_DIR AND GETTEXT_LIBRARY)
	# This is only really used on Windows
	find_path(GETTEXT_INCLUDE_DIR NAMES libintl.h)
	find_library(GETTEXT_LIBRARY NAMES intl)

	set(GETTEXT_REQUIRED_VARS GETTEXT_INCLUDE_DIR GETTEXT_LIBRARY GETTEXT_MSGFMT)
else()
	find_package(Intl)
	set(GETTEXT_INCLUDE_DIR ${Intl_INCLUDE_DIRS})
	set(GETTEXT_LIBRARY ${Intl_LIBRARIES})

	# Because intl may be part of the libc it's valid for the two variables to
	# be empty, therefore we can't just put them into GETTEXT_REQUIRED_VARS.
	if(Intl_FOUND)
		set(GETTEXT_REQUIRED_VARS GETTEXT_MSGFMT)
	else()
		set(GETTEXT_REQUIRED_VARS _LIBINTL_WAS_NOT_FOUND)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GettextLib DEFAULT_MSG ${GETTEXT_REQUIRED_VARS})

if(GETTEXTLIB_FOUND)
	# Set up paths for building
	set(GETTEXT_PO_PATH ${CMAKE_SOURCE_DIR}/po)
	# If the executable is expected to be ran from <source dir>/bin/, also
	# generate the locale in <source dir>/locale/.
	if(RUN_IN_PLACE AND NOT CMAKE_CROSSCOMPILING)
		set(GETTEXT_MO_BUILD_PATH ${CMAKE_SOURCE_DIR}/locale/<locale>/LC_MESSAGES)
	else()
		set(GETTEXT_MO_BUILD_PATH ${CMAKE_BINARY_DIR}/locale/<locale>/LC_MESSAGES)
	endif()
	set(GETTEXT_MO_DEST_PATH ${LOCALEDIR}/<locale>/LC_MESSAGES)
	file(GLOB GETTEXT_AVAILABLE_LOCALES RELATIVE ${GETTEXT_PO_PATH} "${GETTEXT_PO_PATH}/*")
	list(REMOVE_ITEM GETTEXT_AVAILABLE_LOCALES ${PROJECT_NAME}.pot)
	list(REMOVE_ITEM GETTEXT_AVAILABLE_LOCALES timestamp)
	macro(SET_MO_PATHS _buildvar _destvar _locale)
		string(REPLACE "<locale>" ${_locale} ${_buildvar} ${GETTEXT_MO_BUILD_PATH})
		string(REPLACE "<locale>" ${_locale} ${_destvar} ${GETTEXT_MO_DEST_PATH})
	endmacro()
endif()

