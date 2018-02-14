PROJECT(getopt_msvc)

######################
# Build the library. #
######################

ADD_LIBRARY(getopt_msvc STATIC getopt.c getopt.h)
TARGET_COMPILE_DEFINITIONS(getopt_msvc PUBLIC -DSTATIC_GETOPT)

# Include paths:
# - Public: Current source directory.
TARGET_INCLUDE_DIRECTORIES(getopt_msvc
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>		# getopt
	)
# Exclude from ALL builds.
SET_TARGET_PROPERTIES(getopt_msvc PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)
