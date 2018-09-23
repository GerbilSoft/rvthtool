# getopt() for MSVC with Unicode support.
# Reference: https://www.codeproject.com/Articles/157001/Full-getopt-Port-for-Unicode-and-Multibyte-Microso
PROJECT(rvthtool)

# Create the manifest file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/rvthtool.exe.manifest.in" "${CMAKE_CURRENT_BINARY_DIR}/rvthtool.exe.manifest" @ONLY)

# Sources.
SET(rvthtool_SRCS
	main.c
	list-banks.c
	extract.c
	undelete.c
	query.c
	)
# Headers.
SET(rvthtool_H
	list-banks.h
	extract.h
	undelete.h
	query.h
	)
IF(WIN32)
	SET(rvthtool_RC resource.rc)
ENDIF(WIN32)

#########################
# Build the executable. #
#########################

ADD_EXECUTABLE(rvthtool
	${rvthtool_SRCS}
	${rvthtool_H}
	${rvthtool_RC}
	)
SET_TARGET_PROPERTIES(rvthtool PROPERTIES PREFIX "")
DO_SPLIT_DEBUG(rvthtool)
SET_WINDOWS_SUBSYSTEM(rvthtool CONSOLE)
SET_WINDOWS_NO_MANIFEST(rvthtool)

# FIXME: setargv isn't necessarily sorted...
# TODO: Implement our own setargv function?
SET_WINDOWS_ENTRYPOINT(rvthtool wmain ON)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(rvthtool
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	)

TARGET_LINK_LIBRARIES(rvthtool rvth wiicrypto)
IF(MSVC)
	TARGET_LINK_LIBRARIES(rvthtool getopt_msvc)
ENDIF(MSVC)

# CMake-3.7.2 doesn't add include paths to windres.
IF(MINGW)
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_SOURCE_DIR}/..\"")
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_BINARY_DIR}/..\"")
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_BINARY_DIR}\"")
ENDIF(MINGW)

# Win32: image version.
INCLUDE(Win32ImageVersionLinkerFlags)
WIN32_IMAGE_VERSION_LINKER_FLAGS(${VERSION_MAJOR} ${VERSION_MINOR})

#################
# Installation. #
#################

INCLUDE(DirInstallPaths)

# NOTE: Don't install libraries.
# That installs the import library, which isn't used
# for shell extensions.
INSTALL(TARGETS rvthtool
	RUNTIME DESTINATION "${DIR_INSTALL_EXE}"
	LIBRARY DESTINATION "${DIR_INSTALL_DLL}"
	#ARCHIVE DESTINATION "${DIR_INSTALL_LIB}"
	COMPONENT "program"
	)
IF(INSTALL_DEBUG)
	# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
	GET_TARGET_PROPERTY(DEBUG_FILENAME rvthtool PDB)
	INSTALL(FILES "${DEBUG_FILENAME}"
		DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
		COMPONENT "debug"
		)
	UNSET(DEBUG_FILENAME)
ENDIF(INSTALL_DEBUG)
