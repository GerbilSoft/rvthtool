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

#include "hashw.h"

#include <errno.h>

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
int hashw_sha1(uint8_t *pHash, const uint8_t *data, size_t size)
{
	int ret;
	HashCtx_SHA1 *sha1 = hashw_sha1_new();
	if (!sha1) {
		return -errno;
	}
	ret = hashw_sha1_update(sha1, data, size);
	if (ret != 0) {
		hashw_sha1_free(sha1);
		return ret;
	}
	ret = hashw_sha1_finish(sha1, pHash);
	hashw_sha1_free(sha1);
	return ret;
}
