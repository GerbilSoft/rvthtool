/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * aesw_nettle.c: AES wrapper functions. (nettle version)                  *
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

#include "config.nettle.h"

#include "aesw.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Nettle AES functions.
#include <nettle/aes.h>
#include <nettle/cbc.h>

// AES context. (GNU Nettle version.)
struct _AesCtx {
#ifdef HAVE_NETTLE_3
	struct aes128_ctx ctx;
#else /* !HAVE_NETTLE_3 */
	struct aes_ctx ctx;
#endif /* HAVE_NETTLE_3 */

	// Encryption key.
	uint8_t key[16];
	// Initialization vector.
	uint8_t iv[16];
};

/**
 * Create an AES context.
 * @return AES context, or NULL on error.
 */
AesCtx *aesw_new(void)
{
	// Allocate an AES context.
	AesCtx *aesw = calloc(1, sizeof(*aesw));
	if (!aesw) {
		// Could not allocate memory.
		errno = ENOMEM;
		return NULL;
	}

	// AES context has been initialized.
	return aesw;
}

/**
 * Free an AES context
 * @param aesw AES context.
 */
void aesw_free(AesCtx *aesw)
{
	free(aesw);
}

/**
 * Set the AES key.
 * @param aesw	[in] AES context.
 * @param pKey	[in] Key data.
 * @param size	[in] Size of pKey, in bytes.
 * @return 0 on success; negative POSIX error code on error.
 */
int aesw_set_key(AesCtx *aesw, const uint8_t *pKey, size_t size)
{
	if (!aesw || !pKey || size != 16) {
		return -EINVAL;
	}

	memcpy(aesw->key, pKey, size);
	return 0;
}

/**
 * Set the AES IV.
 * @param aesw	[in] AES context.
 * @param pIV	[in] IV data.
 * @param size	[in] Size of pIV, in bytes.
 * @return 0 on success; negative POSIX error code on error.
 */
int aesw_set_iv(AesCtx *aesw, const uint8_t *pIV, size_t size)
{
	if (!aesw || !pIV || size != 16) {
		return -EINVAL;
	}

	memcpy(aesw->iv, pIV, size);
	return 0;
}

/**
 * Encrypt a block of data using the current parameters.
 * @param aesw	[in] AES context.
 * @param pData	[in/out] Data block.
 * @param size	[in] Length of data block. (Must be a multiple of 16.)
 * @return Number of bytes encrypted on success; 0 on error.
 */
size_t aesw_encrypt(AesCtx *aesw, uint8_t *pData, size_t size)
{
	if (!aesw || !pData || (size % 16 != 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return 0;
	}

	// TODO: Only set encrypt key if switching modes?
#ifdef HAVE_NETTLE_3
	aes128_set_encrypt_key(&aesw->ctx, aesw->key);
	cbc_encrypt(&aesw->ctx, (nettle_cipher_func*)aes128_encrypt,
		AES_BLOCK_SIZE, aesw->iv, size, pData, pData);
#else /* !HAVE_NETTLE_3 */
	aes_set_encrypt_key(&aesw->ctx, sizeof(aesw->key), aesw->key);
	cbc_encrypt(&aesw->ctx, (nettle_crypt_func*)aes_encrypt,
		AES_BLOCK_SIZE, aesw->iv, size, pData, pData);
#endif /* HAVE_NETTLE_3 */

	return size;
}

/**
 * Decrypt a block of data using the current parameters.
 * @param aesw	[in] AES context.
 * @param pData	[in/out] Data block.
 * @param size	[in] Length of data block. (Must be a multiple of 16.)
 * @return Number of bytes encrypted on success; 0 on error.
 */
size_t aesw_decrypt(AesCtx *aesw, uint8_t *pData, size_t size)
{
	if (!aesw || !pData || (size % 16 != 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return 0;
	}

	// TODO: Only set decrypt key if switching modes?
#ifdef HAVE_NETTLE_3
	aes128_set_decrypt_key(&aesw->ctx, aesw->key);
	cbc_decrypt(&aesw->ctx, (nettle_cipher_func*)aes128_decrypt,
		AES_BLOCK_SIZE, aesw->iv, size, pData, pData);
#else /* !HAVE_NETTLE_3 */
	aes_set_decrypt_key(&aesw->ctx, sizeof(aesw->key), aesw->key);
	cbc_decrypt(&aesw->ctx, (nettle_crypt_func*)aes_decrypt,
		AES_BLOCK_SIZE, aesw->iv, size, pData, pData);
#endif /* HAVE_NETTLE_3 */

	return size;
}
