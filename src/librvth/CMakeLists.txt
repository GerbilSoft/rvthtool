# librvth: RVT-H Reader library
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(librvth LANGUAGES C CXX)

# Check for C library functions.
IF(NOT WIN32)
	INCLUDE(CheckFunctionExists)
	CHECK_FUNCTION_EXISTS(ftruncate HAVE_FTRUNCATE)
ENDIF(NOT WIN32)

IF(WIN32)
	# Win32 API has built-in device querying functionality.
	SET(HAVE_QUERY 1)
ELSEIF(ENABLE_UDEV)
	# Find UDEV.
	IF(UNIX AND NOT APPLE)
		FIND_PACKAGE(UDev)
		IF(UDEV_FOUND)
			SET(HAVE_UDEV 1)
			SET(HAVE_QUERY 1)
		ENDIF(UDEV_FOUND)
	ENDIF()
ENDIF()

# Write the config.h file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.librvth.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.librvth.h")

# Sources.
SET(librvth_SRCS
	rvth.cpp
	rvth_p.cpp
	write.cpp
	extract.cpp
	rvth_time.c
	recrypt.cpp
	RefFile.cpp
	disc_header.cpp
	query.c
	ptbl.cpp
	extract_crypt.cpp
	bank_init.cpp
	rvth_error.c

	# Disc image readers
	reader/Reader.cpp
	reader/PlainReader.cpp
	reader/CisoReader.cpp
	reader/WbfsReader.cpp
	)
# Headers.
SET(librvth_H
	nhcd_structs.h
	rvth.hpp
	rvth_time.h
	RefFile.hpp
	disc_header.hpp
	query.h
	ptbl.h
	bank_init.h
	rvth_error.h
	rvth_enums.h

	# Disc image readers
	reader/Reader.hpp
	reader/PlainReader.hpp
	reader/CisoReader.hpp
	reader/libwbfs.h
	reader/WbfsReader.hpp
	)

IF(WIN32)
	SET(librvth_QUERY_SRCS query_win32.c)
ELSEIF(HAVE_UDEV)
	SET(librvth_QUERY_SRCS query_udev.c)
ENDIF()

######################
# Build the library. #
######################

ADD_LIBRARY(rvth STATIC
	${librvth_SRCS} ${librvth_H}
	${librvth_RSA_SRCS}
	${librvth_AES_SRCS}
	${librvth_QUERY_SRCS}
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

# libwiicrypto
TARGET_LINK_LIBRARIES(rvth PRIVATE wiicrypto)

# GMP
IF(HAVE_GMP)
	TARGET_INCLUDE_DIRECTORIES(rvth PRIVATE ${GMP_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(rvth PRIVATE ${GMP_LIBRARIES})
ENDIF(HAVE_GMP)

# Nettle
IF(HAVE_NETTLE)
	TARGET_INCLUDE_DIRECTORIES(rvth PRIVATE ${Nettle_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(rvth PRIVATE ${Nettle_LIBRARIES})
ENDIF(HAVE_NETTLE)

# Device query library
IF(WIN32)
	TARGET_LINK_LIBRARIES(rvth PRIVATE setupapi)
ELSEIF(HAVE_UDEV)
	TARGET_INCLUDE_DIRECTORIES(rvth PRIVATE ${UDEV_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(rvth PRIVATE ${UDEV_LIBRARIES})
ENDIF()

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)
