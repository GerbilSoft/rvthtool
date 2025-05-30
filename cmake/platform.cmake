# Platform-specific functionality.

# Hack to remove -rdynamic from CFLAGS and CXXFLAGS
# See http://public.kitware.com/pipermail/cmake/2006-July/010404.html
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
	SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
ENDIF()

# Don't embed rpaths in the executables.
SET(CMAKE_SKIP_RPATH ON)

# stdint.h must be present.
INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
IF(NOT HAVE_STDINT_H)
	MESSAGE(FATAL_ERROR "stdint.h is required.")
ENDIF(NOT HAVE_STDINT_H)

# Common flag variables:
# [common]
# - RP_C_FLAGS_COMMON
# - RP_CXX_FLAGS_COMMON
# - RP_EXE_LINKER_FLAGS_COMMON
# - RP_SHARED_LINKER_FLAGS_COMMON
# - RP_MODULE_LINKER_FLAGS_COMMON
# [debug]
# - RP_C_FLAGS_DEBUG
# - RP_CXX_FLAGS_DEBUG
# - RP_EXE_LINKER_FLAGS_DEBUG
# - RP_SHARED_LINKER_FLAGS_DEBUG
# - RP_MODULE_LINKER_FLAGS_DEBUG
# [release]
# - RP_C_FLAGS_RELEASE
# - RP_CXX_FLAGS_RELEASE
# - RP_EXE_LINKER_FLAGS_RELEASE
# - RP_SHARED_LINKER_FLAGS_RELEASE
# - RP_MODULE_LINKER_FLAGS_RELEASE
# [RelWithDebInfo]
# - RP_C_FLAGS_RELWITHDEBINFO
# - RP_CXX_FLAGS_RELWITHDEBINFO
# - RP_EXE_LINKER_FLAGS_RELWITHDEBINFO
# - RP_SHARED_LINKER_FLAGS_RELWITHDEBINFO
# - RP_MODULE_LINKER_FLAGS_RELWITHDEBINFO
#
# DEBUG and RELEASE variables do *not* include COMMON.
IF(MSVC)
	INCLUDE(cmake/platform/msvc.cmake)
ELSE(MSVC)
	INCLUDE(cmake/platform/gcc.cmake)
ENDIF(MSVC)

# Platform-specific configuration.
IF(WIN32)
	INCLUDE(cmake/platform/win32.cmake)
ENDIF(WIN32)

# Check what flag is needed for stack smashing protection.
INCLUDE(CheckStackProtectorCompilerFlag)
CHECK_STACK_PROTECTOR_COMPILER_FLAG(RP_STACK_CFLAG)
SET(RP_C_FLAGS_COMMON "${RP_C_FLAGS_COMMON} ${RP_STACK_CFLAG}")
SET(RP_CXX_FLAGS_COMMON "${RP_CXX_FLAGS_COMMON} ${RP_STACK_CFLAG}")
UNSET(RP_STACK_CFLAG)

# Check for Large File Support.
INCLUDE(CheckLargeFileSupport)
CHECK_LARGE_FILE_SUPPORT()
IF(NOT LFS_FOUND)
	MESSAGE(FATAL_ERROR "Large File Support is required.")
ENDIF(NOT LFS_FOUND)
IF(LFS_DEFINITIONS)
	SET(RP_C_FLAGS_COMMON "${RP_C_FLAGS_COMMON} ${LFS_DEFINITIONS}")
	SET(RP_CXX_FLAGS_COMMON "${RP_CXX_FLAGS_COMMON} ${LFS_DEFINITIONS}")
ENDIF(LFS_DEFINITIONS)

# Check for 64-bit time_t.
INCLUDE(Check64BitTimeSupport)
CHECK_64BIT_TIME_SUPPORT()
# NOTES:
# - glibc does not currently support 64-bit time_t on 32-bit.
# - Mac OS X does not (and won't) support 64-bit time_t on 32-bit.
IF(TIME64_DEFINITIONS)
	SET(RP_C_FLAGS_COMMON "${RP_C_FLAGS_COMMON} ${TIME64_DEFINITIONS}")
	SET(RP_CXX_FLAGS_COMMON "${RP_CXX_FLAGS_COMMON} ${TIME64_DEFINITIONS}")
ENDIF(TIME64_DEFINITIONS)

# Set CMAKE flags.
# TODO: RelWithDebInfo / MinSizeRel?
# Common
SET(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} ${RP_C_FLAGS_COMMON}")
SET(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} ${RP_CXX_FLAGS_COMMON}")
SET(CMAKE_EXE_LINKER_FLAGS		"${CMAKE_EXE_LINKER_FLAGS} ${RP_EXE_LINKER_FLAGS_COMMON}")
SET(CMAKE_SHARED_LINKER_FLAGS		"${CMAKE_SHARED_LINKER_FLAGS} ${RP_SHARED_LINKER_FLAGS_COMMON}")
SET(CMAKE_MODULE_LINKER_FLAGS		"${CMAKE_MODULE_LINKER_FLAGS} ${RP_MODULE_LINKER_FLAGS_COMMON}")
# Debug
SET(CMAKE_C_FLAGS_DEBUG			"${CMAKE_C_FLAGS_DEBUG} ${RP_C_FLAGS_DEBUG}")
SET(CMAKE_CXX_FLAGS_DEBUG		"${CMAKE_CXX_FLAGS_DEBUG} ${RP_CXX_FLAGS_DEBUG}")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG	"${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${RP_EXE_LINKER_FLAGS_DEBUG}")
SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG	"${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${RP_SHARED_LINKER_FLAGS_DEBUG}")
SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG	"${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${RP_MODULE_LINKER_FLAGS_DEBUG}")
# Release
SET(CMAKE_C_FLAGS_RELEASE		"${CMAKE_C_FLAGS_RELEASE} ${RP_C_FLAGS_RELEASE}")
SET(CMAKE_CXX_FLAGS_RELEASE		"${CMAKE_CXX_FLAGS_RELEASE} ${RP_CXX_FLAGS_RELEASE}")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE	"${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${RP_EXE_LINKER_FLAGS_RELEASE}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE	"${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${RP_SHARED_LINKER_FLAGS_RELEASE}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE	"${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${RP_MODULE_LINKER_FLAGS_RELEASE}")
# RelWithDebInfo
SET(CMAKE_C_FLAGS_RELWITHDEBINFO		"${CMAKE_C_FLAGS_RELWITHDEBINFO} ${RP_C_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO		"${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${RP_CXX_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${RP_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} ${RP_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} ${RP_MODULE_LINKER_FLAGS_RELWITHDEBINFO}")

# Unset temporary variables.
# Common
UNSET(RP_C_FLAGS_COMMON)
UNSET(RP_CXX_FLAGS_COMMON)
UNSET(RP_EXE_LINKER_FLAGS_COMMON)
UNSET(RP_SHARED_LINKER_FLAGS_COMMON)
UNSET(RP_MODULE_LINKER_FLAGS_COMMON)
# Debug
UNSET(RP_C_FLAGS_DEBUG)
UNSET(RP_CXX_FLAGS_DEBUG)
UNSET(RP_EXE_LINKER_FLAGS_DEBUG)
UNSET(RP_SHARED_LINKER_FLAGS_DEBUG)
UNSET(RP_MODULE_LINKER_FLAGS_DEBUG)
# Release
UNSET(RP_C_FLAGS_RELEASE)
UNSET(RP_CXX_FLAGS_RELEASE)
UNSET(RP_EXE_LINKER_FLAGS_RELEASE)
UNSET(RP_SHARED_LINKER_FLAGS_RELEASE)
UNSET(RP_MODULE_LINKER_FLAGS_RELEASE)

###### Windows-specific CMake functions. ######

# Set Windows subsystem when building with MSVC.
FUNCTION(SET_WINDOWS_SUBSYSTEM _target _subsystem)
IF(WIN32 AND MSVC)
	GET_TARGET_PROPERTY(TARGET_LINK_FLAGS ${_target} LINK_FLAGS)
	IF(TARGET_LINK_FLAGS)
		SET(TARGET_LINK_FLAGS "${TARGET_LINK_FLAGS} ${RP_LINKER_FLAGS_${_subsystem}_EXE}")
	ELSE()
		SET(TARGET_LINK_FLAGS "${RP_LINKER_FLAGS_${_subsystem}_EXE}")
	ENDIF()
	SET_TARGET_PROPERTIES(${_target} PROPERTIES LINK_FLAGS "${TARGET_LINK_FLAGS}")
ENDIF(WIN32 AND MSVC)
ENDFUNCTION()

# Disable automatic manifest generation when building with MSVC.
# Only use this if you have a custom manifest specified in the resource script.
FUNCTION(SET_WINDOWS_NO_MANIFEST _target)
IF(WIN32 AND MSVC)
	GET_TARGET_PROPERTY(TARGET_LINK_FLAGS ${_target} LINK_FLAGS)
	IF(TARGET_LINK_FLAGS)
		SET(TARGET_LINK_FLAGS "${TARGET_LINK_FLAGS} /MANIFEST:NO")
	ELSE()
		SET(TARGET_LINK_FLAGS "/MANIFEST:NO")
	ENDIF()
	SET_TARGET_PROPERTIES(${_target} PROPERTIES LINK_FLAGS "${TARGET_LINK_FLAGS}")
ENDIF(WIN32 AND MSVC)
ENDFUNCTION()

# Set Windows entrypoint.
# _target: Target.
# _entrypoint: Entry point. (main, wmain, WinMain, wWinMain)
# _setargv: If true, link to setargv.obj/wsetargv.obj in MSVC builds.
FUNCTION(SET_WINDOWS_ENTRYPOINT _target _entrypoint _setargv)
IF(WIN32)
	IF(MSVC)
		# MSVC automatically prepends an underscore if necessary.
		SET(ENTRY_POINT_FLAG "/ENTRY:${_entrypoint}CRTStartup")
		IF(_setargv)
			IF(_entrypoint MATCHES ^wmain OR _entrypoint MATCHES ^wWinMain)
				SET(SETARGV_FLAG "wsetargv.obj")
			ELSE()
				SET(SETARGV_FLAG "setargv.obj")
			ENDIF()
		ENDIF(_setargv)
	ELSE(MSVC)
		# MinGW does not automatically prepend an underscore.
		# TODO: _setargv for MinGW.

		# NOTE: MinGW uses separate crt*.o files for Unicode
		# instead of a separate entry point.
		IF(_entrypoint MATCHES "^w")
			SET(UNICODE_FLAG "-municode")
			STRING(SUBSTRING "${_entrypoint}" 1 -1 _entrypoint)
		ENDIF()

		IF(CPU_i386)
			SET(ENTRY_POINT "_${_entrypoint}CRTStartup")
		ELSE(CPU_i386)
			SET(ENTRY_POINT "${_entrypoint}CRTStartup")
		ENDIF(CPU_i386)
		SET(ENTRY_POINT_FLAG "-Wl,-e,${ENTRY_POINT}")
	ENDIF(MSVC)

	GET_TARGET_PROPERTY(TARGET_LINK_FLAGS ${_target} LINK_FLAGS)
	IF(TARGET_LINK_FLAGS)
		SET(TARGET_LINK_FLAGS "${TARGET_LINK_FLAGS} ${UNICODE_FLAG} ${ENTRY_POINT_FLAG} ${SETARGV_FLAG}")
	ELSE()
		SET(TARGET_LINK_FLAGS "${UNICODE_FLAG} ${ENTRY_POINT_FLAG} ${SETARGV_FLAG}")
	ENDIF()
	SET_TARGET_PROPERTIES(${_target} PROPERTIES LINK_FLAGS "${TARGET_LINK_FLAGS}")
	UNSET(UNICODE_FLAG)
	UNSET(ENTRY_POINT_FLAG)
	UNSET(SETARGV_FLAG)
ENDIF(WIN32)
ENDFUNCTION()
