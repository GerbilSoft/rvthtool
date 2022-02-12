# libwiicrypto: Wii encryption library
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(libwiicrypto LANGUAGES C)

# Check for C library functions.
IF(NOT WIN32)
	INCLUDE(CheckFunctionExists)
	CHECK_FUNCTION_EXISTS(ftruncate HAVE_FTRUNCATE)
ENDIF(NOT WIN32)

# Sources.
SET(libwiicrypto_SRCS
	cert_store.c
	cert.c
	priv_key_store.c
	sig_tools.c
	title_key.c
	)
# Headers.
SET(libwiicrypto_H
	common.h
	byteorder.h
	byteswap.h
	gcn_structs.h
	cert_store.h
	cert.h
	rsaw.h
	aesw.h
	priv_key_store.h
	sig_tools.h
	wii_sector.h
	title_key.h
	)

IF(WIN32)
	SET(libwiicrypto_H ${libwiicrypto_H}
		win32/Win32_sdk.h
		win32/secoptions.h
		win32/secoptions_win8.h
		)
ENDIF(WIN32)

IF(HAVE_NETTLE)
	SET(libwiicrypto_RSA_SRCS rsaw_nettle.c)
	SET(libwiicrypto_AES_SRCS aesw_nettle.c)
ELSE()
	MESSAGE(FATAL_ERROR "No crypto wrappers are available for this platform.")
ENDIF()

#########
# bin2h #
#########

ADD_EXECUTABLE(bin2h bin2h.c)

################
# Certificates #
################

SET(certs_FILES
	certs/Root-dpki.cert
	certs/Root-ppki.cert

	# Wii: dpki
	certs/Root-CA00000002.cert
	certs/Root-CA00000002-XS00000006.cert
	certs/Root-CA00000002-CP00000007.cert
	certs/Root-CA00000002-MS00000003.cert
	certs/Root-CA00000002-XS00000004.cert
	certs/Root-CA00000002-CP00000005.cert

	# Wii: ppki
	certs/Root-CA00000001.cert
	certs/Root-CA00000001-XS00000003.cert
	certs/Root-CA00000001-CP00000004.cert

	# 3DS: dpki
	#certs/Root-CTR-dpki.cert
	certs/Root-CA00000004.cert
	certs/Root-CA00000004-XS00000009.cert
	certs/Root-CA00000004-CP0000000a.cert

	# 3DS: ppki
	#certs/Root-CTR-ppki.cert
	certs/Root-CA00000003.cert
	certs/Root-CA00000003-XS0000000c.cert
	certs/Root-CA00000003-CP0000000b.cert

	# Wii U: ppki
	#certs/Root-WUP-ppki.cert

	# Wii U: dpki
	#certs/Root-WUP-dpki.cert
	certs/Root-CA00000004.cert
	certs/Root-CA00000004-XS0000000f.cert
	certs/Root-CA00000004-CP00000010.cert
	certs/Root-CA00000004-SP0000000e.cert
	)

FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/certs")
UNSET(h_files)
FOREACH(_cert ${certs_FILES})
	STRING(REPLACE ".cert" ".h" _hfile ${_cert})
	LIST(APPEND h_files "${CMAKE_CURRENT_BINARY_DIR}/${_hfile}")
	ADD_CUSTOM_COMMAND(OUTPUT "${_hfile}"
		COMMAND bin2h "${_cert}" "${CMAKE_CURRENT_BINARY_DIR}/${_hfile}"
		DEPENDS bin2h.c ${_cert}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)
	UNSET(_hfile)
ENDFOREACH()

ADD_CUSTOM_TARGET(certs DEPENDS ${h_files})

######################
# Build the library. #
######################

ADD_LIBRARY(wiicrypto STATIC
	${libwiicrypto_SRCS} ${libwiicrypto_H}
	${libwiicrypto_RSA_SRCS}
	${libwiicrypto_AES_SRCS}
	)
ADD_DEPENDENCIES(wiicrypto certs)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(wiicrypto
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>		# libwiicrypto
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>		# libwiicrypto
	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>			# build
	)
# Exclude from ALL builds.
SET_TARGET_PROPERTIES(wiicrypto PROPERTIES EXCLUDE_FROM_ALL TRUE)
# Make sure git_version.h is created before compiling this target.
IF(TARGET git_version)
	ADD_DEPENDENCIES(wiicrypto git_version)
ENDIF(TARGET git_version)

# Windows-specific
IF(WIN32)
	# CryptGenRandom()
	TARGET_LINK_LIBRARIES(wiicrypto PRIVATE advapi32)
ENDIF(WIN32)

# GMP
IF(HAVE_GMP)
	TARGET_INCLUDE_DIRECTORIES(wiicrypto PRIVATE ${GMP_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(wiicrypto PRIVATE ${GMP_LIBRARIES})
ENDIF(HAVE_GMP)

# Nettle
IF(HAVE_NETTLE)
	TARGET_INCLUDE_DIRECTORIES(wiicrypto PRIVATE ${NETTLE_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(wiicrypto PRIVATE ${NETTLE_LIBRARIES})
ENDIF(HAVE_NETTLE)

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)

# Test suite.
IF(BUILD_TESTING)
	ADD_SUBDIRECTORY(tests)
ENDIF(BUILD_TESTING)
