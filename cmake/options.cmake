# Build options.

# Enable UDEV on Linux.
IF(UNIX AND NOT APPLE)
	OPTION(ENABLE_UDEV "Enable UDEV for the 'query' command." ON)
ELSE()
	SET(ENABLE_UDEV OFF CACHE INTERNAL "Enable UDEV for the 'query' command." FORCE)
ENDIF()

# Enable D-Bus for DockManager / Unity API.
IF(UNIX AND NOT APPLE)
	OPTION(ENABLE_DBUS	"Enable D-Bus support for DockManager / Unity API." 1)
ELSE(UNIX AND NOT APPLE)
	SET(ENABLE_DBUS 0)
ENDIF(UNIX AND NOT APPLE)

# Link-time optimization.
# FIXME: Not working in clang builds and Ubuntu's gcc...
IF(MSVC)
	SET(LTO_DEFAULT ON)
ELSE()
	SET(LTO_DEFAULT OFF)
ENDIF()
OPTION(ENABLE_LTO "Enable link-time optimization in release builds." ${LTO_DEFAULT})

# Split debug information into a separate file.
OPTION(SPLIT_DEBUG "Split debug information into a separate file." ON)

# Install the split debug file.
OPTION(INSTALL_DEBUG "Install the split debug files." ON)
IF(INSTALL_DEBUG AND NOT SPLIT_DEBUG)
	# Cannot install debug files if we're not splitting them.
	SET(INSTALL_DEBUG OFF CACHE INTERNAL "Install the split debug files." FORCE)
ENDIF(INSTALL_DEBUG AND NOT SPLIT_DEBUG)

# Translations.
OPTION(ENABLE_NLS "Enable NLS using Qt's built-in localization system." ON)
