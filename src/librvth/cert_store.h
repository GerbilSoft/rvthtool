/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * cert_store.h: Certificate store.                                        *
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

#ifndef __RVTHTOOL_LIBRVTH_CERT_STORE_H__
#define __RVTHTOOL_LIBRVTH_CERT_STORE_H__

#include "common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reference: http://wiibrew.org/wiki/Certificate_chain

// NOTE: Wii games typically use RSA-2048 signatures and keys.

// Certificate issuers.
typedef enum {
	RVL_CERT_ISSUER_UNKNOWN		= 0,
	RVL_CERT_ISSUER_ROOT,

	// Retail
	RVL_CERT_ISSUER_RETAIL_CA,
	RVL_CERT_ISSUER_RETAIL_TICKET,
	RVL_CERT_ISSUER_RETAIL_TMD,

	// Debug
	RVL_CERT_ISSUER_DEBUG_CA,
	RVL_CERT_ISSUER_DEBUG_TICKET,
	RVL_CERT_ISSUER_DEBUG_TMD,

	RVL_CERT_ISSUER_MAX
} RVL_Cert_Issuer;

// Signature issuers.
extern const char *const RVL_Cert_Issuers[RVL_CERT_ISSUER_MAX];

/**
 * Convert a certificate issuer name to RVT_Cert_Issuer.
 * @param s_issuer Issuer name.
 * @return RVL_Cert_Issuer, or RVL_CERT_ISSUER_UNKNOWN if invalid.
 */
RVL_Cert_Issuer cert_get_issuer_from_name(const char *s_issuer);

// Opaque type for all certificates.
// Certificate users must check the certificate type value
// and cast as necessary.
typedef struct _RVL_Cert {
	uint32_t signature_type;	// [0x000] Signature type. (See Cert_SigType_e.)
} RVL_Cert;
//ASSERT_STRUCT(RVL_Cert, 4);

/**
 * Get a standard certificate.
 * @param issuer RVL_Cert_Issuer
 * @return RVL_Cert*, or NULL if invalid.
 */
const RVL_Cert *cert_get(RVL_Cert_Issuer issuer);

// Signature types.
typedef enum {
	RVL_CERT_SIGTYPE_RSA4096	= 0x00010000,	// RSA-4096
	RVL_CERT_SIGTYPE_RSA2048	= 0x00010001,	// RSA-2048
	RVL_CERT_SIGTYPE_ECC		= 0x00010002,	// Elliptic Curve
} RVL_Cert_SigType_e;

// Signature lengths.
typedef enum {
	RVL_CERT_SIGLENGTH_RSA4096	= 512,		// RSA-4096
	RVL_CERT_SIGLENGTH_RSA2048	= 256,		// RSA-2048
	RVL_CERT_SIGLENGTH_ECC		= 64,		// Elliptic Curve
} Cert_SigLength_e;

// Key types.
typedef enum {
	RVL_CERT_KEYTYPE_RSA4096	= 0,
	RVL_CERT_KEYTYPE_RSA2048	= 1,
} RVL_Cert_KeyType_e;

// Key lengths.
typedef enum {
	// Length = Modulus + Public Exponent + (pad to 0x40)
	RVL_CERT_KEYLENGTH_RSA4096	= 0x200 + 0x4 + 0x38,
	RVL_CERT_KEYLENGTH_RSA2048	= 0x100 + 0x4 + 0x38,
} RVL_Cert_KeyLength_e;

#pragma pack(1)

/**
 * RSA-4096 public key.
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_PubKey_RSA4096 {
	uint32_t unknown;	// Unknown...
	uint8_t modulus[512];
	uint32_t pub_exponent;
} RVL_PubKey_RSA4096;
//ASSERT_STRUCT(RVL_PubKey_RSA4096, 516);

/**
 * Certificate. (RSA-4096, no signature)
 *
 * NOTE: Not actually used in the certificate chain.
 * We're using this to store the root key.
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_Cert_RSA4096_KeyOnly {
	uint32_t signature_type;	// [0x000] Signature type. (Should be 0 for root.)
	uint8_t padding1[0x3C];		// [0x004] Padding.
	char issuer[64];		// [0x040] Issuer.
	uint32_t key_type;		// [0x080] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];	// [0x084] Child certificate identity.
	RVL_PubKey_RSA4096 public_key;	// [0x0C4] Public key.
	uint8_t padding2[0x34];
} RVL_Cert_RSA4096_KeyOnly;
//ASSERT_STRUCT(RVL_Cert_RSA4096_KeyOnly, 0x300);

/**
 * Certificate. (RSA-4096)
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_Cert_RSA4096 {
	uint32_t signature_type;			// [0x000] Signature type. (See Cert_SigType_e.)
	uint8_t signature[RVL_CERT_SIGLENGTH_RSA4096];	// [0x004] Signature.
	uint8_t padding1[0x3C];				// [0x204] Padding.
	char issuer[64];				// [0x240] Issuer.
	uint32_t key_type;				// [0x280] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];			// [0x284] Child certificate identity.
	RVL_PubKey_RSA4096 public_key;			// [0x2C4] Public key.
	uint8_t padding2[0x34];
} RVL_Cert_RSA4096;
//ASSERT_STRUCT(RVL_Cert_RSA4096, 0x500);

/**
 * RSA-2048 public key.
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_PubKey_RSA2048 {
	uint32_t unknown;	// Unknown...
	uint8_t modulus[256];
	uint32_t pub_exponent;
} RVL_PubKey_RSA2048;
//ASSERT_STRUCT(RVL_PubKey_RSA2048, 260);

/**
 * Certificate. (RSA-2048)
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_Cert_RSA2048 {
	uint32_t signature_type;			// [0x000] Signature type. (See Cert_SigType_e.)
	uint8_t signature[RVL_CERT_SIGLENGTH_RSA2048];	// [0x004] Signature.
	uint8_t padding1[0x3C];				// [0x104] Padding.
	char issuer[64];				// [0x140] Issuer.
	uint32_t key_type;				// [0x180] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];			// [0x184] Child certificate identity.
	RVL_PubKey_RSA2048 public_key;			// [0x1C4] Public key.
	uint8_t padding2[0x34];
} RVL_Cert_RSA2048;
//ASSERT_STRUCT(RVL_Cert_RSA2048, 0x300);

/**
 * Certificate. (RSA-4096 signature with RSA-2048 public key)
 * NOTE: Only the CA certificate uses this format.
 *
 * All fields are big-endian when loaded from a disc image.
 * When using the built-in certificates, they're in host-endian.
 */
typedef struct _RVL_Cert_RSA4096_RSA2048 {
	uint32_t signature_type;			// [0x000] Signature type. (See Cert_SigType_e.)
	uint8_t signature[RVL_CERT_SIGLENGTH_RSA4096];	// [0x004] Signature.
	uint8_t padding1[0x3C];				// [0x204] Padding.
	char issuer[64];				// [0x240] Issuer.
	uint32_t key_type;				// [0x280] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];			// [0x284] Child certificate identity.
	RVL_PubKey_RSA2048 public_key;			// [0x2C4] Public key.
	uint8_t padding2[0x34];
} RVL_Cert_RSA4096_RSA2048;
//ASSERT_STRUCT(RVL_Cert_RSA4096_RSA2048, 0x400);

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_CERT_STORE_H__ */
