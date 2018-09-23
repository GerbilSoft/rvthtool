/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * common.h: Common types and macros.                                      *
 *                                                                         *
 * Copyright (c) 2016-2018 by David Korth.                                 *
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

#ifndef __RVTHTOOL_LIBWIICRYPTO_COMMON_H__
#define __RVTHTOOL_LIBWIICRYPTO_COMMON_H__

/**
 * Number of elements in an array.
 *
 * Includes a static check for pointers to make sure
 * a dynamically-allocated array wasn't specified.
 * Reference: http://stackoverflow.com/questions/8018843/macro-definition-array-size
 */
#define ARRAY_SIZE(x) \
	((int)(((sizeof(x) / sizeof(x[0]))) / \
		(size_t)(!(sizeof(x) % sizeof(x[0])))))

// PACKED struct attribute.
// Use in conjunction with #pragma pack(1).
#ifdef __GNUC__
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

/**
 * static_asserts size of a structure
 * Also defines a constant of form StructName_SIZE
 */
// TODO: Check MSVC support for static_assert() in C mode.
#ifndef ASSERT_STRUCT
# if defined(__cplusplus)
#  define ASSERT_STRUCT(st,sz) enum { st##_SIZE = (sz), }; \
	static_assert(sizeof(st)==(sz),#st " is not " #sz " bytes.")
# elif defined(__GNUC__) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define ASSERT_STRUCT(st,sz) enum { st##_SIZE = (sz), }; \
	_Static_assert(sizeof(st)==(sz),#st " is not " #sz " bytes.")
# else
#  define ASSERT_STRUCT(st, sz)
# endif
#endif

// RP equivalent of Q_UNUSED().
#define UNUSED(x) ((void)x)

// Deprecated function attribute.
#ifndef DEPRECATED
# if defined(__GNUC__)
#  define DEPRECATED __attribute__ ((deprecated))
# elif defined(_MSC_VER)
#  define DEPRECATED __declspec(deprecated)
# else
#  define DEPRECATED
# endif
#endif

// Force inline attribute.
#if !defined(FORCEINLINE)
# if (!defined(_DEBUG) || defined(NDEBUG))
#  if defined(__GNUC__)
#   define FORCEINLINE inline __attribute__((always_inline))
#  elif defined(_MSC_VER)
#   define FORCEINLINE __forceinline
#  else
#   define FORCEINLINE inline
#  endif
# else
#  ifdef _MSC_VER
#   define FORCEINLINE __inline
#  else
#   define FORCEINLINE inline
#  endif
# endif
#endif /* !defined(FORCEINLINE) */

// gcc branch prediction hints.
// Should be used in combination with profile-guided optimization.
#ifdef __GNUC__
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely(x)	__builtin_expect(!!(x), 0)
#else
# define likely(x)	x
# define unlikely(x)	x
#endif

// C99 restrict macro.
// NOTE: gcc only defines restrict in C, not C++,
// so use __restrict on both gcc and MSVC.
#define RESTRICT __restrict

// typeof() for MSVC.
#ifdef _MSC_VER
# define __typeof__(x) decltype(x)
#endif

/**
 * Alignment macro.
 * @param a	Alignment value.
 * @param x	Byte count to align.
 */
#define ALIGN(a, x)	(((x)+((a)-1))&~((__typeof__(x))((a)-1)))

/**
 * Alignment assertion macro.
 */
#define ASSERT_ALIGNMENT(a, ptr)	assert(reinterpret_cast<uintptr_t>(ptr) % 16 == 0);

// TODO: Move to our own stdboolx.h file.
// TODO: Move to another file.
#ifndef __cplusplus
# if defined(_MSC_VER) && _MSC_VER >= 1800
#  include <stdbool.h>
# else
typedef unsigned char bool;
#  define true 1
#  define false 0
# endif
#endif /* __cplusplus */

#endif /* __RVTHTOOL_LIBWIICRYPTO_COMMON_H__ */
