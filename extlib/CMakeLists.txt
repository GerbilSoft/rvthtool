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
	ADD_SUBDIRECTORY(googletest)

	SET_EXTLIB_PROPERTIES(
		gtest gtest_main gtest_no_exception gtest_main_no_exception
		gtest_main_no_rtti gtest_dll gtest_main_use_own_tuple
		gmock gmock_main gmock_main_no_exception
		gmock_main_no_rtti gmock_main_use_own_tuple
		)
ENDIF(BUILD_TESTING)
