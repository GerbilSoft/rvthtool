/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * hashw.h: Cryptographic hash wrapper functions.                          *
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

#ifndef __RVTHTOOL_LIBRVTH_HASHW_H__
#define __RVTHTOOL_LIBRVTH_HASHW_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SHA-1 hash length.
#define SHA1_HASH_LENGTH 20

struct _HashCtx_SHA1;
typedef struct _HashCtx_SHA1 HashCtx_SHA1;

/**
 * Create an SHA-1 hash object.
 * @return SHA-1 hash object, or NULL on error.
 */
HashCtx_SHA1 *hashw_sha1_new(void);

/**
 * Free an SHA-1 hash object.
 * @param sha1 SHA-1 hash object.
 */
void hashw_sha1_free(HashCtx_SHA1 *sha1);

/**
 * Update an SHA-1 hash object with new data.
 * @param sha1	[in] SHA-1 hash object.
 * @param data	[in] Data to hash.
 * @param size	[in] Size of `data`.
 * @return 0 on success; negative POSIX error code on error.
 */
int hashw_sha1_update(HashCtx_SHA1 *sha1, const uint8_t *data, size_t size);

/**
 * Calculate the final SHA-1 hash.
 *
 * NOTE: The SHA-1 hash object is NOT freed when calling this function.
 * Its internal state will be reset and it will be usable for calculating
 * another SHA-1 hash.
 *
 * @param sha1		[in] SHA-1 hash object.
 * @param pHash		[out] SHA-1 hash buffer. (Must be SHA1_HASH_LENGTH bytes.)
 * @return 0 on success; negative POSIX error code on error.
 */
int hashw_sha1_finish(HashCtx_SHA1 *sha1, uint8_t *pHash);

/**
 * Calculate an SHA-1 hash.
 *
 * This is a convenience wrapper function that creates a
 * temporary SHA-1 hash object.
 *
 * @param pHash	[out] SHA-1 hash buffer. (Must be SHA1_HASH_LENGTH bytes.)
 * @param data	[in] Data to hash.
 * @param size	[in] Size of `data`.
 * @return 0 on success; negative POSIX error code on error.
 */
int hashw_sha1(uint8_t *pHash, const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_HASHW_H__ */
