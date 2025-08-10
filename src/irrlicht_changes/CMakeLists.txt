if (BUILD_CLIENT)
	file(GLOB client_irrlicht_changes_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

	set(client_irrlicht_changes_SRCS
		${client_irrlicht_changes_HDRS}
		${CMAKE_CURRENT_SOURCE_DIR}/static_text.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CGUITTFont.cpp
	)

	# CMake require us to set a local scope and then parent scope
	# Else the last set win in parent scope
	set(client_irrlicht_changes_SRCS ${client_irrlicht_changes_SRCS} PARENT_SCOPE)
endif()

