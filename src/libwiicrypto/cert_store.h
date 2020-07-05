/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
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

#ifndef __RVTHTOOL_LIBWIICRYPTO_CERT_STORE_H__
#define __RVTHTOOL_LIBWIICRYPTO_CERT_STORE_H__

#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reference: http://wiibrew.org/wiki/Certificate_chain

// NOTE: Wii games typically use RSA-2048 signatures and keys.

// Encryption keys. (AES-128)
typedef enum {
	RVL_KEY_DEBUG	= 0,	// Debug common key
	RVL_KEY_RETAIL	= 1,	// Retail common key
	RVL_KEY_KOREAN	= 2,	// Korean common key
	RVL_KEY_vWii	= 3,	// vWii common key [FIXME: Debug version?]

	RVL_KEY_MAX
} RVL_AES_Keys_e;
extern const uint8_t RVL_AES_Keys[RVL_KEY_MAX][16];

// Public key infrastructure.
typedef enum {
	RVL_PKI_UNKNOWN	= 0,	// unknown
	RVL_PKI_DPKI,	// devel pki (debug)
	RVL_PKI_PPKI,	// prod pki (retail)

	// NOTE: 3DS and Wii U use the same PKIs.
	// Keeping them separate here for various reasons.

	CTR_PKI_DPKI,	// 3DS/Wii U devel pki (debug)
	CTR_PKI_PPKI,	// 3DS/Wii U prod pki (retail)

	WUP_PKI_DPKI,	// 3DS/Wii U devel pki (debug)
	WUP_PKI_PPKI,	// 3DS/Wii U prod pki (retail)
} RVL_PKI;

// Certificate issuers.
typedef enum {
	RVL_CERT_ISSUER_UNKNOWN		= 0,

	// Root certificates
	RVL_CERT_ISSUER_DPKI_ROOT,	// Root (dpki)
	RVL_CERT_ISSUER_PPKI_ROOT,	// Root (ppki)

	// Wii dpki (devel; debug)
	RVL_CERT_ISSUER_DPKI_CA,	// CA
	RVL_CERT_ISSUER_DPKI_TICKET,	// XS
	RVL_CERT_ISSUER_DPKI_TMD,	// CP: Content Provider
	RVL_CERT_ISSUER_DPKI_MS,	// MS: Mastering Server
	// These are unused, but are present on RVT-H Readers.
	RVL_CERT_ISSUER_DPKI_XS04,
	RVL_CERT_ISSUER_DPKI_CP05,

	// Wii ppki (prod; retail)
	RVL_CERT_ISSUER_PPKI_CA,	// CA
	RVL_CERT_ISSUER_PPKI_TICKET,	// XS
	RVL_CERT_ISSUER_PPKI_TMD,	// CP: Content Provider

	// 3DS dpki (devel; debug)
	CTR_CERT_ISSUER_DPKI_CA,	// CA
	CTR_CERT_ISSUER_DPKI_TICKET,	// XS
	CTR_CERT_ISSUER_DPKI_TMD,	// CP: Content Provider

	// 3DS ppki (prod; retail)
	CTR_CERT_ISSUER_PPKI_CA,	// CA
	CTR_CERT_ISSUER_PPKI_TICKET,	// XS
	CTR_CERT_ISSUER_PPKI_TMD,	// CP: Content Provider

	// Wii U dpki (devel; debug)
	WUP_CERT_ISSUER_DPKI_CA,	// CA (same as 3DS)
	WUP_CERT_ISSUER_DPKI_TICKET,	// XS
	WUP_CERT_ISSUER_DPKI_TMD,	// CP: Content Provider
	WUP_CERT_ISSUER_DPKI_SP,	// SP (FIXME: What is SP?)

	// Wii U ppki (prod; retail)
	WUP_CERT_ISSUER_PPKI_CA,	// CA (same as 3DS)
	WUP_CERT_ISSUER_PPKI_TICKET,	// XS (same as 3DS)
	WUP_CERT_ISSUER_PPKI_TMD,	// CP: Content Provider (same as 3DS)

	RVL_CERT_ISSUER_MAX,

	// Min/max values.
	RVL_CERT_ISSUER_DPKI_MIN	= RVL_CERT_ISSUER_DPKI_ROOT,
	RVL_CERT_ISSUER_DPKI_MAX	= RVL_CERT_ISSUER_DPKI_CP05,
	RVL_CERT_ISSUER_PPKI_MIN	= RVL_CERT_ISSUER_PPKI_ROOT,
	RVL_CERT_ISSUER_PPKI_MAX	= RVL_CERT_ISSUER_PPKI_TMD,

	CTR_CERT_ISSUER_DPKI_MIN	= CTR_CERT_ISSUER_DPKI_CA,
	CTR_CERT_ISSUER_DPKI_MAX	= CTR_CERT_ISSUER_DPKI_TMD,
	CTR_CERT_ISSUER_PPKI_MIN	= CTR_CERT_ISSUER_PPKI_CA,
	CTR_CERT_ISSUER_PPKI_MAX	= CTR_CERT_ISSUER_PPKI_TMD,

	WUP_CERT_ISSUER_DPKI_MIN	= WUP_CERT_ISSUER_DPKI_CA,
	WUP_CERT_ISSUER_DPKI_MAX	= WUP_CERT_ISSUER_DPKI_SP,
	WUP_CERT_ISSUER_PPKI_MIN	= WUP_CERT_ISSUER_PPKI_CA,
	WUP_CERT_ISSUER_PPKI_MAX	= WUP_CERT_ISSUER_PPKI_TMD,
} RVL_Cert_Issuer;

// Signature issuers.
extern const char *const RVL_Cert_Issuers[RVL_CERT_ISSUER_MAX];

/**
 * Convert a certificate issuer name to RVT_Cert_Issuer, with a PKI specification.
 * @param s_issuer Issuer name.
 * @param pki PKI. (If RVL_PKI_UNKNOWN, check all PKIs. Not valid for "Root".)
 * @return RVL_Cert_Issuer, or RVL_CERT_ISSUER_UNKNOWN if invalid.
 */
RVL_Cert_Issuer cert_get_issuer_from_name_with_pki(const char *s_issuer, RVL_PKI pki);

/**
 * Convert a certificate issuer name to RVT_Cert_Issuer.
 * PKI is implied by the specified issuer. "Root" will not work here.
 * @param s_issuer Issuer name.
 * @return RVL_Cert_Issuer, or RVL_CERT_ISSUER_UNKNOWN if invalid.
 */
static inline RVL_Cert_Issuer cert_get_issuer_from_name(const char *s_issuer)
{
	return cert_get_issuer_from_name_with_pki(s_issuer, RVL_PKI_UNKNOWN);
}

/**
 * Get the PKI for an RVL_Cert_Issuer.
 * @param issuer RVL_Cert_Issuer.
 * @return PKI.
 */
RVL_PKI cert_get_pki_from_issuer(RVL_Cert_Issuer issuer);

// Opaque type for all certificates.
// Certificate users must check the certificate type value
// and cast as necessary.
typedef struct _RVL_Cert {
	uint32_t signature_type;	// [0x000] Signature type. (See Cert_SigType_e.)
} RVL_Cert;
ASSERT_STRUCT(RVL_Cert, 4);

/**
 * Get a standard certificate.
 * @param issuer RVL_Cert_Issuer
 * @return RVL_Cert*, or NULL if invalid.
 */
const RVL_Cert *cert_get(RVL_Cert_Issuer issuer);

/**
 * Get a standard certificate by issuer name, with a PKI specification.
 * @param s_issuer Issuer name.
 * @param pki PKI. (If RVL_PKI_UNKNOWN, check all PKIs. Not valid for "Root".)
 * @return RVL_Cert*, or NULL if invalid.
 */
const RVL_Cert *cert_get_from_name_with_pki(const char *s_issuer, RVL_PKI pki);

/**
 * Get a standard certificate by issuer name.
 * PKI is implied by the specified issuer. "Root" will not work here.
 * @param s_issuer Issuer name.
 * @return RVL_Cert*, or NULL if invalid.
 */
static inline const RVL_Cert *cert_get_from_name(const char *s_issuer)
{
	return cert_get_from_name_with_pki(s_issuer, RVL_PKI_UNKNOWN);
}

/**
 * Get the size of a certificate.
 * @param issuer RVL_Cert_Issuer
 * @return Certificate size, in bytes. (0 on error)
 */
unsigned int cert_get_size(RVL_Cert_Issuer issuer);

// Signature types.
typedef enum {
	RVL_CERT_SIGTYPE_RSA4096_SHA1	= 0x00010000,	// RSA-4096 with SHA-1
	RVL_CERT_SIGTYPE_RSA2048_SHA1	= 0x00010001,	// RSA-2048 with SHA-1
	RVL_CERT_SIGTYPE_ECC		= 0x00010002,	// Elliptic Curve
	WUP_CERT_SIGTYPE_RSA4096_SHA256	= 0x00010003,	// RSA-4096 with SHA-256
	WUP_CERT_SIGTYPE_RSA2048_SHA256	= 0x00010004,	// RSA-2048 with SHA-256
} RVL_Cert_SigType_e;

// Signature lengths.
typedef enum {
	RVL_CERT_SIGLENGTH_RSA4096	= 512,		// RSA-4096
	RVL_CERT_SIGLENGTH_RSA2048	= 256,		// RSA-2048
	RVL_CERT_SIGLENGTH_ECC		= 64,		// Elliptic Curve
} RVL_Cert_SigLength_e;

// Key types.
typedef enum {
	RVL_CERT_KEYTYPE_RSA4096	= 0,
	RVL_CERT_KEYTYPE_RSA2048	= 1,
	RVL_CERT_KEYTYPE_ECC		= 2,
} RVL_Cert_KeyType_e;

#pragma pack(1)

/**
 * Signature. (Dummy; no signature)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Sig_Dummy {
	uint32_t type;		// [0x000] Signature type. (Must be 0.)
	uint8_t padding1[0x3C];	// [0x004] Padding.
	char issuer[64];	// [0x040] Issuer.
} RVL_Sig_Dummy;
ASSERT_STRUCT(RVL_Sig_Dummy, 0x080);

/**
 * Signature. (RSA-4096)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Sig_RSA4096 {
	uint32_t type;					// [0x000] Signature type. (See Cert_SigType_e.)
	uint8_t sig[RVL_CERT_SIGLENGTH_RSA4096];	// [0x004] Signature.
	uint8_t padding1[0x3C];				// [0x204] Padding.
	char issuer[64];				// [0x240] Issuer.
} RVL_Sig_RSA4096;
ASSERT_STRUCT(RVL_Sig_RSA4096, 0x280);

/**
 * Public key. (RSA-4096)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_PubKey_RSA4096 {
	uint32_t type;			// [0x000] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];	// [0x004] Child certificate identity.
	uint32_t unknown;		// [0x044] // Unknown...
	uint8_t modulus[512];		// [0x048] Modulus.
	uint32_t exponent;		// [0x248] Public exponent.
	uint8_t padding2[0x34];		// [0x24C]
} RVL_PubKey_RSA4096;
ASSERT_STRUCT(RVL_PubKey_RSA4096, 0x280);

/**
 * Certificate. (RSA-4096, no signature)
 *
 * NOTE: Not actually used in the certificate chain.
 * We're using this to store the root key.
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Cert_RSA4096_KeyOnly {
	RVL_Sig_Dummy sig;	// [0x000] Dummy signature.
	RVL_PubKey_RSA4096 pub;	// [0x080] Public key.
} RVL_Cert_RSA4096_KeyOnly;
ASSERT_STRUCT(RVL_Cert_RSA4096_KeyOnly, 0x300);

/**
 * Certificate. (RSA-4096)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Cert_RSA4096 {
	RVL_Sig_RSA4096 sig;	// [0x000] Signature.
	RVL_PubKey_RSA4096 pub;	// [0x280] Public key.
} RVL_Cert_RSA4096;
ASSERT_STRUCT(RVL_Cert_RSA4096, 0x500);

/**
 * Signature. (RSA-4096)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Sig_RSA2048 {
	uint32_t type;					// [0x000] Signature type. (See Cert_SigType_e.)
	uint8_t sig[RVL_CERT_SIGLENGTH_RSA2048];	// [0x004] Signature.
	uint8_t padding1[0x3C];				// [0x104] Padding.
	char issuer[64];				// [0x140] Issuer.
} RVL_Sig_RSA2048;
ASSERT_STRUCT(RVL_Sig_RSA2048, 0x180);

/**
 * Public key. (RSA-2048)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_PubKey_RSA2048 {
	uint32_t type;			// [0x000] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];	// [0x004] Child certificate identity.
	uint32_t unknown;		// [0x044] // Unknown...
	uint8_t modulus[256];		// [0x048] Modulus.
	uint32_t exponent;		// [0x148] Public exponent.
	uint8_t padding2[0x34];		// [0x14C]
} RVL_PubKey_RSA2048;
ASSERT_STRUCT(RVL_PubKey_RSA2048, 0x180);

/**
 * Certificate. (RSA-2048)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Cert_RSA2048 {
	RVL_Sig_RSA2048 sig;	// [0x000] Signature.
	RVL_PubKey_RSA2048 pub;	// [0x180] Public key.
} RVL_Cert_RSA2048;
ASSERT_STRUCT(RVL_Cert_RSA2048, 0x300);

/**
 * Certificate. (RSA-4096 signature with RSA-2048 public key)
 * NOTE: Only the CA certificate uses this format.
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Cert_RSA4096_RSA2048 {
	RVL_Sig_RSA4096 sig;	// [0x000] Signature.
	RVL_PubKey_RSA2048 pub;	// [0x280] Public key.
} RVL_Cert_RSA4096_RSA2048;
ASSERT_STRUCT(RVL_Cert_RSA4096_RSA2048, 0x400);

/**
 * Public key. (ECC)
 *
 * All fields are big-endian.
 */
typedef struct _RVL_PubKey_ECC {
	uint32_t type;			// [0x000] Key type. (See Cert_KeyType_e.)
	char child_cert_identity[64];	// [0x004] Child certificate identity.
	uint32_t unknown;		// [0x044] // Unknown...
	uint8_t modulus[0x3C];		// [0x048] Modulus.
	uint8_t padding1[0x3C];		// [0x084]
} RVL_PubKey_ECC;
ASSERT_STRUCT(RVL_PubKey_ECC, 0xC0);

/**
 * Certificate. (RSA-2048 signature with ECC public key)
 * NOTE: Only the development debug certificate uses this format.
 *
 * All fields are big-endian.
 */
typedef struct _RVL_Cert_RSA2048_ECC {
	RVL_Sig_RSA2048 sig;	// [0x000] Signature.
	RVL_PubKey_ECC pub;	// [0x180] Public key.
} RVL_Cert_RSA2048_ECC;
ASSERT_STRUCT(RVL_Cert_RSA2048_ECC, 0x240);

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBWIICRYPTO_CERT_STORE_H__ */
