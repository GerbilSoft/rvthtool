PROJECT(librvth)

# Sources.
SET(librvth_SRCS rvth.c)
# Headers.
SET(librvth_H
	common.h
	byteorder.h
	byteswap.h
	rvth_structs.h
	gcn_structs.h
	)

######################
# Build the library. #
######################

ADD_LIBRARY(rvth STATIC ${librvth_SRCS} ${librvth_H})
IF(NOT MSVC)
	TARGET_COMPILE_FEATURES(rvth PUBLIC c_std_99)
ENDIF(NOT MSVC)

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

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)
