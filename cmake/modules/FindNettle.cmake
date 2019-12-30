# Find the Nettle library.
# Reference: https://github.com/tatsuhiro-t/wslay/blob/9fe71b1d77823fbca70538bb88a56aec61fecfb3/cmake/FindNettle.cmake

# NETTLE_INCLUDE_DIRS - where to find <nettle/sha.h>, etc.
# NETTLE_LIBRARIES - List of libraries when using libnettle.
# NETTLE_FOUND - True if libnettle found.

IF(NOT TARGET nettle_dll_target)
IF(NOT WIN32)
	if(NETTLE_INCLUDE_DIRS)
		# Already in cache, be silent
		set(NETTLE_FIND_QUIETLY YES)
	endif()

	find_path(NETTLE_INCLUDE_DIRS nettle/md5.h nettle/ripemd160.h nettle/sha.h)
	find_library(NETTLE_LIBRARY NAMES nettle libnettle)
	find_library(HOGWEED_LIBRARY NAMES hogweed libhogweed)

	# handle the QUIETLY and REQUIRED arguments and set NETTLE_FOUND to TRUE if
	# all listed variables are TRUE
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(NETTLE DEFAULT_MSG NETTLE_LIBRARY NETTLE_INCLUDE_DIRS)

	if(NETTLE_FOUND)
		set(NETTLE_LIBRARIES ${HOGWEED_LIBRARY} ${NETTLE_LIBRARY})
	endif()
ELSE(NOT WIN32)
	# Use the included Win32 build of Nettle.
	INCLUDE(DirInstallPaths)
	IF(CPU_i386)
		SET(arch "i386")
	ELSEIF(CPU_amd64)
		SET(arch "amd64")
	ELSE()
		MESSAGE(FATAL_ERROR "Unsupported CPU architecture, please fix!")
	ENDIF()

	SET(NETTLE_WIN32_BASE_PATH "${CMAKE_SOURCE_DIR}/extlib/nettle.win32")
	SET(NETTLE_INCLUDE_DIRS "${NETTLE_WIN32_BASE_PATH}/include")
	IF(MSVC)
		SET(NETTLE_LIBRARY "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libnettle-7.lib")
		SET(HOGWEED_LIBRARY "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libhogweed-5.lib")
	ELSE(MSVC)
		SET(NETTLE_LIBRARY "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libnettle.dll.a")
		SET(HOGWEED_LIBRARY "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libhogweed.dll.a")
	ENDIF(MSVC)
	SET(NETTLE_LIBRARIES ${NETTLE_LIBRARY} ${HOGWEED_LIBRARY})

	# Copy and install the DLLs.
	SET(NETTLE_NETTLE_DLL "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libnettle-7.dll")
	SET(NETTLE_HOGWEED_DLL "${NETTLE_WIN32_BASE_PATH}/lib.${arch}/libhogweed-5.dll")

	# Destination directory.
	# If CMAKE_CFG_INTDIR is set, a Debug or Release subdirectory is being used.
	IF(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
	ELSE(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	ENDIF(CMAKE_CFG_INTDIR)

	ADD_CUSTOM_TARGET(nettle_dll_target ALL
		DEPENDS hogweed_dll_command nettle_dll_command
		)
	ADD_CUSTOM_COMMAND(OUTPUT nettle_dll_command
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy_if_different
			"${NETTLE_NETTLE_DLL}" "${DLL_DESTDIR}/libnettle-7.dll"
		DEPENDS nettle_always_rebuild
		)
	ADD_CUSTOM_COMMAND(OUTPUT hogweed_dll_command
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy_if_different
			"${NETTLE_HOGWEED_DLL}" "${DLL_DESTDIR}/libhogweed-5.dll"
		DEPENDS nettle_always_rebuild
		)
	ADD_CUSTOM_COMMAND(OUTPUT nettle_always_rebuild
		COMMAND ${CMAKE_COMMAND}
		ARGS -E echo
		)

	INSTALL(FILES "${NETTLE_NETTLE_DLL}" "${NETTLE_HOGWEED_DLL}"
		DESTINATION "${DIR_INSTALL_DLL}"
		COMPONENT "dll"
		)

	UNSET(DLL_DESTDIR)
	UNSET(arch)
ENDIF(NOT WIN32)
ENDIF(NOT TARGET nettle_dll_target)
