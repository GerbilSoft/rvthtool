# WAD Resigning and Conversion tool
PROJECT(wadresign LANGUAGES C CXX)

# Create the Win32 manifest file.
IF(WIN32)
	CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/wadresign.exe.manifest.in" "${CMAKE_CURRENT_BINARY_DIR}/wadresign.exe.manifest" @ONLY)
ENDIF(WIN32)

# Sources.
SET(wadresign_SRCS
	main.c
	print-info.c
	wad-fns.c
	resign-wad.cpp
	)
# Headers.
SET(wadresign_H
	print-info.h
	wad-fns.h
	resign-wad.hpp
	)
IF(WIN32)
	SET(wadresign_RC resource.rc)
ENDIF(WIN32)

#########################
# Build the executable. #
#########################

ADD_EXECUTABLE(wadresign
	${wadresign_SRCS}
	${wadresign_H}
	${wadresign_RC}
	)
SET_TARGET_PROPERTIES(wadresign PROPERTIES PREFIX "")
DO_SPLIT_DEBUG(wadresign)
SET_WINDOWS_SUBSYSTEM(wadresign CONSOLE)
SET_WINDOWS_NO_MANIFEST(wadresign)

# FIXME: setargv isn't necessarily sorted...
# TODO: Implement our own setargv function?
SET_WINDOWS_ENTRYPOINT(wadresign wmain ON)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(wadresign
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	)

TARGET_LINK_LIBRARIES(wadresign PRIVATE wiicrypto)
IF(MSVC)
	TARGET_LINK_LIBRARIES(wadresign PRIVATE getopt_msvc)
ENDIF(MSVC)

# GMP
IF(HAVE_GMP)
	TARGET_INCLUDE_DIRECTORIES(wadresign PRIVATE ${GMP_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(wadresign PRIVATE ${GMP_LIBRARIES})
ENDIF(HAVE_GMP)

# Nettle
IF(HAVE_NETTLE)
	TARGET_INCLUDE_DIRECTORIES(wadresign PRIVATE ${NETTLE_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(wadresign PRIVATE ${NETTLE_LIBRARIES})
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
INSTALL(TARGETS wadresign
	RUNTIME DESTINATION "${DIR_INSTALL_EXE}"
	LIBRARY DESTINATION "${DIR_INSTALL_DLL}"
	#ARCHIVE DESTINATION "${DIR_INSTALL_LIB}"
	COMPONENT "program"
	)
IF(INSTALL_DEBUG)
	# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
	GET_TARGET_PROPERTY(DEBUG_FILENAME wadresign PDB)
	INSTALL(FILES "${DEBUG_FILENAME}"
		DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
		COMPONENT "debug"
		)
	UNSET(DEBUG_FILENAME)
ENDIF(INSTALL_DEBUG)
