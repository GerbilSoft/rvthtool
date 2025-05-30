/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * rsaw.h: RSA encryption wrapper functions.                               *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

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
 * Create an RSA-2048 signature using an RSA private key.
 * @param buf			[out] Output buffer.
 * @param buf_size		[in] Size of `buf`.
 * @param priv_key_data		[in] RSA2048PrivateKey struct.
 * @param pHash			[in] Hash.
 * @param hash_size		[in] Hash size. (20 for SHA-1, 32 for SHA-256)
 * @param doSHA256		[in] If 1, do SHA-256. (TODO: Use an enum.)
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_rsa2048_sign(uint8_t *buf, size_t buf_size,
	const RSA2048PrivateKey *priv_key_data,
	const uint8_t *pHash, size_t hash_size,
	int doSHA256);

#ifdef __cplusplus
}
#endif
