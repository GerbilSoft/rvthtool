PROJECT(src)

# C99 compatibility header for MSVC.
IF(MSVC)
        SET(C99_COMPAT_HEADER "-FI${CMAKE_CURRENT_SOURCE_DIR}/c99-compat.msvcrt.h")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C99_COMPAT_HEADER}")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C99_COMPAT_HEADER}")
ENDIF(MSVC)

# Check for hidden visibility flags.
INCLUDE(CheckHiddenVisibility)
CHECK_HIDDEN_VISIBILITY()

# Write the version number to config.version.h.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.version.h")

# Source Code subdirectories.
ADD_SUBDIRECTORY(librvth)
ADD_SUBDIRECTORY(rvthtool)
ADD_SUBDIRECTORY(qrvthtool)
