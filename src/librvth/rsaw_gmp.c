/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rsaw_gmp.c: RSA encryption wrapper functions. (libgmp version)          *
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

#include "rsaw.h"
#include <gmp.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

/**
 * Decrypt an RSA signature.
 * @param buf		[out] Output buffer. (Must be `size` bytes.)
 * @param modulus	[in] Public key modulus. (Must be `size` bytes.)
 * @param exponent	[in] Public key exponent.
 * @param sig		[in] Signature. (Must be `size` bytes.)
 * @param size		[in] Signature size. (256 for RSA-2048; 512 for RSA-4096.)
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_decrypt_signature(uint8_t *buf, const uint8_t *modulus,
	uint32_t exponent, const uint8_t *sig, size_t size)
{
	mpz_t k, s, r;	// key, signature, result

	assert(buf != NULL);
	assert(modulus != NULL);
	assert(exponent != 0);
	assert(sig != NULL);
	assert(size == 256 || size == 512);

	if (!buf || !modulus || exponent == 0 || !sig || (size != 256 && size != 512)) {
		// Invalid parameters.
		return -EINVAL;
	}

	mpz_init(k);
	mpz_init(s);
	mpz_init(r);

	mpz_import(k, 1, 1, size, 1, 0, modulus);
	mpz_import(s, 1, 1, size, 1, 0, sig);
	mpz_powm_ui(r, s, exponent, k);

	mpz_clear(k);
	mpz_clear(s);

	// Decrypted signature must not be more than (size*8) bits.
	if (mpz_sizeinbase(r, 2) > (size*8)) {
		// Decrypted signature is too big.
		mpz_clear(r);
		errno = ENOSPC;
		return -ENOSPC;
	}

	// NOTE: Invalid signatures may be smaller than the buffer.
	// Clear the buffer first to ensure that invalid signatures
	// result in an all-zero buffer.
	memset(buf, 0, size);
	mpz_export(buf, NULL, 1, size, 1, 0, r);
	mpz_clear(r);

	return 0;
}
