/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rsaw_nettle.c: RSA encryption wrapper functions. (Nettle/GMP version)   *
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

#include <nettle/rsa.h>
#include <nettle/yarrow.h>

// NOTE: Using mini-GMP on Windows.
#ifndef NETTLE_USE_MINI_GMP
# include <gmp.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Size of the buffer for random number generation.
#define RANDOM_BUFFER_SIZE 1024

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
	// F(x) = x^e mod n
	mpz_t n, x, f;	// modulus, signature, result

	assert(buf != NULL);
	assert(modulus != NULL);
	assert(exponent != 0);
	assert(sig != NULL);
	assert(size == 256 || size == 512);

	if (!buf || !modulus || exponent == 0 || !sig || (size != 256 && size != 512)) {
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	mpz_init(n);
	mpz_init(x);
	mpz_init(f);

	mpz_import(n, 1, 1, size, 1, 0, modulus);
	mpz_import(x, 1, 1, size, 1, 0, sig);
	mpz_powm_ui(f, x, exponent, n);

	mpz_clear(n);
	mpz_clear(x);

	// Decrypted signature must not be more than (size*8) bits.
	if (mpz_sizeinbase(f, 2) > (size*8)) {
		// Decrypted signature is too big.
		mpz_clear(f);
		errno = ENOSPC;
		return -ENOSPC;
	}

	// NOTE: Invalid signatures may be smaller than the buffer.
	// Clear the buffer first to ensure that invalid signatures
	// result in an all-zero buffer.
	memset(buf, 0, size);
	mpz_export(buf, NULL, 1, size, 1, 0, f);
	mpz_clear(f);

	return 0;
}

/**
 * Initialize a yarrow random number context.
 * This seeds the context with data from /dev/urandom.
 * @return 0 on success; negative POSIX error code on error.
 */
static int init_random(struct yarrow256_ctx *yarrow)
{
	size_t size;
	size_t total_read = 0;
	size_t buf_remain = RANDOM_BUFFER_SIZE;
	uint8_t *buf = NULL;
	FILE *f = NULL;

	int err = 0;

	// TODO: Windows equivalent.
	// Based on simple_random() from nettle-3.4/examples/io.c.
	yarrow256_init(yarrow, 0, NULL);

	errno = 0;
	buf = malloc(buf_remain);
	if (!buf) {
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto end;
	}

	errno = 0;
	f = fopen("/dev/urandom", "rb");
	if (!f) {
		err = errno;
		if (err == 0) {
			err = ENOENT;
		}
		goto end;
	}

	// Read up to the length of the buffer.
	do {
		errno = 0;
		size = fread(&buf[total_read], 1, buf_remain, f);
		if (size == 0) {
			// Read error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			break;
		}
		total_read += size;
		buf_remain -= size;
	} while (total_read < RANDOM_BUFFER_SIZE);

	if (size < 128) {
		// Not enough random data read...
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto end;
	}

	// Seed the random number generator.
	yarrow256_seed(yarrow, size, buf);

end:
	free(buf);
	if (f) {
		fclose(f);
	}
	if (err != 0) {
		errno = err;
	}
	return -err;
}

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
	const uint8_t *cleartext, size_t cleartext_size)
{
	struct rsa_public_key key;
	struct yarrow256_ctx yarrow;

	mpz_t ciphertext;
	int ret = 0;

	assert(buf != NULL);
	assert(buf_size != 0);
	assert(buf_size >= modulus_size);
	assert(modulus != NULL);
	assert(modulus_size == 256 || modulus_size == 512);
	assert(exponent != 0);
	assert(cleartext != NULL);
	assert(cleartext_size != 0);

	if (!buf || buf_size == 0 || buf_size < modulus_size ||
	    !modulus || (modulus_size != 256 && modulus_size != 512) ||
	    exponent == 0 || !cleartext || cleartext_size == 0)
	{
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	// Initialize the RSA public key and ciphertext.
	rsa_public_key_init(&key);
	mpz_init(ciphertext);

	// Initialize the random number generator.
	ret = init_random(&yarrow);
	if (ret != 0) {
		// Error initializing the random number generator.
		goto end;
	}

	// Import the public key.
	mpz_import(key.n, 1, 1, modulus_size, 1, 0, modulus);
	mpz_set_ui(key.e, exponent);
	if (!rsa_public_key_prepare(&key)) {
		// Error importing the public key.
		ret = -EIO;
		goto end;
	}

	// Encrypt the data.
	if (!rsa_encrypt(&key, &yarrow, (nettle_random_func*)yarrow256_random,
	    cleartext_size, cleartext, ciphertext))
	{
		// Error encrypting the data.
		ret = -EIO;
		goto end;
	}

	// Encrypted data must not be more than (buf_size*8) bits.
	if (mpz_sizeinbase(ciphertext, 2) > (buf_size*8)) {
		// Encrypted data is too big.
		ret = -ENOSPC;
		goto end;
	}

	// NOTE: Invalid signatures may be smaller than the buffer.
	// Clear the buffer first to ensure that invalid signatures
	// result in an all-zero buffer.
	memset(buf, 0, buf_size);
	mpz_export(buf, NULL, 1, buf_size, 1, 0, ciphertext);

end:
	rsa_public_key_clear(&key);
	mpz_clear(ciphertext);
	if (ret != 0) {
		errno = -ret;
	}
	return ret;
}

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
	const uint8_t *sha1)
{
	struct rsa_private_key key;
	mpz_t signature;
	int ret = 0;

	assert(buf != NULL);
	assert(buf_size != 0);
	assert(buf_size >= 256);
	assert(priv_key_data != NULL);
	assert(sha1 != NULL);

	if (!buf || buf_size == 0 || buf_size < 256 || !sha1) {
		// Invalid parameters.
		errno = EIO;
		return -EINVAL;
	}

	// Initialize the RSA private key.
	rsa_private_key_init(&key);
	mpz_import(key.p, 1, 1, sizeof(priv_key_data->p), 1, 0, priv_key_data->p);
	mpz_import(key.q, 1, 1, sizeof(priv_key_data->q), 1, 0, priv_key_data->q);
	mpz_import(key.a, 1, 1, sizeof(priv_key_data->a), 1, 0, priv_key_data->a);
	mpz_import(key.b, 1, 1, sizeof(priv_key_data->b), 1, 0, priv_key_data->b);
	mpz_import(key.c, 1, 1, sizeof(priv_key_data->c), 1, 0, priv_key_data->c);
	if (!rsa_private_key_prepare(&key)) {
		// Error importing the private key.
		errno = EIO;
		return -EIO;
	}

	// Create the signature.
	mpz_init(signature);
	if (!rsa_sha1_sign_digest(&key, sha1, signature)) {
		// Error signing the SHA-1 hash.
		ret = -EIO;
		goto end;
	}

	// Encrypted data must not be more than (buf_size*8) bits.
	if (mpz_sizeinbase(signature, 2) > (buf_size*8)) {
		// Encrypted data is too big.
		ret = -ENOSPC;
		goto end;
	}

	// NOTE: Invalid signatures may be smaller than the buffer.
	// Clear the buffer first to ensure that invalid signatures
	// result in an all-zero buffer.
	memset(buf, 0, buf_size);
	mpz_export(buf, NULL, 1, buf_size, 1, 0, signature);

end:
	rsa_private_key_clear(&key);
	mpz_clear(signature);
	if (ret != 0) {
		errno = -ret;
	}
	return ret;
}
