# Build options.

OPTION(ENABLE_WERROR "Treat all compile warnings as errors. (Enable for development!)" OFF)

# Enable UDEV on Linux
IF(UNIX AND NOT APPLE)
	OPTION(ENABLE_UDEV "Enable UDEV for the 'query' command." ON)
ELSE()
	SET(ENABLE_UDEV OFF CACHE INTERNAL "Enable UDEV for the 'query' command." FORCE)
ENDIF()

# Enable D-Bus for DockManager / Unity API
IF(UNIX AND NOT APPLE)
	OPTION(ENABLE_DBUS "Enable D-Bus support for DockManager / Unity API." 1)
ELSE(UNIX AND NOT APPLE)
	SET(ENABLE_DBUS 0)
ENDIF(UNIX AND NOT APPLE)

# Link-time optimization
# FIXME: Not working in clang builds and Ubuntu's gcc...
IF(MSVC)
	SET(LTO_DEFAULT ON)
ELSE()
	SET(LTO_DEFAULT OFF)
ENDIF()
OPTION(ENABLE_LTO "Enable link-time optimization in release builds." ${LTO_DEFAULT})

# Split debug information into a separate file
# FIXME: macOS `strip` shows an error:
# error: symbols referenced by indirect symbol table entries that can't be stripped in: [library]
IF(APPLE)
	OPTION(SPLIT_DEBUG "Split debug information into a separate file." OFF)
ELSE(APPLE)
	OPTION(SPLIT_DEBUG "Split debug information into a separate file." ON)
ENDIF(APPLE)

# Install the split debug file
OPTION(INSTALL_DEBUG "Install the split debug files." ON)
IF(INSTALL_DEBUG AND NOT SPLIT_DEBUG)
	# Cannot install debug files if we're not splitting them.
	SET(INSTALL_DEBUG OFF CACHE INTERNAL "Install the split debug files." FORCE)
ENDIF(INSTALL_DEBUG AND NOT SPLIT_DEBUG)

# Qt version
SET(QT_VERSION AUTO CACHE STRING "Qt version to use (default is 'AUTO' to auto-detect Qt6 or Qt5)")
SET_PROPERTY(CACHE QT_VERSION PROPERTY STRINGS AUTO 6 5)

# Enable KDE integration (defaults to ON on Linux)
IF(WIN32 OR APPLE)
	SET(ENABLE_KDE_DEFAULT OFF)
ELSE(WIN32 OR APPLE)
	SET(ENABLE_KDE_DEFAULT ON)
ENDIF(WIN32 OR APPLE)
OPTION(ENABLE_KDE "Enable KDE integration." ${ENABLE_KDE_DEFAULT})

# Translations
OPTION(ENABLE_NLS "Enable NLS using Qt's built-in localization system." ON)

# Special handling for NixOS
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	OPTION(ENABLE_NIXOS "Enable special handling for NixOS builds." OFF)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Linux")

# Install documentation
OPTION(INSTALL_DOC "Install documentation." ON)
