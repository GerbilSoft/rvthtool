/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_recrypt.h: RVT-H "recryption" functions.                           *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_RECRYPT_H__
#define __RVTHTOOL_LIBRVTH_RVTH_RECRYPT_H__

#include "cert_store.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _RvtH;

/**
 * Re-encrypt partitions in a Wii disc image.
 *
 * This operation will *wipe* the update partition, since installing
 * retail updates on a debug system and vice-versa can result in a brick.
 *
 * NOTE: This function only supports converting from one encryption to
 * another. It does not support converting unencrypted to encrypted or
 * vice-versa.
 *
 * NOTE 2: If the disc image is already encrypted using the specified key,
 * the function will return immediately with a success code (0).
 *
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param toKey	[in] Key to use for re-encryption.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_recrypt_partitions(struct _RvtH *rvth, unsigned int bank, RVL_AES_Keys_e toKey);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_RECRYPT_H__ */
