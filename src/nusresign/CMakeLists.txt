# Wii U NUS Resigning and Conversion tool
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(nusresign LANGUAGES C)

# Create the Win32 manifest file.
IF(WIN32)
	CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/nusresign.exe.manifest.in" "${CMAKE_CURRENT_BINARY_DIR}/nusresign.exe.manifest" @ONLY)
ENDIF(WIN32)

# Sources.
SET(nusresign_SRCS
	main.c
	resign-nus.cpp
	print-info.cpp
	)
# Headers.
SET(nusresign_H
	resign-nus.hpp
	print-info.hpp
	)
IF(WIN32)
	SET(nusresign_RC resource.rc)
ENDIF(WIN32)

#########################
# Build the executable. #
#########################

ADD_EXECUTABLE(nusresign
	${nusresign_SRCS}
	${nusresign_H}
	${nusresign_RC}
	)
SET_TARGET_PROPERTIES(nusresign PROPERTIES PREFIX "")
DO_SPLIT_DEBUG(nusresign)
SET_WINDOWS_SUBSYSTEM(nusresign CONSOLE)
SET_WINDOWS_NO_MANIFEST(nusresign)

# FIXME: setargv isn't necessarily sorted...
# TODO: Implement our own setargv function?
SET_WINDOWS_ENTRYPOINT(nusresign wmain ON)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(nusresign
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	)

TARGET_LINK_LIBRARIES(nusresign PRIVATE wiicrypto)
IF(MSVC)
	TARGET_LINK_LIBRARIES(nusresign PRIVATE getopt_msvc)
ENDIF(MSVC)

# GMP
IF(HAVE_GMP)
	TARGET_INCLUDE_DIRECTORIES(nusresign PRIVATE ${GMP_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(nusresign PRIVATE ${GMP_LIBRARIES})
ENDIF(HAVE_GMP)

# Nettle
IF(HAVE_NETTLE)
	TARGET_INCLUDE_DIRECTORIES(nusresign PRIVATE ${Nettle_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(nusresign PRIVATE ${Nettle_LIBRARIES})
ENDIF(HAVE_NETTLE)

# CMake-3.7.2 doesn't add include paths to windres.
IF(MINGW)
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_SOURCE_DIR}/..\"")
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_BINARY_DIR}/..\"")
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_BINARY_DIR}\"")
ENDIF(MINGW)

#################
# Installation. #
#################

INCLUDE(DirInstallPaths)

# NOTE: Don't install libraries.
# That installs the import library, which isn't used
# for shell extensions.
INSTALL(TARGETS nusresign
	RUNTIME DESTINATION "${DIR_INSTALL_EXE}"
	LIBRARY DESTINATION "${DIR_INSTALL_DLL}"
	#ARCHIVE DESTINATION "${DIR_INSTALL_LIB}"
	COMPONENT "program"
	)
IF(INSTALL_DEBUG)
	# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
	GET_TARGET_PROPERTY(DEBUG_FILENAME nusresign PDB)
	INSTALL(FILES "${DEBUG_FILENAME}"
		DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
		COMPONENT "debug"
		)
	UNSET(DEBUG_FILENAME)
ENDIF(INSTALL_DEBUG)
