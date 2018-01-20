/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * cert.h: Certificate management.                                         *
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

// RSA and SHA-1 wrapper functions
#include "rsaw.h"
#include "hashw.h"

/**
 * The signature consists of:
 * - Magic number: 0x00,0x01,0xFF
 * - 36 bytes at the end of the signature:
 *   - 16 bytes: Fixed
 *   - 20 bytes: SHA-1 of the rest of the contents.
 * All other bytes are 0xFF.
 */

// Magic number at the start of the signature.
static const uint8_t sig_magic_retail[3] = {0x00,0x01,0xFF};
static const uint8_t sig_magic_debug[2]  = {0x00,0x02};

// Fixed data at the end of the signature. (Retail only!)
static const uint8_t sig_fixed_data_retail[16] = {
	0x00,0x30,0x21,0x30,0x09,0x06,0x05,0x2B,
	0x0E,0x03,0x02,0x1A,0x05,0x00,0x04,0x14
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
	uint8_t sha1_hash[SHA1_HASH_LENGTH];

	/** Data offsets. **/
	// Fixed data offset.
	unsigned int sig_fixed_data_offset;
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

	// Fixed data offset.
	sig_fixed_data_offset = sig_len - sizeof(sig_fixed_data_retail) - SHA1_HASH_LENGTH;

	// SHA-1 offset in the signature.
	sig_sha1_offset = sig_len - SHA1_HASH_LENGTH;

	// Start offset within `data` for SHA-1 calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	data_sha1_offset = 4 + sig_len + 0x3C;

	// Verify the signature.
	ret = 0;
	if (!memcmp(buf, sig_magic_retail, sizeof(sig_magic_retail))) {
		// Found retail magic.
		// NOTE: This is also present in the root certificate and
		// the debug certificates
		bool has_FF = true;	// 0xFF padding

		// Check for 0xFF padding.
		for (i = (unsigned int)sizeof(sig_magic_retail); i < sig_fixed_data_offset; i++) {
			if (buf[i] != 0xFF) {
				// Incorrect padding.
				has_FF = false;
				ret = SIG_FAIL_BASE_ERROR | SIG_ERROR_INVALID;
				break;
			}
		}
		if (has_FF) {
			// Check the fixed data.
			if (memcmp(&buf[sig_fixed_data_offset],
			    sig_fixed_data_retail, sizeof(sig_fixed_data_retail)) != 0)
			{
				// Fixed data is incorrect.
				ret = SIG_FAIL_BASE_ERROR | SIG_ERROR_INVALID;
			}
		}
	} else if (!memcmp(buf, sig_magic_debug, sizeof(sig_magic_debug))) {
		// Found debug magic.
		// No FF padding; not sure what the random data is.
		if (issuer < RVL_CERT_ISSUER_DEBUG_CA || issuer > RVL_CERT_ISSUER_DEBUG_TMD) {
			// Wrong magic number for this issuer.
			return SIG_ERROR_WRONG_MAGIC_NUMBER;
		}
	} else {
		// Signature magic is incorrect.
		ret = SIG_FAIL_BASE_ERROR | SIG_ERROR_INVALID;
	}

	// Check the SHA-1 hash.
	tmp_ret = hashw_sha1(sha1_hash, &data[data_sha1_offset], size - data_sha1_offset);
	if (tmp_ret != 0) {
		// SHA-1 hash error.
		return tmp_ret;
	} else if (memcmp(sha1_hash, &buf[sig_sha1_offset], sizeof(sha1_hash)) != 0) {
		// SHA-1 does not match.
		// If strncmp() succeeds, it's fakesigned.
		if (!strncmp((const char*)sha1_hash, (const char*)&buf[sig_sha1_offset], sizeof(sha1_hash))) {
			// Fakesigned.
			ret |= SIG_FAIL_HASH_FAKE | SIG_ERROR_INVALID;
		} else {
			// Not fakesigned.
			ret |= SIG_FAIL_HASH_ERROR | SIG_ERROR_INVALID;
		}
	}

	return ret;
}
