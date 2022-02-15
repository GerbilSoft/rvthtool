/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * rsaw_nettle.c: RSA encryption wrapper functions. (Nettle/GMP version)   *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rsaw.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nettle/rsa.h>
#include <nettle/yarrow.h>

// NOTE: Using mini-GMP on Windows.
#ifndef NETTLE_USE_MINI_GMP
# include <gmp.h>
#endif

#ifdef _WIN32
# include <windows.h>
# include <wincrypt.h>
#endif

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
	uint8_t *buf = NULL;
	size_t buf_remain = RANDOM_BUFFER_SIZE;
#ifdef _WIN32
	const size_t size = buf_remain;
	HCRYPTPROV hCryptProv = 0;
#else /* !_WIN32 */
	size_t size;
	size_t total_read = 0;
	FILE *f = NULL;
#endif

	int err = 0;

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

#ifdef _WIN32
	// TODO: Untested.
	if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0)) {
		// TODO: Convert GetLastError() to errno?
		err = EIO;
		goto end;
	}

	// Generate random data.
	if (!CryptGenRandom(hCryptProv, (DWORD)buf_remain, buf)) {
		// TODO: Convert GetLastError() to errno?
		err = EIO;
		goto end;
	}
#else /* _WIN32 */
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
#endif

	// Seed the random number generator.
	yarrow256_seed(yarrow, size, buf);

end:
	free(buf);
#ifdef _WIN32
	if (hCryptProv != 0) {
		CryptReleaseContext(hCryptProv, 0);
	}
#else /* !_WIN32 */
	if (f) {
		fclose(f);
	}
#endif /* !WIN32 */
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
	int doSHA256)
{
	struct rsa_private_key key;
	struct {
		mpz_t e;	// e
		mpz_t p1;	// p-1
		mpz_t q1;	// q-1
		mpz_t phi;	// (p-1)*(q-1)
		mpz_t d;	// 1 / (e mod phi)
	} bncalc;
	mpz_t signature;
	int ret = 0;

	assert(buf != NULL);
	assert(buf_size != 0);
	assert(buf_size >= 256);
	assert(priv_key_data != NULL);
	assert(pHash != NULL);

	if (!buf || buf_size == 0 || buf_size < 256 || !pHash) {
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	if (!doSHA256) {
		// SHA-1: Hash size must be 20.
		if (hash_size != SHA1_DIGEST_SIZE) {
			errno = EINVAL;
			return -EINVAL;
		}
	} else {
		// SHA-256: Hash size must be 32.
		if (hash_size != SHA256_DIGEST_SIZE) {
			errno = EINVAL;
			return -EINVAL;
		}
	}

	// Initialize the temporary bignums.
	mpz_init(bncalc.e);
	mpz_init(bncalc.p1);
	mpz_init(bncalc.q1);
	mpz_init(bncalc.phi);
	mpz_init(bncalc.d);

	// Initialize the RSA private key.
	rsa_private_key_init(&key);
	mpz_import(key.p, 1, 1, sizeof(priv_key_data->p), 1, 0, priv_key_data->p);
	mpz_import(key.q, 1, 1, sizeof(priv_key_data->q), 1, 0, priv_key_data->q);
	if (!rsa_private_key_prepare(&key)) {
		// Error importing the private key.
		errno = EIO;
		return -EIO;
	}

	// Calculate a, b, and c.
	mpz_sub_ui(bncalc.p1, key.p, 1);
	mpz_sub_ui(bncalc.q1, key.q, 1);
	mpz_mul(bncalc.phi, bncalc.p1, bncalc.q1);
	mpz_set_ui(bncalc.e, priv_key_data->e);
	mpz_invert(bncalc.d, bncalc.e, bncalc.phi);
	// a = d % (p - 1)
	mpz_fdiv_r(key.a, bncalc.d, bncalc.p1);
	// b = d % (q - 1)
	mpz_fdiv_r(key.b, bncalc.d, bncalc.q1);
	// c = q^{-1} (mod p)
	mpz_invert(key.c, key.q, key.p);

	// Create the signature.
	mpz_init(signature);
	if (!doSHA256) {
		if (!rsa_sha1_sign_digest(&key, pHash, signature)) {
			// Error signing the SHA-1 hash.
			ret = -EIO;
			goto end;
		}
	} else {
		if (!rsa_sha256_sign_digest(&key, pHash, signature)) {
			// Error signing the SHA-256 hash.
			ret = -EIO;
			goto end;
		}
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
	mpz_clear(bncalc.e);
	mpz_clear(bncalc.p1);
	mpz_clear(bncalc.q1);
	mpz_clear(bncalc.phi);
	mpz_clear(bncalc.d);
	if (ret != 0) {
		errno = -ret;
	}
	return ret;
}
