if (BUILD_CLIENT)
	set(client_irrlicht_changes_SRCS
		${CMAKE_CURRENT_SOURCE_DIR}/printing.h
		${CMAKE_CURRENT_SOURCE_DIR}/static_text.h
		${CMAKE_CURRENT_SOURCE_DIR}/CGUITTFont.h

		${CMAKE_CURRENT_SOURCE_DIR}/static_text.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CGUITTFont.cpp
	)

	# CMake require us to set a local scope and then parent scope
	# Else the last set win in parent scope
	set(client_irrlicht_changes_SRCS ${client_irrlicht_changes_SRCS} PARENT_SCOPE)
endif()

