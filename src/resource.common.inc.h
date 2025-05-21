/***************************************************************************
 * RVT-H Tool                                                              *
 * resource.common.inc.h: Common Win32 resource header.                    *
 *                                                                         *
 * Copyright (c) 2016-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#ifndef RC_INVOKED
# error This file should only be included in Win32 resource scripts.
#endif

// Windows SDK.
#include <windows.h>

/* program version */
#include "config.version.h"
/* git version */
#include "git.h"

/* Windows-style version number */
#define VERSION_WIN32 VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,VERSION_DEVEL

/* Windows-style version string */
#ifdef RP_GIT_VERSION
# ifdef RP_GIT_DESCRIBE
#  define Win32_RC_FileVersion VERSION_STRING "\r\n" RP_GIT_VERSION "\r\n" RP_GIT_DESCRIBE
# else /* !RP_GIT_DESCRIBE */
#  define Win32_RC_FileVersion VERSION_STRING "\r\n" RP_GIT_VERSION
# endif /* RP_GIT_DESCRIBE */
#else /* !RP_GIT_VERSION */
# define Win32_RC_FileVersion VERSION_STRING
#endif /* RP_GIT_VERSION */

/* File flags for VERSIONINFO */
#ifdef _DEBUG
# define RP_VS_FF_DEBUG VS_FF_DEBUG
#else
# define RP_VS_FF_DEBUG 0
#endif
#if VERSION_DEVEL != 0
# define RP_VS_FF_PRERELEASE VS_FF_PRERELEASE
#else
# define RP_VS_FF_PRERELEASE 0
#endif

#define RP_VS_FILEFLAGS (RP_VS_FF_DEBUG | RP_VS_FF_PRERELEASE)
