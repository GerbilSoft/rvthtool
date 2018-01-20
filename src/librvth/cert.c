/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
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
	const RVL_Sig_RSA2048 *sig;
	const RVL_Cert_RSA2048 *cert;
	uint8_t buf[RVL_CERT_SIGLENGTH_RSA2048];
	RVL_Cert_Issuer issuer;
	unsigned int i;
	int ret;	// our return value
	int tmp_ret;	// function return values

	// SHA-1 hash.
	uint8_t sha1_hash[SHA1_HASH_LENGTH];

	// Fixed data offset.
	// TODO: Adjust for certificate and hash types?
	static const unsigned int sig_fixed_data_offset =
		(unsigned int)(sizeof(buf) - sizeof(sig_fixed_data_retail)) -
		SHA1_HASH_LENGTH;

	// SHA-1 offset in the signature.
	static const unsigned int sig_sha1_offset =
		(unsigned int)sizeof(buf) - SHA1_HASH_LENGTH;

	// Start offset within `data` for SHA-1 calculation.
	// Includes sig->issuer[], which is always 64-byte aligned.
	static const unsigned int data_sha1_offset =
		(unsigned int)(sizeof(*sig) - sizeof(sig->issuer));

	if (!data || size <= 4) {
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	// Assuming RSA-2048 signatures only for now.
	sig = (const RVL_Sig_RSA2048*)data;
	if (be32_to_cpu(sig->type) != RVL_CERT_SIGTYPE_RSA2048) {
		// Unsupported signature type.
		errno = ENOTSUP;
		return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Get the issuer's certificate.
	issuer = cert_get_issuer_from_name(sig->issuer);
	cert = (const RVL_Cert_RSA2048*)cert_get(issuer);
	if (!cert) {
		// Unknown issuer.
		errno = EINVAL;
		return SIG_ERROR_UNKNOWN_ISSUER;
	}

	// Certificate must be RSA-2048.
	if (cert->sig.type != RVL_CERT_SIGTYPE_RSA2048 ||
	    cert->pub.type != RVL_CERT_KEYTYPE_RSA2048)
	{
		// Unsupported signature type.
		errno = ENOTSUP;
		return SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE;
	}

	// Decrypt the signature.
	tmp_ret = rsaw_decrypt_signature(buf, cert->pub.modulus, cert->pub.exponent, sig->sig, 256);
	if (tmp_ret != 0) {
		// Unable to decrypt the signature.
		if (tmp_ret == ENOSPC) {
			// Wrong signature type.
			tmp_ret = SIG_ERROR_WRONG_TYPE_DECLARATION;
		}
		return tmp_ret;
	}

	// Verify the signature.
	ret = 0;
	if (!memcmp(buf, sig_magic_retail, sizeof(sig_magic_retail))) {
		// Found retail magic.
		// TODO: If root is the issuer, is it retail or debug magic?
		bool has_FF = true;	// 0xFF padding

		if (issuer < RVL_CERT_ISSUER_RETAIL_CA || issuer > RVL_CERT_ISSUER_RETAIL_TMD) {
			// Wrong magic number for this issuer.
			return SIG_ERROR_WRONG_MAGIC_NUMBER;
		}

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
