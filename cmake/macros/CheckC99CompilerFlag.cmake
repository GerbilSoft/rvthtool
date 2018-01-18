# - Check what flag is needed to activate C99 mode.
# CHECK_C99_COMPILER_FLAG(VARIABLE)
#
#  VARIABLE - variable to store the result
# 
#  This actually calls the check_c_source_compiles macro.
#  See help for CheckCSourceCompiles for a listing of variables
#  that can modify the build.

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# Based on CHECK_C_COMPILER_FLAG(). (CheckCCompilerFlag.cmake)

INCLUDE(CheckCSourceCompiles)

MACRO(CHECK_C99_COMPILER_FLAG _RESULT)
	# Flag listing borrowed from GNU autoconf's AC_PROG_CC_C99 macro.
	UNSET(${_RESULT})

	# MSVC doesn't allow setting the C standard.
	IF(NOT MSVC)
	# Check if C99 is present without any flags.
	# gcc-5.1 uses C11 mode by default.
	MESSAGE(STATUS "Checking if C99 is enabled by default:")
	CHECK_C_SOURCE_COMPILES("
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error C99 is not enabled
#endif

int main() { return 0; }" CHECK_C99_ENABLED_DEFAULT)
	IF (${CHECK_C99_ENABLED_DEFAULT})
		UNSET(${_RESULT})
		MESSAGE(STATUS "Checking if C99 is enabled by default: yes")
	ELSE()
		MESSAGE(STATUS "Checking if C99 is enabled by default: no")
		MESSAGE(STATUS "Checking what CFLAG is required for C99:")
		FOREACH(CHECK_C99_CFLAG "-std=gnu99" "-std=c99" "-c99" "-AC99" "-xc99=all" "-qlanglvl=extc99")
			SET(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
			SET(CMAKE_REQUIRED_DEFINITIONS "${CHECK_C99_CFLAG}")
			CHECK_C_SOURCE_COMPILES("int main() { return 0; }" CFLAG_${CHECK_C99_CFLAG})
			SET(CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
			IF(CFLAG_${CHECK_C99_CFLAG})
				SET(${_RESULT} ${CHECK_C99_CFLAG})
				BREAK()
			ENDIF(CFLAG_${CHECK_C99_CFLAG})
			UNSET(CFLAG_${CHECK_C99_CFLAG})
		ENDFOREACH()
	
		IF(${_RESULT})
			MESSAGE(STATUS "Checking what CFLAG is required for C99: ${${_RESULT}}")
		ELSE(${_RESULT})
			MESSAGE(STATUS "Checking what CFLAG is required for C99: none")
		ENDIF(${_RESULT})
	ENDIF()
	UNSET(CHECK_C99_ENABLED_DEFAULT)
	ENDIF(NOT MSVC)
ENDMACRO (CHECK_C99_COMPILER_FLAG)
