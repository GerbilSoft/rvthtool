# Check for Large File Support.
# Known cases:
# - Windows, MSVC: No macros are needed, but 64-bit functions must be
#   used explicitly, e.g. _fseeki64() and _ftelli64(). libW32U
#   provides macro redefinitions for this.
# - Windows, MinGW: LFS functions fseeko() and ftello() are present.
#   LFS Macros are required for large files.
# - 32-bit Linux: LFS macros are required.
# - 64-bit Linux: No macros are required.
# - All Mac OS X: No macros are required.

# Partially based on:
# https://github.com/Benjamin-Dobell/Heimdall/blob/master/cmake/LargeFiles.cmake

# This sets the following variables:
# - LFS_FOUND: Set to 1 if LFS is supported.
# - LFS_FOUND_FSEEKO: Set to 1 if LFS is supported using fseeko().
# - LFS_FOUND_FSEEKI64: Set to 1 if LFS is supported using _fseeki64().
# - LFS_DEFINITIONS: Preprocessor macros required for large file support, if any.

# TODO: Use _fseeki64() and _ftelli64() on MinGW to avoid
# the use of wrapper functions?

FUNCTION(CHECK_LARGE_FILE_SUPPORT)
	IF(NOT DEFINED LFS_FOUND)
		# NOTE: ${CMAKE_MODULE_PATH} has two directories, macros/ and libs/,
		# so we have to configure this manually.
		SET(LFS_SOURCE_PATH "${CMAKE_SOURCE_DIR}/cmake/macros")

		# Check for LFS.
		MESSAGE(STATUS "Checking if Large File Support is available")
		IF(MSVC)
			# Check if _fseeki64() and _ftelli64() are available.
			TRY_COMPILE(TMP_LFS_FOUND "${CMAKE_BINARY_DIR}"
				"${LFS_SOURCE_PATH}/LargeFileSupport_fseeki64.c")
			IF(TMP_LFS_FOUND)
				# _fseeki64() and _ftelli64() are available.
				MESSAGE(STATUS "Checking if Large File Support is available - yes, using MSVC non-standard functions")
				SET(TMP_LFS_FOUND_FSEEKI64 1)
			ELSE()
				# Not available...
				MESSAGE(STATUS "Checking if Large File Support is available - no")
				MESSAGE(WARNING "MSVC 2005 (8.0) or later is required for Large File Support.")
			ENDIF()
		ELSE()
			# Check if the OS supports Large Files out of the box.
			TRY_COMPILE(TMP_LFS_FOUND "${CMAKE_BINARY_DIR}"
				"${LFS_SOURCE_PATH}/LargeFileSupport_fseeko.c")
			IF(TMP_LFS_FOUND)
				# Supported out of the box.
				MESSAGE(STATUS "Checking if Large File Support is available - yes")
			ELSE()
				# Try adding LFS macros.
				SET(TMP_LFS_DEFINITIONS -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64)
				TRY_COMPILE(TMP_LFS_FOUND "${CMAKE_BINARY_DIR}"
					"${LFS_SOURCE_PATH}/LargeFileSupport_fseeko.c"
					COMPILE_DEFINITIONS ${TMP_LFS_DEFINITIONS})
				IF(TMP_LFS_FOUND)
					# LFS macros work.
					MESSAGE(STATUS "Checking if Large File Support is available - yes, using LFS macros")
					SET(TMP_LFS_FOUND_FSEEKO 1)
					# NOTE: COMPILE_DEFINITIONS requires a semicolon-separated list;
					# CFLAGS reqiures space-separated.
					STRING(REPLACE ";" " " TMP_LFS_DEFINITIONS "${TMP_LFS_DEFINITIONS}")
				ELSE()
					# LFS macros failed.
					MESSAGE(STATUS "Checking if Large File Support is available - no")
					UNSET(TMP_LFS_DEFINITIONS)
				ENDIF()
			ENDIF()
		ENDIF()

		SET(LFS_FOUND ${TMP_LFS_FOUND} CACHE INTERNAL "Is Large File Support available?")
		SET(LFS_FOUND_FSEEKO ${TMP_LFS_FOUND_FSEEKO} CACHE INTERNAL "Large File Support is available using LFS macros")
		SET(LFS_FOUND_FSEEKI64 ${TMP_LFS_FOUND_FSEEKI64} CACHE INTERNAL "Large File Support is available using MSVC non-standard functions")
		SET(LFS_DEFINITIONS "${TMP_LFS_DEFINITIONS}" CACHE INTERNAL "Definitions required for Large File Support")
	ENDIF()
ENDFUNCTION(CHECK_LARGE_FILE_SUPPORT)
