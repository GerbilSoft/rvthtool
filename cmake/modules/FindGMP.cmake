# Try to find the GMP librairies
#  GMP_FOUND - system has GMP lib
#  GMP_INCLUDE_DIR - the GMP include directory
#  GMP_LIBRARIES - Libraries needed to use GMP

# Copyright (c) 2006, Laurent Montel, <montel@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


IF(NOT WIN32)
	if (GMP_INCLUDE_DIR AND GMP_LIBRARIES)
	# Already in cache, be silent
	set(GMP_FIND_QUIETLY TRUE)
	endif (GMP_INCLUDE_DIR AND GMP_LIBRARIES)

	find_path(GMP_INCLUDE_DIR NAMES gmp.h )
	find_library(GMP_LIBRARIES NAMES gmp libgmp)

	include(FindPackageHandleStandardArgs)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(GMP DEFAULT_MSG GMP_INCLUDE_DIR GMP_LIBRARIES)

	mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARIES)
ELSE(NOT WIN32)
	# Use the included Win32 build of MPIR.
	# NOTE: DirInstallPaths sets ${arch}.
	INCLUDE(DirInstallPaths)
	IF(NOT arch MATCHES "^(i.|x)86$|^x86_64$|^amd64$")
		MESSAGE(FATAL_ERROR "Architecture ${arch} is not supported.")
	ENDIF(NOT arch MATCHES "^(i.|x)86$|^x86_64$|^amd64$")

	SET(GMP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/win32/${arch}/include")
	SET(GMP_LIBRARIES "${CMAKE_SOURCE_DIR}/win32/${arch}/lib/mpir.lib")

	# Copy and install the DLL.
	SET(GMP_DLL "${CMAKE_SOURCE_DIR}/win32/${arch}/lib/mpir.dll")

	# Destination directory.
	# If CMAKE_CFG_INTDIR is set, a Debug or Release subdirectory is being used.
	IF(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}")
	ELSE(CMAKE_CFG_INTDIR)
		SET(DLL_DESTDIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	ENDIF(CMAKE_CFG_INTDIR)

	ADD_CUSTOM_TARGET(mpir_dll_target ALL
		DEPENDS mpir_dll_command
		)
	ADD_CUSTOM_COMMAND(OUTPUT mpir_dll_command
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy_if_different
			"${GMP_DLL}" "${DLL_DESTDIR}/mpir.dll"
		DEPENDS always_rebuild
		)
	ADD_CUSTOM_COMMAND(OUTPUT always_rebuild
		COMMAND ${CMAKE_COMMAND}
		ARGS -E echo
		)

	INSTALL(FILES "${GMP_DLL}"
		DESTINATION "${DIR_INSTALL_DLL}"
		COMPONENT "dll"
		)

	UNSET(DLL_DESTDIR)
ENDIF(NOT WIN32)
