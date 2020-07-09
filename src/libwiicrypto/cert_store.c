/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * cert_store.c: Certificate store.                                        *
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

// Reference: http://wiibrew.org/wiki/Certificate_chain
#include "cert_store.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

/** Certificate data **/

// Root certificates
#include "certs/Root-dpki.h"
#include "certs/Root-ppki.h"

// Wii: dpki (Debug)
#include "certs/Root-CA00000002.h"
#include "certs/Root-CA00000002-XS00000006.h"
#include "certs/Root-CA00000002-CP00000007.h"
#include "certs/Root-CA00000002-MS00000003.h"
#include "certs/Root-CA00000002-XS00000004.h"
#include "certs/Root-CA00000002-CP00000005.h"

// Wii: ppki (Retail)
#include "certs/Root-CA00000001.h"
#include "certs/Root-CA00000001-XS00000003.h"
#include "certs/Root-CA00000001-CP00000004.h"

// 3DS: dpki (Debug)
#include "certs/Root-CA00000004.h"
#include "certs/Root-CA00000004-XS00000009.h"
#include "certs/Root-CA00000004-CP0000000a.h"

// 3DS: ppki (Retail)
#include "certs/Root-CA00000003.h"
#include "certs/Root-CA00000003-XS0000000c.h"
#include "certs/Root-CA00000003-CP0000000b.h"

// Wii U: dpki (Debug)
//#include "certs/Root-CA00000004.h"		// included above for CTR
#include "certs/Root-CA00000004-XS0000000f.h"
#include "certs/Root-CA00000004-CP00000010.h"
#include "certs/Root-CA00000004-SP0000000e.h"

// Wii U: ppki (Retail)
//#include "certs/Root-CA00000003.h"		// included above for 3DS
//#include "certs/Root-CA00000003-XS0000000c.h"	// included above for 3DS
//#include "certs/Root-CA00000003-CP0000000b.h"	// included above for 3DS

// Encryption keys. (AES-128)
const uint8_t RVL_AES_Keys[RVL_KEY_MAX][16] = {
	// RVL_KEY_DEBUG
	{0xA1,0x60,0x4A,0x6A,0x71,0x23,0xB5,0x29,
	 0xAE,0x8B,0xEC,0x32,0xC8,0x16,0xFC,0xAA},

	// RVL_KEY_RETAIL
	{0xEB,0xE4,0x2A,0x22,0x5E,0x85,0x93,0xE4,
	 0x48,0xD9,0xC5,0x45,0x73,0x81,0xAA,0xF7},

	// RVL_KEY_KOREAN
	{0x63,0xB8,0x2B,0xB4,0xF4,0x61,0x4E,0x2E,
	 0x13,0xF2,0xFE,0xFB,0xBA,0x4C,0x9B,0x7E},

	 // vWii_KEY_DEBUG
	{0x2A,0x9F,0x29,0x48,0xE6,0x23,0xCF,0x0D,
	 0x0C,0x4C,0x07,0x96,0xB2,0x1D,0xCD,0xF6},

	// vWii_KEY_RETAIL
	{0x30,0xBF,0xC7,0x6E,0x7C,0x19,0xAF,0xBB,
	 0x23,0x16,0x33,0x30,0xCE,0xD7,0xC2,0x8D},

	// WUP_KEY_DEBUG
	{0x2F,0x5C,0x1B,0x29,0x44,0xE7,0xFD,0x6F,
	 0xC3,0x97,0x96,0x4B,0x05,0x76,0x91,0xFA},

	// WUP_KEY_RETAIL
	{0xD7,0xB0,0x04,0x02,0x65,0x9B,0xA2,0xAB,
	 0xD2,0xCB,0x0D,0xB2,0x7F,0xA2,0xB6,0x56},
};

// Signature issuers.
const char *const RVL_Cert_Issuers[RVL_CERT_ISSUER_MAX] = {
	NULL,				// RVL_CERT_ISSUER_UNKNOWN

	// Root certificates
	"Root",				// RVL_CERT_ISSUER_DPKI_ROOT
	"Root",				// RVL_CERT_ISSUER_PPKI_ROOT

	// Wii: dpki (Debug)
	"Root-CA00000002",		// RVL_CERT_ISSUER_DPKI_CA
	"Root-CA00000002-XS00000006",	// RVL_CERT_ISSUER_DPKI_TICKET
	"Root-CA00000002-CP00000007",	// RVL_CERT_ISSUER_DPKI_TMD
	"Root-CA00000002-MS00000003",	// RVL_CERT_ISSUER_DPKI_MS
	// These are unused, but are present on RVT-H Readers.
	"Root-CA00000002-XS00000004",	// RVL_CERT_ISSUER_DPKI_XS04
	"Root-CA00000002-CP00000005",	// RVL_CERT_ISSUER_DPKI_CP05

	// Wii: ppki (Retail)
	"Root-CA00000001",		// RVL_CERT_ISSUER_PPKI_CA
	"Root-CA00000001-XS00000003",	// RVL_CERT_ISSUER_PPKI_TICKET
	"Root-CA00000001-CP00000004",	// RVL_CERT_ISSUER_PPKI_TMD

	// 3DS: dpki (Debug)
	"Root-CA00000004",		// CTR_CERT_ISSUER_DPKI_CA
	"Root-CA00000004-XS00000009",	// CTR_CERT_ISSUER_DPKI_TICKET
	"Root-CA00000004-CP0000000a",	// CTR_CERT_ISSUER_DPKI_TMD

	// 3DS: ppki (Retail)
	"Root-CA00000003",		// CTR_CERT_ISSUER_PPKI_CA
	"Root-CA00000003-XS0000000c",	// CTR_CERT_ISSUER_PPKI_TICKET
	"Root-CA00000003-CP0000000b",	// CTR_CERT_ISSUER_PPKI_TMD

	// Wii U: dpki (Debug)
	"Root-CA00000004",		// WUP_CERT_ISSUER_DPKI_CA (same as 3DS)
	"Root-CA00000004-XS0000000f",	// WUP_CERT_ISSUER_DPKI_TICKET
	"Root-CA00000004-CP00000010",	// WUP_CERT_ISSUER_DPKI_TMD
	"Root-CA00000004-SP0000000e",	// WUP_CERT_ISSUER_DPKI_SP (TODO: What is this?)

	// Wii U: ppki (Retail)
	"Root-CA00000003",		// WUP_CERT_ISSUER_PPKI_CA (same as 3DS)
	"Root-CA00000003-XS0000000c",	// WUP_CERT_ISSUER_PPKI_TICKET (same as 3DS)
	"Root-CA00000003-CP0000000b",	// WUP_CERT_ISSUER_PPKI_TMD (same as 3DS)
};

/** Certificate access functions. **/

/**
 * Convert a certificate issuer name to RVT_Cert_Issuer, with a PKI specification.
 * @param s_issuer Issuer name.
 * @param pki PKI. (If RVL_PKI_UNKNOWN, check all PKIs. Not valid for "Root".)
 * @return RVL_Cert_Issuer, or RVL_CERT_ISSUER_UNKNOWN if invalid.
 */
RVL_Cert_Issuer cert_get_issuer_from_name_with_pki(const char *s_issuer, RVL_PKI pki)
{
	unsigned int i;

	if (!s_issuer || s_issuer[0] == 0) {
		// Unknown certificate.
		// NOTE: If issuer is NULL, this is probably the root
		// certificate, but the root certificate isn't used by
		// discs or WAD files, so it's more likely an error.
		errno = EINVAL;
		return RVL_CERT_ISSUER_UNKNOWN;
	}

	RVL_Cert_Issuer min, max;	// max is inclusive here
	switch (pki) {
		case RVL_PKI_DPKI:
			min = RVL_CERT_ISSUER_DPKI_MIN;
			max = RVL_CERT_ISSUER_DPKI_MAX;
			break;
		case RVL_PKI_PPKI:
			min = RVL_CERT_ISSUER_PPKI_MIN;
			max = RVL_CERT_ISSUER_PPKI_MAX;
			break;
		case CTR_PKI_DPKI:
			min = CTR_CERT_ISSUER_DPKI_MIN;
			max = CTR_CERT_ISSUER_DPKI_MAX;
			break;
		case CTR_PKI_PPKI:
			min = CTR_CERT_ISSUER_PPKI_MIN;
			max = CTR_CERT_ISSUER_PPKI_MAX;
			break;
		case WUP_PKI_DPKI:
			min = WUP_CERT_ISSUER_DPKI_MIN;
			max = WUP_CERT_ISSUER_DPKI_MAX;
			break;
		case WUP_PKI_PPKI:
			min = WUP_CERT_ISSUER_PPKI_MIN;
			max = WUP_CERT_ISSUER_PPKI_MAX;
			break;
		default:
			// If the issuer is "Root" and a PKI isn't specified, something's wrong.
			if (pki == RVL_PKI_UNKNOWN && !strncmp(s_issuer, "Root", 5)) {
				// Unable to handle this one.
				errno = EINVAL;
				return RVL_CERT_ISSUER_UNKNOWN;
			}
			min = RVL_CERT_ISSUER_UNKNOWN+1;
			max = RVL_CERT_ISSUER_MAX-1;
			break;
	}

	for (i = min; i <= max; i++) {
		if (!strcmp(s_issuer, RVL_Cert_Issuers[i])) {
			// Found a match!
			return (RVL_Cert_Issuer)i;
		}
	}

	// No match.
	errno = ERANGE;
	return RVL_CERT_ISSUER_UNKNOWN;
}

/**
 * Get a standard certificate.
 * @param issuer RVL_Cert_Issuer
 * @return RVL_Cert*, or NULL if invalid.
 */
const RVL_Cert *cert_get(RVL_Cert_Issuer issuer)
{
	static const RVL_Cert *const certs[RVL_CERT_ISSUER_MAX] = {
		NULL,					// RVL_CERT_ISSUER_UNKNOWN

		// Root certificates
		(const RVL_Cert*)&Root_dpki,			// RVL_CERT_ISSUER_DPKI_ROOT
		(const RVL_Cert*)&Root_ppki,			// RVL_CERT_ISSUER_PPKI_ROOT

		// Wii: dpki (Debug)
		(const RVL_Cert*)&Root_CA00000002,		// RVL_CERT_ISSUER_DPKI_CA
		(const RVL_Cert*)&Root_CA00000002_XS00000006,	// RVL_CERT_ISSUER_DPKI_TICKET
		(const RVL_Cert*)&Root_CA00000002_CP00000007,	// RVL_CERT_ISSUER_DPKI_TMD
		(const RVL_Cert*)&Root_CA00000002_MS00000003,	// RVL_CERT_ISSUER_DPKI_MS
		(const RVL_Cert*)&Root_CA00000002_XS00000004,	// RVL_CERT_ISSUER_DPKI_XS04
		(const RVL_Cert*)&Root_CA00000002_CP00000005,	// RVL_CERT_ISSUER_DPKI_CP05

		// Wii: ppki (Retail)
		(const RVL_Cert*)&Root_CA00000001,		// RVL_CERT_ISSUER_PPKI_CA
		(const RVL_Cert*)&Root_CA00000001_XS00000003,	// RVL_CERT_ISSUER_PPKI_TICKET
		(const RVL_Cert*)&Root_CA00000001_CP00000004,	// RVL_CERT_ISSUER_PPKI_TMD

		// 3DS: dpki (Debug)
		(const RVL_Cert*)&Root_CA00000004,		// CTR_CERT_ISSUER_DPKI_CA
		(const RVL_Cert*)&Root_CA00000004_XS00000009,	// CTR_CERT_ISSUER_DPKI_TICKET
		(const RVL_Cert*)&Root_CA00000004_CP0000000a,	// CTR_CERT_ISSUER_DPKI_TMD

		// 3DS: ppki (Retail)
		(const RVL_Cert*)&Root_CA00000003,		// CTR_CERT_ISSUER_PPKI_CA
		(const RVL_Cert*)&Root_CA00000003_XS0000000c,	// CTR_CERT_ISSUER_PPKI_TICKET
		(const RVL_Cert*)&Root_CA00000003_CP0000000b,	// CTR_CERT_ISSUER_PPKI_TMD

		// Wii U: dpki (Debug)
		(const RVL_Cert*)&Root_CA00000004,		// WUP_CERT_ISSUER_DPKI_CA (same as 3DS)
		(const RVL_Cert*)&Root_CA00000004_XS0000000f,	// WUP_CERT_ISSUER_DPKI_TICKET
		(const RVL_Cert*)&Root_CA00000004_CP00000010,	// WUP_CERT_ISSUER_DPKI_TMD
		(const RVL_Cert*)&Root_CA00000004_SP0000000e,	// WUP_CERT_ISSUER_DPKI_SP

		// Wii U: ppki (Retail)
		(const RVL_Cert*)&Root_CA00000003,		// CTR_CERT_ISSUER_PPKI_CA (same as 3DS)
		(const RVL_Cert*)&Root_CA00000003_XS0000000c,	// CTR_CERT_ISSUER_PPKI_TICKET (same as 3DS)
		(const RVL_Cert*)&Root_CA00000003_CP0000000b,	// CTR_CERT_ISSUER_PPKI_TMD (same as 3DS)
	};

	assert(issuer > RVL_CERT_ISSUER_UNKNOWN && issuer < RVL_CERT_ISSUER_MAX);
	if (issuer <= RVL_CERT_ISSUER_UNKNOWN || issuer >= RVL_CERT_ISSUER_MAX) {
		errno = ERANGE;
		return NULL;
	}
	return certs[issuer];
}

/**
 * Get a standard certificate by issuer name, with a PKI specification.
 * @param s_issuer Issuer name.
 * @param pki PKI. (If RVL_PKI_UNKNOWN, check all PKIs. Not valid for "Root".)
 * @return RVL_Cert*, or NULL if invalid.
 */
const RVL_Cert *cert_get_from_name_with_pki(const char *s_issuer, RVL_PKI pki)
{
	RVL_Cert_Issuer issuer = cert_get_issuer_from_name_with_pki(s_issuer, pki);
	if (issuer == RVL_CERT_ISSUER_UNKNOWN) {
		return NULL;
	}
	return cert_get(issuer);
}

/**
 * Get the PKI for an RVL_Cert_Issuer.
 * @param issuer RVL_Cert_Issuer.
 * @return PKI.
 */
RVL_PKI cert_get_pki_from_issuer(RVL_Cert_Issuer issuer)
{
	RVL_PKI ret = RVL_PKI_UNKNOWN;

	switch (issuer) {
	// NOTE: Using RVL platform for the Root certificates.
		case RVL_CERT_ISSUER_DPKI_ROOT:		ret = RVL_PKI_DPKI; break;
		case RVL_CERT_ISSUER_PPKI_ROOT:		ret = RVL_PKI_PPKI; break;

		case RVL_CERT_ISSUER_DPKI_CA:
		case RVL_CERT_ISSUER_DPKI_TICKET:
		case RVL_CERT_ISSUER_DPKI_TMD:
		case RVL_CERT_ISSUER_DPKI_MS:
		case RVL_CERT_ISSUER_DPKI_XS04:
		case RVL_CERT_ISSUER_DPKI_CP05:		ret = RVL_PKI_DPKI; break;

		case RVL_CERT_ISSUER_PPKI_CA:
		case RVL_CERT_ISSUER_PPKI_TICKET:
		case RVL_CERT_ISSUER_PPKI_TMD:		ret = RVL_PKI_PPKI; break;

		case CTR_CERT_ISSUER_DPKI_CA:
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case CTR_CERT_ISSUER_DPKI_TMD:		ret = CTR_PKI_DPKI; break;

		case CTR_CERT_ISSUER_PPKI_CA:
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case CTR_CERT_ISSUER_PPKI_TMD:		ret = CTR_PKI_PPKI; break;

		case WUP_CERT_ISSUER_DPKI_CA:
		case WUP_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TMD:
		case WUP_CERT_ISSUER_DPKI_SP:		ret = WUP_PKI_DPKI; break;

		case WUP_CERT_ISSUER_PPKI_CA:
		case WUP_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TMD:		ret = WUP_PKI_PPKI; break;

		default:
			assert(!"Unrecognized PKI.");
			break;
	}

	return ret;
}

/**
 * Get the size of a certificate.
 * @param issuer RVL_Cert_Issuer
 * @return Certificate size, in bytes. (0 on error)
 */
unsigned int cert_get_size(RVL_Cert_Issuer issuer)
{
	static const unsigned int cert_sizes[RVL_CERT_ISSUER_MAX] = {
		0U,						// RVL_CERT_ISSUER_UNKNOWN

		// Root certificates
		(unsigned int)sizeof(Root_dpki),			// RVL_CERT_ISSUER_DPKI_ROOT
		(unsigned int)sizeof(Root_ppki),			// RVL_CERT_ISSUER_PPKI_ROOT

		// Wii: dpki (debug)
		(unsigned int)sizeof(Root_CA00000002),			// RVL_CERT_ISSUER_DPKI_CA
		(unsigned int)sizeof(Root_CA00000002_XS00000006),	// RVL_CERT_ISSUER_DPKI_TICKET
		(unsigned int)sizeof(Root_CA00000002_CP00000007),	// RVL_CERT_ISSUER_DPKI_TMD
		(unsigned int)sizeof(Root_CA00000002_MS00000003),	// RVL_CERT_ISSUER_DPKI_MS
		(unsigned int)sizeof(Root_CA00000002_XS00000004),	// RVL_CERT_ISSUER_DPKI_XS04
		(unsigned int)sizeof(Root_CA00000002_CP00000005),	// RVL_CERT_ISSUER_DPKI_CP05

		// Wii: ppki (retail)
		(unsigned int)sizeof(Root_CA00000001),			// RVL_CERT_ISSUER_PPKI_CA
		(unsigned int)sizeof(Root_CA00000001_XS00000003),	// RVL_CERT_ISSUER_PPKI_TICKET
		(unsigned int)sizeof(Root_CA00000001_CP00000004),	// RVL_CERT_ISSUER_PPKI_TMD

		// 3DS: dpki (Debug)
		(unsigned int)sizeof(Root_CA00000004),			// CTR_CERT_ISSUER_DPKI_CA
		(unsigned int)sizeof(Root_CA00000004_XS00000009),	// CTR_CERT_ISSUER_DPKI_TICKET
		(unsigned int)sizeof(Root_CA00000004_CP0000000a),	// CTR_CERT_ISSUER_DPKI_TMD

		// 3DS: ppki (Retail)
		(unsigned int)sizeof(Root_CA00000003),			// CTR_CERT_ISSUER_PPKI_CA
		(unsigned int)sizeof(Root_CA00000003_XS0000000c),	// CTR_CERT_ISSUER_PPKI_TICKET
		(unsigned int)sizeof(Root_CA00000003_CP0000000b),	// CTR_CERT_ISSUER_PPKI_TMD

		// Wii U: dpki (Debug)
		(unsigned int)sizeof(Root_CA00000004),			// WUP_CERT_ISSUER_DPKI_CA (same as 3DS)
		(unsigned int)sizeof(Root_CA00000004_XS0000000f),	// WUP_CERT_ISSUER_DPKI_TICKET
		(unsigned int)sizeof(Root_CA00000004_CP00000010),	// WUP_CERT_ISSUER_DPKI_TMD
		(unsigned int)sizeof(Root_CA00000004_SP0000000e),	// WUP_CERT_ISSUER_DPKI_SP

		// Wii U: ppki (Retail)
		(unsigned int)sizeof(Root_CA00000003),			// WUP_CERT_ISSUER_PPKI_CA (same as 3DS)
		(unsigned int)sizeof(Root_CA00000003_XS0000000c),	// WUP_CERT_ISSUER_PPKI_TICKET (same as 3DS)
		(unsigned int)sizeof(Root_CA00000003_CP0000000b),	// WUP_CERT_ISSUER_PPKI_TMD (same as 3DS)
	};

	assert(issuer > RVL_CERT_ISSUER_UNKNOWN && issuer < RVL_CERT_ISSUER_MAX);
	if (issuer <= RVL_CERT_ISSUER_UNKNOWN || issuer >= RVL_CERT_ISSUER_MAX) {
		errno = ERANGE;
		return 0;
	}
	return cert_sizes[issuer];
}
