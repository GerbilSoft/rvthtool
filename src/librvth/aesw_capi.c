/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * aesw_capi.c: AES wrapper functions. (CryptoAPI version)                 *
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

#include "aesw.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <windows.h>
#include <wincrypt.h>

// AES context. (Microsoft CryptoAPI version.)
struct _AesCtx {
	HCRYPTPROV hProv;
	HCRYPTKEY hKey;
};

/**
 * Create an AES context.
 * @return AES context, or NULL on error.
 */
AesCtx *aesw_new(void)
{
	// Allocate an AES context.
	AesCtx *aesw = malloc(sizeof(*aesw));
	if (!aesw) {
		// Could not allocate memory.
		errno = ENOMEM;
		return NULL;
	}

	// Open the crypto provider.
	if (!CryptAcquireContext(&aesw->hProv, NULL, NULL,
	    PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
	{
		// TODO: Convert GetLastError() to errno?
		free(aesw);
		errno = EIO;
		return NULL;
	}

	// AES key will be created when it's set.
	aesw->hKey = 0;

	// AES context has been initialized.
	return aesw;
}

/**
 * Free an AES context
 * @param aesw AES context.
 */
void aesw_free(AesCtx *aesw)
{
	if (aesw) {
		if (aesw->hKey != 0) {
			CryptDestroyKey(aesw->hKey);
		}
		CryptReleaseContext(aesw->hProv, 0);
		free(aesw);
	}
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
	HCRYPTKEY hNewKey, hOldKey;
	DWORD dwMode;
	DWORD blobSize;
	struct aesblob {
		BLOBHEADER hdr;
		DWORD keySize;
		BYTE bytes[32];	// maximum size
	} blob;

	if (!aesw || !pKey || size != 16) {
		return -EINVAL;
	}

	// Create an AES key blob.
	// Reference: http://stackoverflow.com/questions/842357/hard-coded-aes-256-key-with-wincrypt-cryptimportkey
	blob.hdr.bType = PLAINTEXTKEYBLOB;
	blob.hdr.bVersion = CUR_BLOB_VERSION;
	blob.hdr.reserved = 0;
	blob.hdr.aiKeyAlg = CALG_AES_128;
	blob.keySize = (DWORD)size;
	memcpy(blob.bytes, pKey, size);

	// Blob size.
	blobSize = (DWORD)(sizeof(blob.hdr) + sizeof(blob.keySize) + size);

	// Load the key.
	if (!CryptImportKey(aesw->hProv, (BYTE*)&blob, blobSize, 0, 0, &hNewKey)) {
		// Error loading the key.
		// TODO: Convert GetLastError() to errno?
		return -EIO;
	}

	// Set the chaining mode to CBC.
	dwMode = CRYPT_MODE_CBC;
	if (!CryptSetKeyParam(hNewKey, KP_MODE, (BYTE*)&dwMode, 0)) {
		// Error setting the chaining mode.
		// TODO: Convert GetLastError() to errno?
		CryptDestroyKey(hNewKey);
		return -EIO;
	}

	// Key loaded successfully.
	hOldKey = aesw->hKey;
	aesw->hKey = hNewKey;
	if (hOldKey != 0) {
		// Destroy the old key.
		CryptDestroyKey(hOldKey);
	}
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
	} else if (aesw->hKey == 0) {
		return -EBADF;
	}

	// Set the IV.
	if (!CryptSetKeyParam(aesw->hKey, KP_IV, pIV, 0)) {
		// Error setting the IV.
		// TODO: Convert GetLastError() to errno?
		return -EIO;
	}

	// IV set.
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
	DWORD dwLen;
	BOOL bRet;

	if (!aesw || !pData || (size % 16 != 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return 0;
	} else if (aesw->hKey == 0) {
		// No hKey.
		errno = EBADF;
		return 0;
	}

	dwLen = (DWORD)size;
	bRet = CryptEncrypt(aesw->hKey, 0, FALSE, 0, pData, &dwLen, dwLen);
	return (bRet ? (int)dwLen : 0);
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
	DWORD dwLen;
	BOOL bRet;

	if (!aesw || !pData || (size % 16 != 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return 0;
	} else if (aesw->hKey == 0) {
		// No hKey.
		errno = EBADF;
		return 0;
	}

	dwLen = (DWORD)size;
	bRet = CryptDecrypt(aesw->hKey, 0, FALSE, 0, pData, &dwLen);
	return (bRet ? (int)dwLen : 0);
}
