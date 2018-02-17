/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * extract_crypt.h: Extract and encrypt an unencrypted image.              *
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

#ifndef __RVTHTOOL_LIBRVTH_EXTRACT_CRYPT_H__
#define __RVTHTOOL_LIBRVTH_EXTRACT_CRYPT_H__

#include "rvth.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to a writable standalone disc image.
 *
 * This function copies an unencrypted Game Partition and encrypts it
 * using the existing title key. It does *not* change the encryption
 * method or signature, so recryption will be needed afterwards.
 *
 * @param rvth_dest	[out] Destination RvtH object.
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_gcm_doCrypt(RvtH *rvth_dest, const RvtH *rvth_src,
	unsigned int bank_src, RvtH_Progress_Callback callback);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_EXTRACT_CRYPT_H__ */
