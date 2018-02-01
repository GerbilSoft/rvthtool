/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * cert.c: Certificate management.                                         *
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

#include "config.librvth.h"

#include "cert.h"
#include "cert_store.h"

#include "common.h"
#include "byteswap.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

// RSA and SHA-1 functions
#include "rsaw.h"
#include <nettle/sha1.h>

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
	const RVL_Cert *cert;	// Certificate header.

	// Signature (pointer into `data`)
	const uint8_t *sig;
	unsigned int sig_len;
	const char *s_issuer;
	RVL_Cert_Issuer issuer;

	// Parent certificate.
	const uint8_t *pubkey_mod;
	unsigned int pubkey_len;
	uint32_t pubkey_exp;

	uint8_t buf[RVL_CERT_SIGLENGTH_RSA4096];
	unsigned int i;
	int ret;	// our return value
	int tmp_ret;	// function return values

	// SHA-1 hash.
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];

	/** Data offsets. **/
	// DER identifier offset.
	unsigned int sig_der_offset;
	// SHA-1 offset in the signature.
	unsigned int sig_sha1_offset;
	// Start offset within `data` for SHA-1 calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	unsigned int data_sha1_offset;

	if (!data || size <= 4) {
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	// Determine the signature length.
	cert = (const RVL_Cert*)data;
	sig = &data[4];
	switch (be32_to_cpu(cert->signature_type)) {
		case RVL_CERT_SIGTYPE_RSA4096:
			sig_len = 4096/8;
			break;
		case RVL_CERT_SIGTYPE_RSA2048:
			sig_len = 2048/8;
			break;
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}
	s_issuer = (const char*)(sig + sig_len + 0x3C);

	// Get the issuer's certificate.
	issuer = cert_get_issuer_from_name(s_issuer);
	if (issuer == RVL_CERT_ISSUER_UNKNOWN) {
		// Unknown issuer.
		errno = EINVAL;
		return SIG_ERROR_UNKNOWN_ISSUER;
	}
	cert = (const RVL_Cert*)cert_get(issuer);
	if (!cert) {
		// Unknown issuer.
		errno = EINVAL;
		return SIG_ERROR_UNKNOWN_ISSUER;
	}

	// Skip over the issuer certificate's signature.
	switch (be32_to_cpu(cert->signature_type)) {
		case RVL_CERT_SIGTYPE_RSA4096:
			cert = (const RVL_Cert*)((const uint8_t*)cert + sizeof(RVL_Sig_RSA4096));
			break;
		case RVL_CERT_SIGTYPE_RSA2048:
			cert = (const RVL_Cert*)((const uint8_t*)cert + sizeof(RVL_Sig_RSA2048));
			break;
		case 0:
			// Only valid if this is the root certificate.
			if (issuer != RVL_CERT_ISSUER_ROOT) {
				// Not root.
				errno = ENOTSUP;
				return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
			}
			cert = (const RVL_Cert*)((const uint8_t*)cert + sizeof(RVL_Sig_Dummy));
			break;
		default:
			// Unsupported signature type.
			errno = ENOTSUP;
			return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Check the issuer certificate's public key type.
	switch (be32_to_cpu(cert->signature_type)) {
		case RVL_CERT_KEYTYPE_RSA4096: {
			const RVL_PubKey_RSA4096 *pubkey = (const RVL_PubKey_RSA4096*)cert;
			pubkey_mod = pubkey->modulus;
			pubkey_len = sizeof(pubkey->modulus);
			pubkey_exp = be32_to_cpu(pubkey->exponent);
			break;
		}
		case RVL_CERT_KEYTYPE_RSA2048: {
			const RVL_PubKey_RSA2048 *pubkey = (const RVL_PubKey_RSA2048*)cert;
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

	// DER separator offset.
	sig_der_offset = sig_len - 1 - sizeof(pkcs1_der_sha1) - SHA1_DIGEST_SIZE;

	// SHA-1 offset in the signature.
	sig_sha1_offset = sig_len - SHA1_DIGEST_SIZE;

	// Start offset within `data` for SHA-1 calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	data_sha1_offset = 4 + sig_len + 0x3C;

	// Check for the PKCS#1 magic number.
	// Reference: https://tools.ietf.org/html/rfc2313
	// Format: 00 || BT || PS || 00 || D
	// BT can be one of the following:
	// - 00: Private key - padding (PS) is 00
	// - 01: Private key - padding (PS) is FF
	// - 02: Public key - padding is pseudorandom
	// PS is the padding string.
	// Padding is followed by a 0x00 byte, then the data.
	// Last one seems to match some RVT-H prototypes. (TODO ignore previous commit)
	// Verify the signature.
	ret = 0;
	if (buf[0] == 0x00 && buf[1] <= 0x02) {
		// Padding depends on the block type.
		if (buf[1] < 0x02) {
			// Block type 0x00 or 0x01.
			// Check the padding.
			static const uint8_t padding[2] = {0x00, 0xFF};
			const uint8_t ps = padding[buf[1]];

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
		// There's still a 0x00 byte before the SHA-1 hash, though.
		if (buf[sig_sha1_offset-1] != 0x00) {
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
		if (memcmp(&buf[sig_der_offset+1], pkcs1_der_sha1, sizeof(pkcs1_der_sha1)) != 0)
		{
			// Incorrect or missing DER identifier.
			ret |= SIG_FAIL_DER_TYPE_ERROR | SIG_ERROR_INVALID;
		}
	}

	// Check the SHA-1 hash.
	sha1_init(&sha1);
	sha1_update(&sha1, size - data_sha1_offset, &data[data_sha1_offset]);
	sha1_digest(&sha1, sizeof(digest), digest);
	if (memcmp(digest, &buf[sig_sha1_offset], sizeof(digest)) != 0) {
		// SHA-1 does not match.
		// If strncmp() succeeds, it's fakesigned.
		if (!strncmp((const char*)digest, (const char*)&buf[sig_sha1_offset], sizeof(digest))) {
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
 * Fakesign a ticket.
 *
 * NOTE: If changing the encryption type, the issuer and title key
 * must be updated *before* calling this function.
 *
 * @param ticket Ticket to fakesign.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_fakesign_ticket(RVL_Ticket *ticket)
{
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];
	uint32_t *fake;

	if (!ticket) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Zero out the signature and padding.
	ticket->signature_type = cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048);
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
		sha1_update(&sha1, sizeof(*ticket) - offsetof(RVL_Ticket, issuer),
			(const uint8_t*)&ticket->issuer);
		sha1_digest(&sha1, sizeof(digest), digest);
	} while (digest[0] != 0 && ++(*fake) != 0);

	// TODO: If the first byte of the hash is not 0, failed.
	return 0;
}

/**
 * Sign a ticket with real encryption keys.
 *
 * NOTE: If changing the encryption type, the issuer and title key
 * must be updated *before* calling this function.
 *
 * @param ticket Ticket to fakesign.
 * @param key RSA-2048 private key.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_realsign_ticket(RVL_Ticket *ticket, const RSA2048PrivateKey *key)
{
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];

	if (!ticket) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Zero out the padding.
	ticket->signature_type = cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048);
	memset(ticket->padding_sig, 0, sizeof(ticket->padding_sig));

	// Calculate the SHA-1 hash.
	sha1_init(&sha1);
	sha1_update(&sha1, sizeof(*ticket) - offsetof(RVL_Ticket, issuer),
		(const uint8_t*)&ticket->issuer);
	sha1_digest(&sha1, sizeof(digest), digest);

	// Sign the ticket.
	// TODO: Check for errors.
	rsaw_sha1_sign(ticket->signature, sizeof(ticket->signature), key, digest);
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

	// Zero out the signature and padding.
	tmdHeader->signature_type = cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048);
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
 * Sign a TMD with real encryption keys.
 *
 * NOTE: If changing the encryption type, the issuer must be
 * updated *before* calling this function.
 *
 * @param tmd TMD to fakesign.
 * @param size Size of TMD.
 * @param key RSA-2048 private key.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_realsign_tmd(uint8_t *tmd, size_t size, const RSA2048PrivateKey *key)
{
	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];
	RVL_TMD_Header *const tmdHeader = (RVL_TMD_Header*)tmd;

	if (!tmd || size < sizeof(RVL_TMD_Header)) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Zero out the padding.
	tmdHeader->signature_type = cpu_to_be32(RVL_CERT_SIGTYPE_RSA2048);
	memset(tmdHeader->padding_sig, 0, sizeof(tmdHeader->padding_sig));

	// Calculate the SHA-1 hash.
	sha1_init(&sha1);
	sha1_update(&sha1, size - offsetof(RVL_TMD_Header, issuer),
		&tmd[offsetof(RVL_TMD_Header, issuer)]);
	sha1_digest(&sha1, sizeof(digest), digest);

	// Sign the TMD.
	// TODO: Check for errors.
	rsaw_sha1_sign(tmdHeader->signature, sizeof(tmdHeader->signature), key, digest);
	return 0;
}
