PROJECT(src)

# Check for Large File Support.
# NOTE: This should have been done in platform.cmake, but we're
# including it here because the off_t/off64_t sizes are written
# to config.libc.h.
INCLUDE(CheckLargeFileSupport)
CHECK_LARGE_FILE_SUPPORT()

# Check for reentrant time functions.
# NOTE: May be _gmtime32_s() or _gmtime64_s() on MSVC 2005+.
# The "inline" part will detect that.
INCLUDE(CheckSymbolExistsOrInline)
CHECK_SYMBOL_EXISTS_OR_INLINE(gmtime_r "time.h" "time_t tm; gmtime_r(&tm, NULL);" HAVE_GMTIME_R)
IF(NOT HAVE_GMTIME_R)
	CHECK_SYMBOL_EXISTS_OR_INLINE(gmtime_s "time.h" "time_t tm; gmtime_s(NULL, &tm);" HAVE_GMTIME_S)
ENDIF(NOT HAVE_GMTIME_R)
CHECK_SYMBOL_EXISTS_OR_INLINE(localtime_r "time.h" "time_t tm; localtime_r(&tm, NULL);" HAVE_LOCALTIME_R)
IF(NOT HAVE_LOCALTIME_R)
	CHECK_SYMBOL_EXISTS_OR_INLINE(localtime_s "time.h" "time_t tm; localtime_s(NULL, &tm);" HAVE_LOCALTIME_S)
ENDIF(NOT HAVE_LOCALTIME_R)

# Other time functions.
CHECK_SYMBOL_EXISTS_OR_INLINE(timegm "time.h" "struct tm tm; time_t x = timegm(&tm);" HAVE_TIMEGM)
IF(NOT HAVE_TIMEGM)
	# NOTE: MSVCRT's _mkgmtime64() has a range of [1970/01/01, 3000/12/31].
	# glibc and boost both support arbitrary ranges.
	CHECK_SYMBOL_EXISTS_OR_INLINE(_mkgmtime "time.h" "struct tm tm; time_t x = _mkgmtime(&tm);" HAVE__MKGMTIME)
	CHECK_SYMBOL_EXISTS_OR_INLINE(_mkgmtime32 "time.h" "struct tm tm; time_t x = _mkgmtime(&tm);" HAVE__MKGMTIME32)
	CHECK_SYMBOL_EXISTS_OR_INLINE(_mkgmtime64 "time.h" "struct tm tm; time_t x = _mkgmtime64(&tm);" HAVE__MKGMTIME64)
ENDIF(NOT HAVE_TIMEGM)
IF(NOT HAVE_TIMEGM AND NOT HAVE__MKGMTIME AND NOT HAVE__MKGMTIME32 AND NOT HAVE__MKGMTIME64)
	MESSAGE(FATAL_ERROR "timegm() or equivalent function not found.")
ENDIF(NOT HAVE_TIMEGM AND NOT HAVE__MKGMTIME AND NOT HAVE__MKGMTIME32 AND NOT HAVE__MKGMTIME64)

IF(NOT WIN32)
	# Check for statx().
	SET(OLD_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
	SET(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
	CHECK_SYMBOL_EXISTS(statx "sys/stat.h" HAVE_STATX)
	SET(CMAKE_REQUIRED_DEFINITIONS "${OLD_CMAKE_REQUIRED_DEFINITIONS}")
	UNSET(OLD_CMAKE_REQUIRED_DEFINITIONS)
ENDIF(NOT WIN32)

# Check for stdbool.h.
INCLUDE(CheckIncludeFile)
CHECK_INCLUDE_FILE("stdbool.h" HAVE_STDBOOL_H)

# Write the libc configuration file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.libc.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.libc.h")

# Write stdboolx.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/stdboolx.h.in" "${CMAKE_CURRENT_BINARY_DIR}/stdboolx.h")

# C++11 compatibility header.
# NOTE: This must be included regardless of C++11 support in the compiler.
# gcc-4.6 supports some C++11, but is missing explicit virtual overrides.
SET(RP_CONFIG_LIBC_H "${CMAKE_CURRENT_BINARY_DIR}/config.libc.h")
SET(RP_CXX11_COMPAT_H "${CMAKE_CURRENT_SOURCE_DIR}/c++11-compat.h")
IF(MSVC)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -FI${RP_CONFIG_LIBC_H} -FI${RP_CXX11_COMPAT_H}")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -FI${RP_CONFIG_LIBC_H} -FI${RP_CXX11_COMPAT_H}")
ELSE(MSVC)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -include ${RP_CONFIG_LIBC_H} -include ${RP_CXX11_COMPAT_H}")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -include ${RP_CONFIG_LIBC_H} -include ${RP_CXX11_COMPAT_H}")
ENDIF(MSVC)

# Check for hidden visibility flags.
INCLUDE(CheckHiddenVisibility)
CHECK_HIDDEN_VISIBILITY()

# Find nettle.
FIND_PACKAGE(NETTLE REQUIRED)
SET(HAVE_NETTLE 1)
IF(NOT WIN32)
	# Find GMP.
	# On Windows, we're using nettle's mini-GMP.
	FIND_PACKAGE(GMP REQUIRED)
	SET(HAVE_GMP 1)
ENDIF(NOT WIN32)

# Check if this is Nettle 3.x.
# Nettle 3.1 added version.h, which isn't available
# in older verisons, so we can't simply check that.
INCLUDE(CheckSymbolExists)
SET(OLD_CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
SET(OLD_CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
SET(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES} ${NETTLE_INCLUDE_DIRS}")
SET(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${NETTLE_LIBRARY}")
CHECK_SYMBOL_EXISTS(aes128_set_decrypt_key "nettle/aes.h" HAVE_NETTLE_3)
IF(HAVE_NETTLE_3)
	# Check for Nettle versioning symbols.
	# Nettle 3.1 added version.h.
	CHECK_SYMBOL_EXISTS(NETTLE_VERSION_MAJOR "nettle/version.h" HAVE_NETTLE_VERSION_H)
	CHECK_SYMBOL_EXISTS(nettle_version_major "nettle/version.h" HAVE_NETTLE_VERSION_FUNCTIONS)
ENDIF(HAVE_NETTLE_3)
SET(CMAKE_REQUIRED_INCLUDES "${OLD_CMAKE_REQUIRED_INCLUDES}")
SET(CMAKE_REQUIRED_LIBRARIES "${OLD_CMAKE_REQUIRED_LIBRARIES}")
UNSET(OLD_CMAKE_REQUIRED_INCLUDES)
UNSET(OLD_CMAKE_REQUIRED_LIBRARIES)

# Write the version number to config.version.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.version.h")
# Write the Nettle configuration to config.nettle.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.nettle.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.nettle.h")

# Source Code subdirectories.
ADD_SUBDIRECTORY(libwiicrypto)
ADD_SUBDIRECTORY(librvth)
ADD_SUBDIRECTORY(rvthtool)
ADD_SUBDIRECTORY(qrvthtool)
ADD_SUBDIRECTORY(wadresign)
ADD_SUBDIRECTORY(nusresign)
