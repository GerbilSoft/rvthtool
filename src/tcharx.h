/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * tcharx.h: TCHAR support for Windows and Linux.                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#ifdef _WIN32

// Windows: Use the SDK tchar.h.
#include <tchar.h>

// std::tstring
#ifdef _UNICODE
# define tstring wstring
#else /* !_UNICODE */
# define tstring string
#endif /* _UNICODE */

#else /* !_WIN32 */

// Other systems: Define TCHAR and related macros.
typedef char TCHAR;
#define _T(x) x
#define _tmain main
#define tstring string

// ctype.h
#define _istalpha(c) isalpha(c)

// stdio.h
#define _fputts(s, stream) fputs((s), (stream))
#define _fputtc(c, stream) fputc((c), (stream))

#define _tfopen(filename, mode)		fopen((filename), (mode))
#define _tmkdir(path, mode)		mkdir((path), (mode))
#define _tremove(pathname)		remove(pathname)

#define _tprintf printf
#define _ftprintf fprintf
#define _sntprintf snprintf
#define _vtprintf vprintf
#define _vftprintf vfprintf
#define _vsprintf vsprintf

// stdlib.h
#define _tcscmp(s1, s2)			strcmp((s1), (s2))
#define _tcsicmp(s1, s2)		strcasecmp((s1), (s2))
#define _tcsnicmp(s1, s2)		strncasecmp((s1), (s2), (n))
#define _tcstol(nptr, endptr, base)	strtol((nptr), (endptr), (base))
#define _tcstoul(nptr, endptr, base)	strtoul((nptr), (endptr), (base))

// string.h
#define _tcschr(s, c)			strchr((s), (c))
#define _tcscmp(s1, s2)			strcmp((s1), (s2))
#define _tcsdup(s)			strdup(s)
#define _tcserror(errnum)		strerror(errnum)
#define _tcslen(s)			strlen(s)
#define _tcsncmp(s1, s2, n)		strncmp((s1), (s2), (n))
#define _tcsrchr(s, c)			strrchr((s), (c))

#endif /* _WIN32 */
