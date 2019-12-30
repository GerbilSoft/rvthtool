# Directory install paths.
# Files are installed in different directories depending
# on platform, e.g. Unix-style for Linux and most other
# Unix systems.

# NOTE: DIR_INSTALL_DOC_ROOT is for documents that should
# be in the root of the Windows ZIP file. On other platforms,
# it's the same as DIR_INSTALL_DOC.

IF(NOT PACKAGE_NAME)
	MESSAGE(FATAL_ERROR "PACKAGE_NAME is not set.")
ENDIF(NOT PACKAGE_NAME)

# Architecture name for arch-specific paths.
IF(CPU_amd64)
	SET(arch "amd64")
ELSEIF(CPU_i386)
	SET(arch "i386")
ELSEIF(CPU_ia64)
	SET(arch "ia64")
ELSEIF(CPU_arm)
	SET(arch "arm")
ELSEIF(CPU_arm64)
	SET(arch "arm64")
ELSE()
	MESSAGE(FATAL_ERROR "Unsupported CPU architecture, please fix!")
ENDIF()

INCLUDE(GNUInstallDirs)
IF(UNIX AND NOT APPLE)
	SET(DIR_INSTALL_EXE "${CMAKE_INSTALL_BINDIR}")
	SET(DIR_INSTALL_DLL "${CMAKE_INSTALL_LIBDIR}")
	SET(DIR_INSTALL_LIB "${CMAKE_INSTALL_LIBDIR}")
	SET(DIR_INSTALL_TRANSLATIONS "share/${PACKAGE_NAME}/translations")
	SET(DIR_INSTALL_DOC "share/doc/${PACKAGE_NAME}")
	SET(DIR_INSTALL_DOC_ROOT "${DIR_INSTALL_DOC}")
	SET(DIR_INSTALL_EXE_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_EXE}")
	SET(DIR_INSTALL_DLL_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_DLL}")
	SET(DIR_INSTALL_LIB_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_LIB}")
ELSEIF(APPLE)
	# Mac OS X-style install paths.
	# Install should be relative to the application bundle.
	# TODO: Optimize for bundles. For now, using the same layout as Linux.
	SET(DIR_INSTALL_EXE "${CMAKE_INSTALL_BINDIR}")
	SET(DIR_INSTALL_DLL "${CMAKE_INSTALL_LIBDIR}")
	SET(DIR_INSTALL_LIB "${CMAKE_INSTALL_LIBDIR}")
	SET(DIR_INSTALL_TRANSLATIONS "share/${PACKAGE_NAME}/translations")
	SET(DIR_INSTALL_DOC "share/doc/${PACKAGE_NAME}")
	SET(DIR_INSTALL_DOC_ROOT "${DIR_INSTALL_DOC}")
	SET(DIR_INSTALL_EXE_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_EXE}")
	SET(DIR_INSTALL_DLL_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_DLL}")
	SET(DIR_INSTALL_LIB_DEBUG "lib/debug/${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_LIB}")
ELSEIF(WIN32)
	# Win32-style install paths.
	# Files are installed relative to root, since the
	# program is run out of its own directory.
	SET(DIR_INSTALL_EXE ".")
	SET(DIR_INSTALL_DLL ".")
	SET(DIR_INSTALL_LIB ".")
	SET(DIR_INSTALL_TRANSLATIONS "translations")
	SET(DIR_INSTALL_DOC "doc")
	SET(DIR_INSTALL_DOC_ROOT ".")
	# Installing debug symbols for DLLs in the
	# same directory as the DLL.
	SET(DIR_INSTALL_EXE_DEBUG "${DIR_INSTALL_EXE}")
	SET(DIR_INSTALL_DLL_DEBUG "${DIR_INSTALL_DLL}")
	SET(DIR_INSTALL_LIB_DEBUG "${DIR_INSTALL_LIB}")
ELSE()
	MESSAGE(WARNING "Installation paths have not been set up for this system.")
ENDIF()
