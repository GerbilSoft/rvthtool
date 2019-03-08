PROJECT(src)

# C++11 compatibility header.
# NOTE: This must be included regardless of C++11 support in the compiler.
# gcc-4.6 supports some C++11, but is missing explicit virtual overrides.
IF(MSVC)
	SET(CXX11_COMPAT_HEADER "-FI${CMAKE_CURRENT_SOURCE_DIR}/c++11-compat.h")
ELSE(MSVC)
	SET(CXX11_COMPAT_HEADER "-include ${CMAKE_CURRENT_SOURCE_DIR}/c++11-compat.h")
ENDIF(MSVC)

# Add the C++11 compatibility header to CFLAGS/CXXFLAGS.
SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} ${CXX11_COMPAT_HEADER}")
SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} ${CXX11_COMPAT_HEADER}")

# Check for hidden visibility flags.
INCLUDE(CheckHiddenVisibility)
CHECK_HIDDEN_VISIBILITY()

# Find nettle.
FIND_PACKAGE(Nettle REQUIRED)
SET(HAVE_NETTLE 1)
IF(NOT WIN32)
	# Find GMP.
	# On Windows, we're using nettle's mini-GMP.
	FIND_PACKAGE(GMP REQUIRED)
	SET(HAVE_GMP 1)
ENDIF(NOT WIN32)

# Check if this is Nettle 3.x.
# Nettle 3.1 added version.h, which isn't available
# in older verisons, so we can't simply check that.
INCLUDE(CheckSymbolExists)
SET(OLD_CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
SET(OLD_CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
SET(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES} ${NETTLE_INCLUDE_DIRS}")
SET(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${NETTLE_LIBRARY}")
CHECK_SYMBOL_EXISTS(aes128_set_decrypt_key "nettle/aes.h" HAVE_NETTLE_3)
IF(HAVE_NETTLE_3)
	# Check for Nettle versioning symbols.
	# Nettle 3.1 added version.h.
	CHECK_SYMBOL_EXISTS(NETTLE_VERSION_MAJOR "nettle/version.h" HAVE_NETTLE_VERSION_H)
	CHECK_SYMBOL_EXISTS(nettle_version_major "nettle/version.h" HAVE_NETTLE_VERSION_FUNCTIONS)
ENDIF(HAVE_NETTLE_3)
SET(CMAKE_REQUIRED_INCLUDES "${OLD_CMAKE_REQUIRED_INCLUDES}")
SET(CMAKE_REQUIRED_LIBRARIES "${OLD_CMAKE_REQUIRED_LIBRARIES}")
UNSET(OLD_CMAKE_REQUIRED_INCLUDES)
UNSET(OLD_CMAKE_REQUIRED_LIBRARIES)

# Write the version number to config.version.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.version.h")
# Write the Nettle configuration to config.nettle.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.nettle.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.nettle.h")

# Source Code subdirectories.
ADD_SUBDIRECTORY(libwiicrypto)
ADD_SUBDIRECTORY(librvth)
ADD_SUBDIRECTORY(rvthtool)
ADD_SUBDIRECTORY(qrvthtool)
ADD_SUBDIRECTORY(wadresign)
