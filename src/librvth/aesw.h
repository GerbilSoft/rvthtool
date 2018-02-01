/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * aesw.h: AES wrapper functions.                                          *
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

// NOTE: Currently only supports AES-128-CBC.

#ifndef __RVTHTOOL_LIBRVTH_AESW_H__
#define __RVTHTOOL_LIBRVTH_AESW_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AesCtx;
typedef struct _AesCtx AesCtx;

/**
 * Create an AES context.
 * @return AES context, or NULL on error.
 */
AesCtx *aesw_new(void);

/**
 * Free an AES context
 * @param aesw AES context.
 */
void aesw_free(AesCtx *aesw);

/**
 * Set the AES key.
 * @param aesw	[in] AES context.
 * @param pKey	[in] Key data.
 * @param size	[in] Size of pKey, in bytes.
 * @return 0 on success; negative POSIX error code on error.
 */
int aesw_set_key(AesCtx *aesw, const uint8_t *pKey, size_t size);

/**
 * Set the AES IV.
 * @param aesw	[in] AES context.
 * @param pIV	[in] IV data.
 * @param size	[in] Size of pIV, in bytes.
 * @return 0 on success; negative POSIX error code on error.
 */
int aesw_set_iv(AesCtx *aesw, const uint8_t *pIV, size_t size);

/**
 * Encrypt a block of data using the current parameters.
 * @param aesw	[in] AES context.
 * @param pData	[in/out] Data block.
 * @param size	[in] Length of data block. (Must be a multiple of 16.)
 * @return Number of bytes encrypted on success; 0 on error.
 */
size_t aesw_encrypt(AesCtx *aesw, uint8_t *pData, size_t size);

/**
 * Decrypt a block of data using the current parameters.
 * @param aesw	[in] AES context.
 * @param pData	[in/out] Data block.
 * @param size	[in] Length of data block. (Must be a multiple of 16.)
 * @return Number of bytes encrypted on success; 0 on error.
 */
size_t aesw_decrypt(AesCtx *aesw, uint8_t *pData, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_HASHW_H__ */
