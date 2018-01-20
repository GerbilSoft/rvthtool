/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
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

#include <windows.h>
#include <wincrypt.h>

/**
 * Calculate an SHA-1 hash.
 * @param hash	[out] SHA-1 hash buffer. (Must be SHA1_HASH_LENGTH bytes.)
 * @param data	[in] Data to hash.
 * @param size	[in] Size of `data`.
 * @return 0 on success; negative POSIX error code on error.
 */
int hashw_sha1(uint8_t *hash, const uint8_t *data, size_t size)
{
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/aa382380%28v=vs.85%29.aspx
	BOOL bResult = FALSE;
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	DWORD cbHash;
	BOOL bRet;

	assert(hash != NULL);
	assert(data != NULL);
	assert(size != 0);

	if (!hash || !data || size == 0) {
		// Invalid parameters.
		return -EINVAL;
	}

	// Open the crypto provider.
	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
		// TODO: Convert GetLastError() to errno.
		return -EIO;
	}

	// Create an SHA-1 hash.
	if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
		// TODO: Convert GetLastError() to errno.
		CryptReleaseContext(hProv, 0);
		return -EIO;
	}

	// Hash the data.
	if (!CryptHashData(hHash, data, size, 0)) {
		// TODO: Convert GetLastError() to errno.
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return -EIO;
	}

	// Get the hash value.
	cbHash = SHA1_HASH_LENGTH;
	bRet = CryptGetHashParam(hHash, HP_HASHVAL, hash, &cbHash, 0);
	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	if (!bRet) {
		// TODO: Convert GetLastError() to errno.
		return -EIO;
	}

	// Data hashed successfully.
	return 0;
}
