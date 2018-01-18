/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * tcharx.h: TCHAR support for Windows and Linux.                          *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ***************************************************************************/

#ifndef __RVTHTOOL_LIBRVTH_TCHAR_H__
#define __RVTHTOOL_LIBRVTH_TCHAR_H__

#ifdef _WIN32

// Windows: Use the SDK tchar.h.
#include <tchar.h>

#else /* !_WIN32 */

// Other systems: Define TCHAR and related macros.
typedef char TCHAR;
#define _T(x) x
#define _tmain main

// ctype.h
#define _istalpha(c) isalpha(c)

// stdio.h
#define _fputts(s, stream) fputs(s, stream)

#define _tfopen(filename, mode) fopen((filename), (mode))

#define _tprintf printf
#define _ftprintf fprintf
#define _sntprintf snprintf

// stdlib.h
#define _tcscmp(s1, s2) strcmp(s1, s2)
#define _tcstoul(nptr, endptr, base) strtoul(nptr, endptr, base)

// string.h
#define _tcsdup(s) strdup(s)

#endif

#endif /* __RVTHTOOL_LIBRVTH_TCHAR_H__ */
