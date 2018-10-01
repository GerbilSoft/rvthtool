# Build options.

# Enable UDEV on Linux.
IF(UNIX AND NOT APPLE)
	OPTION(ENABLE_UDEV "Enable UDEV for the 'query' command." ON)
ELSE()
	SET(ENABLE_UDEV OFF CACHE "Enable UDEV for the 'query' command." INTERAL FORCE)
ENDIF()

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
	SET(INSTALL_DEBUG OFF CACHE "Install the split debug files." INTERNAL FORCE)
ENDIF(INSTALL_DEBUG AND NOT SPLIT_DEBUG)

# Translations.
OPTION(ENABLE_NLS "Enable NLS using Qt's built-in localization system." ON)
