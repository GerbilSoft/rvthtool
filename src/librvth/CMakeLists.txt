PROJECT(librvth)

# Check for C library functions.
IF(NOT WIN32)
	INCLUDE(CheckFunctionExists)
	CHECK_FUNCTION_EXISTS(ftruncate HAVE_FTRUNCATE)
ENDIF(NOT WIN32)

# Find GMP and nettle.
# FIXME: Windows equivalent.
IF(NOT WIN32)
	FIND_PACKAGE(GMP REQUIRED)
	SET(HAVE_GMP 1)

	FIND_PACKAGE(Nettle REQUIRED)
	SET(HAVE_NETTLE 1)
	# Check if this is Nettle 3.x.
	# Nettle 3.1 added version.h, which isn't available
	# in older verisons, so we can't simply check that.
	INCLUDE(CheckSymbolExists)
	CHECK_SYMBOL_EXISTS(aes128_set_decrypt_key "nettle/aes.h" HAVE_NETTLE_3)
	IF(HAVE_NETTLE_3)
		# Check for Nettle versioning symbols.
		# Nettle 3.1 added version.h.
		SET(OLD_CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
		SET(OLD_CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
		SET(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES} ${NETTLE_INCLUDE_DIRS}")
		SET(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${NETTLE_LIBRARY}")
		CHECK_SYMBOL_EXISTS(NETTLE_VERSION_MAJOR "nettle/version.h" HAVE_NETTLE_VERSION_H)
		CHECK_SYMBOL_EXISTS(nettle_version_major "nettle/version.h" HAVE_NETTLE_VERSION_FUNCTIONS)
		SET(CMAKE_REQUIRED_INCLUDES "${OLD_CMAKE_REQUIRED_INCLUDES}")
		SET(CMAKE_REQUIRED_LIBRARIES "${OLD_CMAKE_REQUIRED_LIBRARIES}")
		UNSET(OLD_CMAKE_REQUIRED_INCLUDES)
		UNSET(OLD_CMAKE_REQUIRED_LIBRARIES)
	ENDIF(HAVE_NETTLE_3)
ENDIF(NOT WIN32)

# Write the config.h file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.librvth.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.librvth.h")

# Sources.
SET(librvth_SRCS
	rvth.c
	rvth_p.c
	rvth_write.c
	rvth_extract.c
	ref_file.c
	reader_plain.c
	cert_store.c
	cert.c
	)
# Headers.
SET(librvth_H
	common.h
	byteorder.h
	byteswap.h
	nhcd_structs.h
	gcn_structs.h
	rvth.h
	rvth_p.h
	ref_file.h
	tcharx.h
	reader.h
	reader_plain.h
	cert_store.h
	cert.h
	rsaw.h
	)

IF(HAVE_GMP)
	SET(librvth_RSA_SRCS rsaw_gmp.c)
ELSE(HAVE_GMP)
	SET(librvth_RSA_SRCS rsaw_generic.c)
ENDIF(HAVE_GMP)

IF(HAVE_NETTLE)
	SET(librvth_HASH_SRCS hashw_nettle.c)
ELSEIF(WIN32)
	SET(librvth_HASH_SRCS hashw_capi.c)
ELSE()
	MESSAGE(FATAL_ERROR "No hash function wrappers for this platform.")
ENDIF()

######################
# Build the library. #
######################

ADD_LIBRARY(rvth STATIC
	${librvth_SRCS} ${librvth_H}
	${librvth_RSA_SRCS}
	${librvth_HASH_SRCS}
	)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(rvth
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>		# librvth
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>		# librvth
	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>			# build
	)
# Exclude from ALL builds.
SET_TARGET_PROPERTIES(rvth PROPERTIES EXCLUDE_FROM_ALL TRUE)
# Make sure git_version.h is created before compiling this target.
IF(TARGET git_version)
	ADD_DEPENDENCIES(rvth git_version)
ENDIF(TARGET git_version)

# GMP
# TODO: Detect nettle.
IF(HAVE_GMP)
	TARGET_INCLUDE_DIRECTORIES(rvth PRIVATE ${GMP_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(rvth ${GMP_LIBRARIES} -lnettle)
ENDIF(HAVE_GMP)

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)

# Test suite.
IF(BUILD_TESTING)
	ADD_SUBDIRECTORY(tests)
ENDIF(BUILD_TESTING)
