# Locate ngtcp2
# This module defines
#  NGTCP2_FOUND, true if libngtcp2 was located
#  NGTCP2_INCLUDE_DIRS, header search paths
#  NGTCP2_LIBRARIES, libraries to link against
#
# Imported target:
#  Ngtcp2::Ngtcp2
#  Ngtcp2::CryptoOssl

include(FindPackageHandleStandardArgs)

find_path(NGTCP2_INCLUDE_DIR
	NAMES ngtcp2/ngtcp2.h
)

find_library(NGTCP2_LIBRARY
	NAMES ngtcp2
)

find_path(NGTCP2_CRYPTO_OSSL_INCLUDE_DIR
	NAMES ngtcp2/ngtcp2_crypto_ossl.h
)

find_library(NGTCP2_CRYPTO_OSSL_LIBRARY
	NAMES ngtcp2_crypto_ossl
)

find_package_handle_standard_args(Ngtcp2
	REQUIRED_VARS NGTCP2_LIBRARY NGTCP2_INCLUDE_DIR NGTCP2_CRYPTO_OSSL_INCLUDE_DIR NGTCP2_CRYPTO_OSSL_LIBRARY
)

if(NGTCP2_FOUND)
	if(NOT TARGET Ngtcp2::Ngtcp2)
		add_library(Ngtcp2::Ngtcp2 UNKNOWN IMPORTED)
		set_target_properties(Ngtcp2::Ngtcp2 PROPERTIES
			IMPORTED_LOCATION "${NGTCP2_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${NGTCP2_INCLUDE_DIR}"
		)
	endif()

	if(NOT TARGET Ngtcp2::CryptoOssl)
		add_library(Ngtcp2::CryptoOssl UNKNOWN IMPORTED)
		set_target_properties(Ngtcp2::CryptoOssl PROPERTIES
			IMPORTED_LOCATION "${NGTCP2_CRYPTO_OSSL_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${NGTCP2_CRYPTO_OSSL_INCLUDE_DIR}"
		)
	endif()
endif()

mark_as_advanced(NGTCP2_INCLUDE_DIR NGTCP2_LIBRARY NGTCP2_CRYPTO_OSSL_INCLUDE_DIR NGTCP2_CRYPTO_OSSL_LIBRARY)
