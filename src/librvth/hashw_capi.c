/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * hashw_capi.c: Cryptographic hash wrapper functions. (CryptoAPI version) *
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <windows.h>
#include <wincrypt.h>

// SHA-1 hash object. (Microsoft CryptoAPI version.)
struct _HashCtx_SHA1 {
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
};

/**
 * Create an SHA-1 hash object.
 * @return SHA-1 hash object, or NULL on error.
 */
HashCtx_SHA1 *hashw_sha1_new(void)
{
	// Allocate a hash context.
	HashCtx_SHA1 *sha1 = malloc(sizeof(*sha1));
	if (!sha1) {
		// Could not allocate memory.
		errno = ENOMEM;
		return NULL;
	}

	// Open the crypto provider.
	if (!CryptAcquireContext(&sha1->hProv, NULL, NULL,
	    PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
	{
		// TODO: Convert GetLastError() to errno?
		free(sha1);
		errno = EIO;
		return NULL;
	}

	// Create an SHA-1 hash.
	if (!CryptCreateHash(sha1->hProv, CALG_SHA1, 0, 0, &sha1->hHash)) {
		// TODO: Convert GetLastError() to errno?
		CryptReleaseContext(sha1->hProv, 0);
		free(sha1);
		errno = EIO;
		return NULL;
	}

	// SHA-1 hash context has been initialized.
	return sha1;
}

/**
 * Free an SHA-1 hash object.
 * @param sha1 SHA-1 hash object.
 */
void hashw_sha1_free(HashCtx_SHA1 *sha1)
{
	if (sha1) {
		if (sha1->hHash != 0) {
			CryptDestroyHash(sha1->hHash);
		}
		CryptReleaseContext(sha1->hProv, 0);
		free(sha1);
	}
}

/**
 * Update an SHA-1 hash object with new data.
 * @param sha1	[in] SHA-1 hash object.
 * @param data	[in] Data to hash.
 * @param size	[in] Size of `data`.
 * @return 0 on success; negative POSIX error code on error.
 */
int hashw_sha1_update(HashCtx_SHA1 *sha1, const uint8_t *data, size_t size)
{
	if (!sha1 || !data) {
		return -EINVAL;
	} else if (size == 0) {
		// Nothing to hash...
		return 0;
	}

	if (sha1->hHash == 0) {
		// Create a new hash.
		if (!CryptCreateHash(sha1->hProv, CALG_SHA1, 0, 0, &sha1->hHash)) {
			// TODO: Convert GetLastError() to errno.
			return -EIO;
		}
	}

	// Hash the data.
	if (!CryptHashData(sha1->hHash, data, size, 0)) {
		// TODO: Convert GetLastError() to errno.
		return -EIO;
	}

	// Data hashed successfully.
	return 0;
}

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
int hashw_sha1_finish(HashCtx_SHA1 *sha1, uint8_t *pHash)
{
	DWORD cbHash;
	BOOL bRet;

	if (!sha1 || !pHash) {
		return -EINVAL;
	}

	// Get the hash value.
	cbHash = SHA1_HASH_LENGTH;
	bRet = CryptGetHashParam(sha1->hHash, HP_HASHVAL, pHash, &cbHash, 0);
	CryptDestroyHash(sha1->hHash);
	sha1->hHash = 0;

	if (!bRet) {
		// TODO: Convert GetLastError() to errno.
		return -EIO;
	}

	// Data hashed successfully.
	return 0;
}
