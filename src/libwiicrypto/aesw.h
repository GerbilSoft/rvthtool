/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * aesw.h: AES wrapper functions.                                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// NOTE: Currently only supports AES-128-CBC.

#pragma once

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
