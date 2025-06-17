# RVT-H Tool
CMAKE_MINIMUM_REQUIRED(VERSION 3.5..3.10)

# CMP0048: Set VERSION variables based on the project version specified in PROJECT().
# Introduced in CMake 3.0.
CMAKE_POLICY(SET CMP0048 NEW)

# CMP0063: Honor visibility properties for all target types,
# including static libraries and executables.
# Introduced in CMake 3.3.
CMAKE_POLICY(SET CMP0063 NEW)

SET(RVTHTOOL_VERSION 2.0.0.1)
PROJECT(rvthtool-base VERSION ${RVTHTOOL_VERSION})

# CMAKE_PROJECT_VERSION was introduced in 3.12.
IF(NOT CMAKE_PROJECT_VERSION OR NOT CMAKE_PROJECT_VERSION_MAJOR)
	SET(CMAKE_PROJECT_VERSION ${PROJECT_VERSION})
	SET(CMAKE_PROJECT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
	SET(CMAKE_PROJECT_VERSION_MINOR ${PROJECT_VERSION_MINOR})
	SET(CMAKE_PROJECT_VERSION_PATCH ${PROJECT_VERSION_PATCH})
	SET(CMAKE_PROJECT_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
ENDIF(NOT CMAKE_PROJECT_VERSION OR NOT CMAKE_PROJECT_VERSION_MAJOR)

LIST(APPEND CMAKE_MODULE_PATH
	"${CMAKE_SOURCE_DIR}/cmake/macros"
	"${CMAKE_SOURCE_DIR}/cmake/libs"
	)

# If no build type is set, default to "Debug".
# TODO: Default to "Release"?
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" TMP_BUILD_TYPE)
IF(TMP_BUILD_TYPE STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Release")
ELSEIF(TMP_BUILD_TYPE MATCHES "none")
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF()
UNSET(TMP_BUILD_TYPE)

# Put all the binaries and libraries into a single directory.
# NOTE: CACHE INTERNAL is required in order to get this to work
# for KDE5 for some reason. (and maybe that's why KDE4 did this
# layout by default?)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "Put all binaries in a single directory.")
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE INTERNAL "Put all libraries in a single directory.")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE INTERNAL "Put all archives in a single directory.")

# Enable testing.
# TODO: INCLUDE(CTest) for more advanced testing after
# enough tests are added.
ENABLE_TESTING()

# Set the standards versions: C11, C++17
# C17 was added in CMake 3.21; for older versions, use C11.
# C++17 was added in CMake 3.8; for older versions, use C++14.
# NOTE: These aren't set as hard requirements, though if the compiler
# doesn't support them, code will either be less optimal or will fail
# to compile.
SET(CMAKE_C_STANDARD_REQUIRED OFF)
SET(CMAKE_C_EXTENSIONS ON)
SET(CMAKE_CXX_STANDARD_REQUIRED OFF)
SET(CMAKE_CXX_EXTENSIONS ON)
IF(CMAKE_VERSION VERSION_GREATER 3.20.99)	# >= 3.21
	SET(CMAKE_C_STANDARD 17)
	SET(CMAKE_CXX_STANDARD 17)
ELSEIF(CMAKE_VERSION VERSION_GREATER 3.7.99)	# >= 3.8
	SET(CMAKE_C_STANDARD 11)
	SET(CMAKE_CXX_STANDARD 17)
ELSE()
	SET(CMAKE_C_STANDARD 11)
	SET(CMAKE_CXX_STANDARD 14)
ENDIF()
IF(MSVC)
	# FIXME: Windows SDK prior to 10.0.18362.0 has issues when compiling as either C11 or C17.
	# C:\Program Files (x86)\Windows Kits\8.1\Include\um\winbase.h(8816,5): warning C5105: macro expansion producing 'defined' has undefined behavior
	# C:\Program Files (x86)\Windows Kits\8.1\Include\um\oaidl.h(473,17): warning C5103: pasting '/' and '/' does not result in a valid preprocessing token
	# C:\Program Files (x86)\Windows Kits\8.1\Include\shared\wtypes.h(742,1): message : in expansion of macro '_VARIANT_BOOL'
	# C:\Program Files (x86)\Windows Kits\8.1\Include\um\oaidl.h(473,17): error C2059: syntax error: '/'
	# Reference: https://github.com/microsoft/vcpkg/issues/15035
	SET(CMAKE_C_STANDARD 99)
ENDIF(MSVC)

# Set default build options.
INCLUDE(cmake/options.cmake)
# Check for platform-specific functionality.
INCLUDE(cmake/platform.cmake NO_POLICY_SCOPE)

# Program information.
SET(DESCRIPTION "RVT-H Tool")
SET(PACKAGE_NAME "rvthtool")
SET(AUTHOR "David Korth")
IF(CMAKE_PROJECT_VERSION_PATCH)
	SET(VERSION_STRING "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${CMAKE_PROJECT_VERSION_PATCH}")
ELSE(CMAKE_PROJECT_VERSION_PATCH)
	SET(VERSION_STRING "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")
ENDIF(CMAKE_PROJECT_VERSION_PATCH)
IF(CMAKE_PROJECT_VERSION_TWEAK)
	SET(VERSION_STRING "${VERSION_STRING}+")
ENDIF(CMAKE_PROJECT_VERSION_TWEAK)
SET(VERSION_STRING_WIN32 "${CMAKE_PROJECT_VERSION_MAJOR},${CMAKE_PROJECT_VERSION_MINOR},${CMAKE_PROJECT_VERSION_PATCH},${CMAKE_PROJECT_VERSION_TWEAK}")

# Split Debug macro.
# Also sets the image version for Windows builds.
# TODO: Move to SplitDebugInformation.cmake?
FUNCTION(DO_SPLIT_DEBUG _target)
	GET_TARGET_PROPERTY(_target_type ${_target} TYPE)
	IF(TARGET ${_target} AND NOT _target_type STREQUAL "STATIC_LIBRARY")
		# Split debug information.
		INCLUDE(SetMSVCDebugPath)
		SET_MSVC_DEBUG_PATH(${_target})
		IF(SPLIT_DEBUG)
			INCLUDE(SplitDebugInformation)
			SPLIT_DEBUG_INFORMATION(${_target})
		ENDIF(SPLIT_DEBUG)
		IF(WIN32)
			# Set image version.
			# Subprojects can override ${PROJECT_VERSION_MAJOR} and ${PROJECT_VERSION_MINOR}.
			# FIXME: If minor version is e.g. "3", Windows interprets it as "03",
			# so "1.3" will actually be "1.03".
			INCLUDE(Win32ImageVersionLinkerFlags)
			IF(PROJECT_VERSION_MAJOR AND PROJECT_VERSION_MINOR)
				WIN32_IMAGE_VERSION_LINKER_FLAGS("${PROJECT_VERSION_MAJOR}" "${PROJECT_VERSION_MINOR}")
			ELSE()
				WIN32_IMAGE_VERSION_LINKER_FLAGS("${CMAKE_PROJECT_VERSION_MAJOR}" "${CMAKE_PROJECT_VERSION_MINOR}")
			ENDIF()
		ENDIF(WIN32)
	ENDIF(TARGET ${_target} AND NOT _target_type STREQUAL "STATIC_LIBRARY")
ENDFUNCTION(DO_SPLIT_DEBUG)

# Git version information.
FIND_PROGRAM(POSIX_SH sh)
IF(POSIX_SH)
	# sh is available.
	# Run the git version script.
	IF(WIN32)
		SET(ENV{SHELLOPTS} igncr)
	ENDIF(WIN32)
	ADD_CUSTOM_TARGET(git_version ALL
		${POSIX_SH} "${CMAKE_SOURCE_DIR}/git_version.sh"
		-s "${CMAKE_SOURCE_DIR}"
		-o "${CMAKE_BINARY_DIR}/git_version.h"
		VERBATIM
		)
ELSE(POSIX_SH)
	# sh isn't available.
	# Create a blank git_version.h.
	FILE(WRITE "${CMAKE_BINARY_DIR}/git_version.h"
		"/* dummy file; POSIX sh is not available */\n")
	# Dummy target for dependencies.
	ADD_CUSTOM_TARGET(git_version)
ENDIF(POSIX_SH)

# Make sure the file is deleted on `make clean`.
SET_PROPERTY(DIRECTORY APPEND
	PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_BINARY_DIR}/git_version.h")

# Reference: https://cmake.org/Wiki/RecipeAddUninstallTarget
########### Add uninstall target ###############
CONFIGURE_FILE(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
	"${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake")

### Subdirectories. ###

# Translations.
IF(ENABLE_NLS)
	# NOTE: ENABLE_NLS only controls whether or not translations
	# are built. TranslationManager and related are always built
	# regardless.
	ADD_SUBDIRECTORY(locale)
ENDIF(ENABLE_NLS)

# Project subdirectories.
ADD_SUBDIRECTORY(extlib)
ADD_SUBDIRECTORY(src)
IF(INSTALL_DOC)
	ADD_SUBDIRECTORY(doc)
ENDIF(INSTALL_DOC)

# TODO: Print build summary indicating what plugins will be built.
# (Some other project had something like this...)
# TODO: Fail if no plugins are being built.

# CPack settings.
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${DESCRIPTION}")
SET(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
SET(CPACK_PACKAGE_VENDOR "${AUTHOR}")
SET(CPACK_PACKAGE_CONTACT "David Korth <gerbilsoft@gerbilsoft.com>")
SET(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
SET(CPACK_PACKAGE_VERSION ${VERSION_STRING})

# TODO: DESCRIPTION and WELCOME files.
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/doc/DESCRIPTION.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
#SET(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_SOURCE_DIR}/doc/WELCOME.txt")

# CPack: Source package settings.
# NOTE: Double-escape is required because the unescaped
# string # is written to CPackSourceConfig.cmake, which
# is then unescaped.
SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_SOURCE_IGNORE_FILES
	"build.*/"
	"build.*\\\\.sh"
	"\\\\.git/"
	"\\\\.gitignore"
	"*\\\\.kate-swp"
	)

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.|x)86\$")
	SET(CPACK_PACKAGE_SYSTEM_PROCESSOR "i386")
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86_64\$")
	SET(CPACK_PACKAGE_SYSTEM_PROCESSOR "amd64")
ELSE()
	SET(CPACK_PACKAGE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
ENDIF()

IF(APPLE)
	# TODO: Support for Mac OS X.
ELSEIF(WIN32)
	IF(MSVC AND CMAKE_CL_64)
		SET(WIN32_PACKAGE_SUFFIX "win64")
	ELSEIF(NOT MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
		SET(WIN32_PACKAGE_SUFFIX "win64")
	ELSE()
		SET(WIN32_PACKAGE_SUFFIX "win32")
	ENDIF()

	SET(CPACK_GENERATOR "ZIP")
	SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${WIN32_PACKAGE_SUFFIX}")
	SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
ENDIF()

# Components.
SET(CPACK_COMPONENTS_ALL dll program debug doc i18n)
SET(CPACK_COMPONENT_DLL_DISPLAY_NAME "DLLs")
SET(CPACK_COMPONENT_PROGRAM_DISPLAY_NAME "Programs")
SET(CPACK_COMPONENT_DEBUG_DISPLAY_NAME "Debugging Symbols")
SET(CPACK_COMPONENT_DOC_DISPLAY_NAME "Documents")
SET(CPACK_COMPONENT_I18N_DISPLAY_NAME "Internationalization")

# Initialize CPack.
INCLUDE(CPack)
