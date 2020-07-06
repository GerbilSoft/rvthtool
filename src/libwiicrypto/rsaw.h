/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * rsaw.h: RSA encryption wrapper functions.                               *
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

#ifndef __RVTHTOOL_LIBWIICRYPTO_RSAW_H__
#define __RVTHTOOL_LIBWIICRYPTO_RSAW_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
	uint32_t exponent, const uint8_t *sig, size_t size);

/**
 * Encrypt data using an RSA public key.
 * @param buf			[out] Output buffer.
 * @param buf_size		[in] Size of `buf`.
 * @param modulus		[in] Public key modulus.
 * @param modulus_size		[in] Size of `modulus`, in bytes.
 * @param exponent		[in] Public key exponent.
 * @param cleartext		[in] Cleartext.
 * @param cleartext_size	[in] Size of `cleartext`, in bytes.
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_encrypt(uint8_t *buf, size_t buf_size,
	const uint8_t *modulus, size_t modulus_size,
	uint32_t exponent,
	const uint8_t *cleartext, size_t cleartext_size);

/** Private key stuff. **/

// These private keys consist of p, q, and e.
// Nettle also requires a, b, and c, but those can be
// calculated at runtime.

typedef struct _RSA2048PrivateKey {
	uint8_t p[128];
	uint8_t q[128];

	// Exponent. (Same as the public key.)
	uint32_t e;
} RSA2048PrivateKey;

/**
 * Create an RSA signature using an RSA private key.
 * NOTE: This function only supports RSA-2048 keys.
 * @param buf			[out] Output buffer.
 * @param buf_size		[in] Size of `buf`.
 * @param priv_key_data		[in] RSA2048PrivateKey struct.
 * @param sha1			[in] SHA-1 hash. (Must be 20 bytes.)
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_sha1_sign(uint8_t *buf, size_t buf_size,
	const RSA2048PrivateKey *priv_key_data,
	const uint8_t *sha1);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBWIICRYPTO_RSAW_H__ */
