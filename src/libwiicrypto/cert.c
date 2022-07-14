/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * cert.c: Certificate management.                                         *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "cert.h"
#include "cert_store.h"

#include "common.h"
#include "byteswap.h"
#include "stdboolx.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

// RSA and hash functions
#include "rsaw.h"
#include <nettle/sha1.h>
#include <nettle/sha2.h>

/**
 * The signature is stored in PKCS #1 format.
 * References:
 * - https://tools.ietf.org/html/rfc3447
 * - https://tools.ietf.org/html/rfc2313
 *
 * The signature consists of:
 * - PKCS#1 header: 0x00,0x01,0xFF
 *   - 0x01 is the block type. May be 0x02 or 0x00 also.
 *   - 0xFF is the first padding byte.
 *     - For block type 0, this is 0x00.
 *     - For block type 2, this is pseudorandom.
 * - 36 bytes at the end of the signature:
 *   - 1 byte: 0x00
 *   - 15 bytes: DER SHA-1 identifier.
 *   - 20 bytes: SHA-1 of the rest of the contents.
 */

// DER identifier for SHA-1 hashes.
static const uint8_t pkcs1_der_sha1[15] = {
	0x30,0x21,0x30,0x09,0x06,0x05,0x2B,0x0E,
	0x03,0x02,0x1A,0x05,0x00,0x04,0x14
};

// DER identifier for SHA-256 hashes.
static const uint8_t pkcs1_der_sha256[19] = {
	0x30,0x31,0x30,0x0D,0x06,0x09,0x60,0x86,
	0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,
	0x00,0x04,0x20
};

/**
 * Verify a ticket or TMD. (internal function)
 *
 * @param issuer_cert Certificate to verify against.
 * @param data Data to verify.
 * @param size Size of data.
 * @return Signature status. (Sig_Status if positive; if negative, POSIX error code.)
 */
static int cert_verify_int(const RVL_Cert *issuer_cert, const uint8_t *data, size_t size)
{
	// Signature (pointer into `data`)
	const RVL_Cert *verify_cert;	// Certificate to verify.
	const uint8_t *sig;
	unsigned int sig_len;

	// Parent certificate.
	const uint8_t *pubkey_mod;
	unsigned int pubkey_len;
	uint32_t pubkey_exp;

	uint8_t buf[RVL_CERT_SIGLENGTH_RSA4096];
	int ret;	// our return value
	int tmp_ret;	// function return values

	// Hash data.
	uint8_t digest[SHA256_DIGEST_SIZE];
	unsigned int hash_digest_size;
	bool isSha2;

	/** Data offsets. **/
	// DER identifier offset.
	unsigned int sig_der_offset;
	// Hash offset in the signature.
	unsigned int sig_hash_offset;
	// Start offset within `data` for hash calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	unsigned int data_hash_offset;
	// DER to check.
	const uint8_t *der_data;
	size_t der_size;

	// Determine the signature length.
	verify_cert = (const RVL_Cert*)data;
	sig = &data[4];
	switch (be32_to_cpu(verify_cert->signature_type)) {
		case RVL_CERT_SIGTYPE_RSA4096_SHA1:
			sig_len = 4096/8;
			isSha2 = false;
			break;
		case WUP_CERT_SIGTYPE_RSA4096_SHA256:
			sig_len = 4096/8;
			isSha2 = true;
			break;
		case RVL_CERT_SIGTYPE_RSA2048_SHA1:
			sig_len = 2048/8;
			isSha2 = false;
			break;
		case WUP_CERT_SIGTYPE_RSA2048_SHA256:
			sig_len = 2048/8;
			isSha2 = true;
			break;
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Skip over the issuer certificate's signature.
	switch (be32_to_cpu(issuer_cert->signature_type)) {
		case RVL_CERT_SIGTYPE_RSA4096_SHA1:
		case WUP_CERT_SIGTYPE_RSA4096_SHA256:
			issuer_cert = (const RVL_Cert*)((const uint8_t*)issuer_cert + sizeof(RVL_Sig_RSA4096));
			break;
		case RVL_CERT_SIGTYPE_RSA2048_SHA1:
		case WUP_CERT_SIGTYPE_RSA2048_SHA256:
			issuer_cert = (const RVL_Cert*)((const uint8_t*)issuer_cert + sizeof(RVL_Sig_RSA2048));
			break;
		case 0: {
			// Only valid if this is a root certificate.
			const char *const s_issuer = (const char*)(sig + sig_len + 0x3C);
			if (strncmp(s_issuer, "Root", 5) != 0) {
				// Not root.
				errno = ENOTSUP;
				return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
			}
			issuer_cert = (const RVL_Cert*)((const uint8_t*)issuer_cert + sizeof(RVL_Sig_Dummy));
			break;
		}
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Check the issuer certificate's public key type.
	switch (be32_to_cpu(issuer_cert->signature_type)) {
		case RVL_CERT_KEYTYPE_RSA4096: {
			const RVL_PubKey_RSA4096 *pubkey = (const RVL_PubKey_RSA4096*)issuer_cert;
			pubkey_mod = pubkey->modulus;
			pubkey_len = sizeof(pubkey->modulus);
			pubkey_exp = be32_to_cpu(pubkey->exponent);
			break;
		}
		case RVL_CERT_KEYTYPE_RSA2048: {
			const RVL_PubKey_RSA2048 *pubkey = (const RVL_PubKey_RSA2048*)issuer_cert;
			pubkey_mod = pubkey->modulus;
			pubkey_len = sizeof(pubkey->modulus);
			pubkey_exp = be32_to_cpu(pubkey->exponent);
			break;
		}
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	if (sig_len != pubkey_len) {
		// Key lengths differ.
		// TODO: Better error code?
		errno = ENOTSUP;
		return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Decrypt the signature.
	tmp_ret = rsaw_decrypt_signature(buf, pubkey_mod, pubkey_exp, sig, sig_len);
	if (tmp_ret != 0) {
		// Unable to decrypt the signature.
		if (tmp_ret == ENOSPC) {
			// Wrong signature type.
			tmp_ret = SIG_ERROR_WRONG_TYPE_DECLARATION;
		}
		return tmp_ret;
	}

	// Get signature data.
	if (likely(!isSha2)) {
		// SHA-1 signature.
		der_data = pkcs1_der_sha1;
		der_size = sizeof(pkcs1_der_sha1);
		hash_digest_size = SHA1_DIGEST_SIZE;
	} else {
		// SHA-256 signature.
		der_data = pkcs1_der_sha256;
		der_size = sizeof(pkcs1_der_sha256);
		hash_digest_size = SHA256_DIGEST_SIZE;
	}

	// DER separator offset.
	sig_der_offset = sig_len - 1 - der_size - hash_digest_size;

	// Hash offset in the signature.
	sig_hash_offset = sig_len - hash_digest_size;

	// Start offset within `data` for SHA-1 calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	data_hash_offset = 4 + sig_len + 0x3C;

	// Check for the PKCS#1 header and padding.
	// Reference: https://tools.ietf.org/html/rfc2313
	// Format: 00 || BT || PS || 00 || D
	// BT can be one of the following:
	// - 00: Private key - padding (PS) is 00
	// - 01: Private key - padding (PS) is FF
	// - 02: Public key - padding is pseudorandom
	// PS is the padding string.
	// Padding is followed by a 0x00 byte, then the data.
	// - Retail and official RVT-R images use 01.
	// - RVT-H prototypes I've encountered seem to use 02.
	// Verify the signature.
	ret = 0;
	if (buf[0] == 0x00 && buf[1] <= 0x02) {
		// Padding depends on the block type.
		if (buf[1] < 0x02) {
			// Block type 0x00 or 0x01.
			// Check the padding.
			static const uint8_t padding[2] = {0x00, 0xFF};
			const uint8_t ps = padding[buf[1]];
			unsigned int i;

			for (i = 2; i < sig_der_offset-1; i++) {
				if (buf[i] != ps) {
					// Incorrect padding.
					ret = SIG_FAIL_PADDING_ERROR | SIG_ERROR_INVALID;
					break;
				}
			}
		} else {
			// Block type 0x02.
			// This has pseudorandom data, so we can't check it.
		}
	} else {
		// Invalid block type.
		ret = SIG_FAIL_HEADER_ERROR | SIG_ERROR_INVALID;
	}

	// NOTE: If block type is 0x02, the DER identifier is missing.
	// This might be specific to the debug issuer, though.
	// Reference: https://github.com/iversonjimmy/acer_cloud_wifi_copy/blob/master/sw_x/gvm_core/internal/csl/src/cslrsa.c#L165
	if (buf[1] == 0x02) {
		// There's still a 0x00 byte before the hash, though.
		if (buf[sig_hash_offset-1] != 0x00) {
			// Missing 0x00 byte.
			ret |= SIG_FAIL_NO_SEPARATOR_ERROR | SIG_ERROR_INVALID;
		}
	} else {
		// Check for the 0x00 byte before the DER identifier.
		if (buf[sig_der_offset] != 0x00) {
			// Missing 0x00 byte.
			ret |= SIG_FAIL_NO_SEPARATOR_ERROR | SIG_ERROR_INVALID;
		}

		// Check the DER identifier.
		if (memcmp(&buf[sig_der_offset+1], der_data, der_size) != 0) {
			// Incorrect or missing DER identifier.
			ret |= SIG_FAIL_DER_TYPE_ERROR | SIG_ERROR_INVALID;
		}
	}

	// Check the hash.
	if (likely(!isSha2)) {
		struct sha1_ctx sha1;
		sha1_init(&sha1);
		sha1_update(&sha1, size - data_hash_offset, &data[data_hash_offset]);
		sha1_digest(&sha1, SHA1_DIGEST_SIZE, digest);
	} else {
		struct sha256_ctx sha256;
		sha256_init(&sha256);
		sha256_update(&sha256, size - data_hash_offset, &data[data_hash_offset]);
		sha256_digest(&sha256, SHA256_DIGEST_SIZE, digest);
	}

	if (memcmp(digest, &buf[sig_hash_offset], hash_digest_size) != 0) {
		// Hash does not match.
		// [SHA-1 only] If strncmp() succeeds, it's fakesigned.
		if (likely(!isSha2) && !strncmp((const char*)digest, (const char*)&buf[sig_hash_offset], sizeof(digest))) {
			// Fakesigned.
			ret |= SIG_FAIL_HASH_FAKE | SIG_ERROR_INVALID;
		} else {
			// Not fakesigned.
			ret |= SIG_FAIL_HASH_ERROR | SIG_ERROR_INVALID;
		}
	}

	return ret;
}

/**
 * Verify a ticket or TMD.
 *
 * Both structs have the same initial layout:
 * - Signature type
 * - Signature
 * - Issuer
 *
 * Both Signature and Issuer are padded for 64-byte alignment.
 * The Signature covers Issuer and everything up to `size`.
 *
 * @param data Data to verify.
 * @param size Size of data.
 * @return Signature status. (Sig_Status if positive; if negative, POSIX error code.)
 */
int cert_verify(const uint8_t *data, size_t size)
{
	// Signature (pointer into `data`)
	const RVL_Cert *verify_cert;	// Certificate to verify.
	const uint8_t *sig;
	unsigned int sig_len;
	const char *s_issuer;

	int ret;	// our return value

	if (!data || size <= 4) {
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	// Determine the signature length.
	verify_cert = (const RVL_Cert*)data;
	sig = &data[4];
	switch (be32_to_cpu(verify_cert->signature_type)) {
		case RVL_CERT_SIGTYPE_RSA4096_SHA1:
		case WUP_CERT_SIGTYPE_RSA4096_SHA256:
			sig_len = 4096/8;
			break;
		case RVL_CERT_SIGTYPE_RSA2048_SHA1:
		case WUP_CERT_SIGTYPE_RSA2048_SHA256:
			sig_len = 2048/8;
			break;
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}
	// Get the signature issuer.
	s_issuer = (const char*)(sig + sig_len + 0x3C);

	// Get the issuer's certificate.
	// NOTE: If it's Root, we won't be able to get the issuer directly.
	// We'll need to test both dpki and ppki.
	// TODO: Add a PKI specification to prevent errors where a certificate
	// is signed by the wrong Root certificate? This isn't likely, since
	// the Root keys for both PKIs aren't public.
	if (!strncmp(s_issuer, "Root", 5)) {
		// Try dpki first.
		const RVL_Cert *issuer_cert = cert_get(RVL_CERT_ISSUER_DPKI_ROOT);
		assert(issuer_cert != NULL);
		if (!issuer_cert) {
			return -EIO;
		}
		ret = cert_verify_int(issuer_cert, data, size);
		if (ret < 0) {
			return ret;
		}
		if (ret != SIG_STATUS_OK) {
			// Signature is not valid. Try ppki.
			issuer_cert = cert_get(RVL_CERT_ISSUER_PPKI_ROOT);
			assert(issuer_cert != NULL);
			if (!issuer_cert) {
				return -EIO;
			}
			ret = cert_verify_int(issuer_cert, data, size);
		}
	} else {
		const RVL_Cert *issuer_cert;

		RVL_Cert_Issuer issuer = cert_get_issuer_from_name(s_issuer);
		if (issuer == RVL_CERT_ISSUER_UNKNOWN) {
			// Unknown issuer.
			errno = EINVAL;
			return SIG_ERROR_UNKNOWN_ISSUER;
		}
		issuer_cert = cert_get(issuer);
		assert(issuer_cert != NULL);
		if (!issuer_cert) {
			return -EIO;
		}
		ret = cert_verify_int(issuer_cert, data, size);
	}

	return ret;
}

/**
 * Fakesign a ticket.
 *
 * NOTE: If changing the encryption type, the issuer and title key
 * must be updated *before* calling this function.
 *
 * @param ticket_u8 Ticket to fakesign.
 * @param size Size of ticket.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_fakesign_ticket(uint8_t *ticket_u8, size_t size)
{
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];
	uint32_t *fake;
	RVL_Ticket *const ticket = (RVL_Ticket*)ticket_u8;

	if (!ticket) {
		errno = EINVAL;
		return -EINVAL;
	}

	if (ticket->signature_type != cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048_SHA1)) {
		// Only RSA-2048 with SHA-1 is supported for fakesigning.
		errno = EINVAL;
		return -EINVAL;
	}

	// Zero out the signature and padding.
	memset(ticket->signature, 0, sizeof(ticket->signature));
	memset(ticket->padding_sig, 0, sizeof(ticket->padding_sig));

	// Using 0x25C for brute-forcing the SHA-1 hash.
	// This area is part of the content access permissions.
	// Disc partitions only have one content, so the rest is unused.
	// (Wiimm's ISO Tools uses 0x24C.)
	// NOTE: Brute-forcing is done using HOST-endian.
	fake = (uint32_t*)&ticket->content_access_perm[0x3A];
	*fake = 0;
	sha1_init(&sha1);
	do {
		// Calculate the SHA-1 of the ticket.
		// If the first byte is 0, we're done.
		static const unsigned int signing_offset = offsetof(RVL_Ticket, issuer);
		sha1_update(&sha1, size - signing_offset, (const uint8_t*)(&ticket_u8[signing_offset]));
		sha1_digest(&sha1, sizeof(digest), digest);
	} while (digest[0] != 0 && ++(*fake) != 0);

	// TODO: If the first byte of the hash is not 0, failed.
	return 0;
}

/**
 * Fakesign a TMD.
 *
 * NOTE: If changing the encryption type, the issuer must be
 * updated *before* calling this function.
 *
 * @param tmd TMD to fakesign.
 * @param size Size of TMD.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_fakesign_tmd(uint8_t *tmd, size_t size)
{
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];
	RVL_TMD_Header *const tmdHeader = (RVL_TMD_Header*)tmd;
	uint32_t *fake;

	if (!tmd || size < sizeof(RVL_TMD_Header)) {
		errno = EINVAL;
		return -EINVAL;
	}

	if (tmdHeader->signature_type != cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048_SHA1)) {
		// Only RSA-2048 with SHA-1 is supported for fakesigning.
		errno = EINVAL;
		return -EINVAL;
	}

	// Zero out the signature and padding.
	memset(tmdHeader->signature, 0, sizeof(tmdHeader->signature));
	memset(tmdHeader->padding_sig, 0, sizeof(tmdHeader->padding_sig));

	// Using 0x19C for brute-forcing the SHA-1 hash.
	// This area is "reserved" and is otherwise unused.
	// (Wiimm's ISO Tools uses 0x19A.)
	// NOTE: Brute-forcing is done using HOST-endian.
	fake = (uint32_t*)&tmdHeader->reserved[2];
	*fake = 0;
	sha1_init(&sha1);
	do {
		// Calculate the SHA-1 of the TMD.
		// If the first byte is 0, we're done.
		sha1_update(&sha1, size - offsetof(RVL_TMD_Header, issuer),
			&tmd[offsetof(RVL_TMD_Header, issuer)]);
		sha1_digest(&sha1, sizeof(digest), digest);
	} while (digest[0] != 0 && ++(*fake) != 0);

	// TODO: If the first byte of the hash is not 0, failed.
	return 0;
}

/**
 * Sign a ticket or TMD with real encryption keys.
 *
 * NOTE: If changing the encryption type, the issuer must be
 * updated *before* calling this function.
 *
 * NOTE 2: For Wii, the full TMD must be signed.
 * For Wii U, only the TMD header is signed.
 *
 * @param data		[in/out] Ticket or TMD to fakesign.
 * @param size		[in] Size of ticket or TMD.
 * @param key		[in] RSA-2048 private key.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_realsign_ticketOrTMD(uint8_t *data, size_t size, const RSA2048PrivateKey *key)
{
	uint8_t hash[SHA256_DIGEST_SIZE];
	size_t hash_size;
	RVL_Sig_RSA2048 *const sig = (RVL_Sig_RSA2048*)data;
	bool doSHA256;

	// Signature is in the first 0x140 bytes.
	if (!data || size < 0x140) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Determine the algorithm to use.
	switch (be32_to_cpu(sig->type)) {
		case RVL_CERT_SIGTYPE_RSA2048_SHA1:
			hash_size = SHA1_DIGEST_SIZE;
			doSHA256 = false;
			break;
		case WUP_CERT_SIGTYPE_RSA2048_SHA256:
			hash_size = SHA256_DIGEST_SIZE;
			doSHA256 = true;
			break;
		default:
			// Not supported.
			errno = EINVAL;
			return -EINVAL;
	}

	// Zero out the padding.
	memset(sig->padding, 0, sizeof(sig->padding));

	if (!doSHA256) {
		// Calculate the SHA-1 hash.
		struct sha1_ctx sha1;
		hash_size = SHA1_DIGEST_SIZE;
		sha1_init(&sha1);
		sha1_update(&sha1, size - offsetof(RVL_Sig_RSA2048, issuer),
			&data[offsetof(RVL_Sig_RSA2048, issuer)]);
		sha1_digest(&sha1, SHA1_DIGEST_SIZE, hash);
	} else {
		// Calculate the SHA-256 hash.
		struct sha256_ctx sha256;
		hash_size = SHA256_DIGEST_SIZE;
		sha256_init(&sha256);
		sha256_update(&sha256, size - offsetof(RVL_Sig_RSA2048, issuer),
			&data[offsetof(RVL_Sig_RSA2048, issuer)]);
		sha256_digest(&sha256, SHA256_DIGEST_SIZE, hash);
	}

	// Sign the TMD.
	// TODO: Check for errors.
	rsaw_rsa2048_sign(sig->sig, sizeof(sig->sig), key, hash, hash_size, doSHA256);
	return 0;
}
