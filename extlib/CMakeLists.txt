PROJECT(extlib)

# Set common properties for extlib targets.
INCLUDE(SetMSVCDebugPath)
FUNCTION(SET_EXTLIB_PROPERTIES)
	FOREACH(_target ${ARGV})
		IF(TARGET ${_target})
			# Exclude from ALL builds.
			SET_TARGET_PROPERTIES(${_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
		ENDIF(TARGET ${_target})
	ENDFOREACH(_target ${ARGV})
ENDFUNCTION(SET_EXTLIB_PROPERTIES)

# getopt for MSVC
IF(MSVC)
	ADD_SUBDIRECTORY(getopt_msvc)
	SET_EXTLIB_PROPERTIES(getopt_msvc)
ENDIF(MSVC)

# Google Test
IF(BUILD_TESTING)
	# Reference: http://stackoverflow.com/questions/12540970/how-to-make-gtest-build-mdd-instead-of-mtd-by-default-using-cmake
	SET(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll")
	SET(SKIP_INSTALL_LIBRARIES ON)
	SET(SKIP_INSTALL_ALL ON)

	# Use shared libraries on Windows.
	IF(WIN32)
		SET(BUILD_STATIC_LIBS OFF)
		SET(BUILD_SHARED_LIBS ON)
	ELSE(WIN32)
		SET(BUILD_STATIC_LIBS ON)
		SET(BUILD_SHARED_LIBS OFF)
	ENDIF(WIN32)

	ADD_SUBDIRECTORY(googletest)
	INCLUDE(SetMSVCDebugPath)
	SET_EXTLIB_PROPERTIES(
		gtest gtest_main gtest_no_exception gtest_main_no_exception
		gtest_main_no_rtti gtest_dll gtest_main_use_own_tuple
		gmock gmock_main gmock_main_no_exception
		gmock_main_no_rtti gmock_main_use_own_tuple
		)
	IF(WIN32)
		# GTest is a DLL, so we need to set this.
		TARGET_COMPILE_DEFINITIONS(gtest INTERFACE GTEST_LINKED_AS_SHARED_LIBRARY=1)
		#TARGET_COMPILE_DEFINITIONS(gtest_main INTERFACE GTEST_LINKED_AS_SHARED_LIBRARY=1)
	ENDIF(WIN32)
ENDIF(BUILD_TESTING)
