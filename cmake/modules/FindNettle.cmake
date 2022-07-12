# Find the Nettle library.
# Reference: https://github.com/tatsuhiro-t/wslay/blob/9fe71b1d77823fbca70538bb88a56aec61fecfb3/cmake/FindNettle.cmake

# Nettle_INCLUDE_DIRS - where to find <nettle/sha.h>, etc.
# Nettle_LIBRARIES - List of libraries when using libnettle.
# Nettle_FOUND - True if libnettle found.

IF(NOT TARGET Nettle_dll_target)
IF(NOT WIN32)
	if(Nettle_INCLUDE_DIRS)
		# Already in cache, be silent
		set(Nettle_FIND_QUIETLY YES)
	endif()

	find_path(Nettle_INCLUDE_DIRS nettle/md5.h nettle/ripemd160.h nettle/sha.h)
	find_library(Nettle_LIBRARY NAMES nettle libnettle)
	find_library(Hogweed_LIBRARY NAMES hogweed libhogweed)

	# handle the QUIETLY and REQUIRED arguments and set Nettle_FOUND to TRUE if
	# all listed variables are TRUE
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(Nettle DEFAULT_MSG Nettle_LIBRARY Nettle_INCLUDE_DIRS)

	if(Nettle_FOUND)
		set(Nettle_LIBRARIES ${Hogweed_LIBRARY} ${Nettle_LIBRARY})
	endif()
ELSE(NOT WIN32)
	# Use the included Win32 build of Nettle.
	INCLUDE(CPUInstructionSetFlags)
	IF(CPU_i386 OR CPU_amd64)
		# Supported CPU
	ELSE()
		MESSAGE(FATAL_ERROR "Unsupported CPU architecture, please fix!")
	ENDIF()

	SET(Nettle_WIN32_BASE_PATH "${CMAKE_SOURCE_DIR}/extlib/nettle.win32")
	SET(Nettle_INCLUDE_DIRS "${Nettle_WIN32_BASE_PATH}/include")
	IF(MSVC)
		SET(Nettle_LIBRARY "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libnettle-8.lib")
		SET(Hogweed_LIBRARY "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libhogweed-6.lib")
	ELSE(MSVC)
		SET(Nettle_LIBRARY "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libnettle.dll.a")
		SET(Hogweed_LIBRARY "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libhogweed.dll.a")
	ENDIF(MSVC)
	SET(Nettle_LIBRARIES ${Nettle_LIBRARY} ${Hogweed_LIBRARY})

	# Copy and install the DLLs.
	SET(Nettle_Nettle_DLL "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libnettle-8.dll")
	SET(Nettle_Hogweed_DLL "${Nettle_WIN32_BASE_PATH}/lib.${arch}/libhogweed-6.dll")

	# Destination directory.
	# If CMAKE_CFG_INTDIR is set, a Debug or Release subdirectory is being used.
	IF(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
	ELSE(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	ENDIF(CMAKE_CFG_INTDIR)

	ADD_CUSTOM_TARGET(Nettle_dll_target ALL
		DEPENDS Hogweed_dll_command Nettle_dll_command
		)
	ADD_CUSTOM_COMMAND(OUTPUT Nettle_dll_command
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy_if_different
			"${Nettle_Nettle_DLL}" "${DLL_DESTDIR}/libnettle-8.dll"
		DEPENDS Nettle_always_rebuild
		)
	ADD_CUSTOM_COMMAND(OUTPUT Hogweed_dll_command
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy_if_different
			"${Nettle_Hogweed_DLL}" "${DLL_DESTDIR}/libhogweed-6.dll"
		DEPENDS Nettle_always_rebuild
		)
	ADD_CUSTOM_COMMAND(OUTPUT Nettle_always_rebuild
		COMMAND ${CMAKE_COMMAND}
		ARGS -E echo
		)

	INSTALL(FILES "${Nettle_Nettle_DLL}" "${Nettle_Hogweed_DLL}"
		DESTINATION "${DIR_INSTALL_DLL}"
		COMPONENT "dll"
		)

	UNSET(DLL_DESTDIR)
	UNSET(arch)
ENDIF(NOT WIN32)
ENDIF(NOT TARGET Nettle_dll_target)
