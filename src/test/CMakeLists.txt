if(BUILD_CLIENT)
	add_subdirectory(client)

	set(test_client_SRCS
		${test_client_SRCS}
		PARENT_SCOPE)
endif()

set(test_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/test.h
	${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_irr_matrix4.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_irr_rotation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_k_d_tree.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_nodedef.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_serveractiveobjectmgr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_translations.cpp
)

if(USE_NGTCP2_QUIC)
	list(APPEND test_SRCS
		${CMAKE_CURRENT_SOURCE_DIR}/test_quic_address.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_quic_tls.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_quic_connection.cpp
	)
endif()

set(test_SRCS ${test_SRCS} PARENT_SCOPE)
