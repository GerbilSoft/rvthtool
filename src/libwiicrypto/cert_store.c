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

// Wii: dpki
#include "certs/Root-RVL-dpki.h"
#include "certs/Root-CA00000002.h"
#include "certs/Root-CA00000002-XS00000006.h"
#include "certs/Root-CA00000002-CP00000007.h"
#include "certs/Root-CA00000002-MS00000003.h"
#include "certs/Root-CA00000002-XS00000004.h"
#include "certs/Root-CA00000002-CP00000005.h"

// Wii: ppki
#include "certs/Root-RVL-ppki.h"
#include "certs/Root-CA00000001.h"
#include "certs/Root-CA00000001-XS00000003.h"
#include "certs/Root-CA00000001-CP00000004.h"

// Wii U: dpki
//#include "certs/Root-WUP-dpki.h"
#include "certs/Root-CA00000004.h"
#include "certs/Root-CA00000004-XS0000000f.h"
#include "certs/Root-CA00000004-CP00000010.h"
#include "certs/Root-CA00000004-SP0000000e.h"

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

	// RVL_KEY_vWii [FIXME: Debug version?]
	{0x30,0xBF,0xC7,0x6E,0x7C,0x19,0xAF,0xBB,
	 0x23,0x16,0x33,0x30,0xCE,0xD7,0xC2,0x8D},
};

// Signature issuers.
const char *const RVL_Cert_Issuers[RVL_CERT_ISSUER_MAX] = {
	NULL,				// RVL_CERT_ISSUER_UNKNOWN

	// Wii: Debug
	"Root",				// RVL_CERT_ISSUER_DPKI_ROOT
	"Root-CA00000002",		// RVL_CERT_ISSUER_DPKI_CA
	"Root-CA00000002-XS00000006",	// RVL_CERT_ISSUER_DPKI_TICKET
	"Root-CA00000002-CP00000007",	// RVL_CERT_ISSUER_DPKI_TMD
	"Root-CA00000002-MS00000003",	// RVL_CERT_ISSUER_DPKI_MS
	// These are unused, but are present on RVT-H Readers.
	"Root-CA00000002-XS00000004",	// RVL_CERT_ISSUER_DPKI_XS04
	"Root-CA00000002-CP00000005",	// RVL_CERT_ISSUER_DPKI_CP05

	// Wii: Retail
	"Root",				// RVL_CERT_ISSUER_PPKI_ROOT
	"Root-CA00000001",		// RVL_CERT_ISSUER_PPKI_CA
	"Root-CA00000001-XS00000003",	// RVL_CERT_ISSUER_PPKI_TICKET
	"Root-CA00000001-CP00000004",	// RVL_CERT_ISSUER_PPKI_TMD

	// Wii U: Debug
	//"Root",				// WUP_CERT_ISSUER_DPKI_ROOT (TODO)
	"Root-CA00000004",		// WUP_CERT_ISSUER_DPKI_CA
	"Root-CA00000004-XS0000000f",	// WUP_CERT_ISSUER_DPKI_TICKET
	"Root-CA00000004-CP00000010",	// WUP_CERT_ISSUER_DPKI_TMD
	"Root-CA00000004-SP0000000e",	// WUP_CERT_ISSUER_DPKI_SP
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
		case WUP_PKI_DPKI:
			min = WUP_CERT_ISSUER_DPKI_MIN;
			max = WUP_CERT_ISSUER_DPKI_MAX;
			break;
		/*case WUP_PKI_PPKI: // TODO
			min = WUP_CERT_ISSUER_PPKI_MIN;
			min = WUP_CERT_ISSUER_PPKI_MAX;
			break;
		*/
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

		// Wii: Debug
		(const RVL_Cert*)&Root_RVL_dpki,		// RVL_CERT_ISSUER_DPKI_ROOT
		(const RVL_Cert*)&Root_CA00000002,		// RVL_CERT_ISSUER_DPKI_CA
		(const RVL_Cert*)&Root_CA00000002_XS00000006,	// RVL_CERT_ISSUER_DPKI_TICKET
		(const RVL_Cert*)&Root_CA00000002_CP00000007,	// RVL_CERT_ISSUER_DPKI_TMD
		(const RVL_Cert*)&Root_CA00000002_MS00000003,	// RVL_CERT_ISSUER_DPKI_MS
		(const RVL_Cert*)&Root_CA00000002_XS00000004,	// RVL_CERT_ISSUER_DPKI_XS04
		(const RVL_Cert*)&Root_CA00000002_CP00000005,	// RVL_CERT_ISSUER_DPKI_CP05

		// Wii: Retail
		(const RVL_Cert*)&Root_RVL_ppki,		// RVL_CERT_ISSUER_PPKI_ROOT
		(const RVL_Cert*)&Root_CA00000001,		// RVL_CERT_ISSUER_PPKI_CA
		(const RVL_Cert*)&Root_CA00000001_XS00000003,	// RVL_CERT_ISSUER_PPKI_TICKET
		(const RVL_Cert*)&Root_CA00000001_CP00000004,	// RVL_CERT_ISSUER_PPKI_TMD

		// Wii U: Debug
		//(const RVL_Cert*)&Root_WUP_dpki,		// WUP_CERT_ISSUER_DPKI_ROOT (TODO)
		(const RVL_Cert*)&Root_CA00000004,		// WUP_CERT_ISSUER_DPKI_CA
		(const RVL_Cert*)&Root_CA00000004_XS0000000f,	// WUP_CERT_ISSUER_DPKI_TICKET
		(const RVL_Cert*)&Root_CA00000004_CP00000010,	// WUP_CERT_ISSUER_DPKI_TMD
		(const RVL_Cert*)&Root_CA00000004_SP0000000e,	// WUP_CERT_ISSUER_DPKI_SP
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

	if (issuer >= RVL_CERT_ISSUER_DPKI_MIN && issuer <= RVL_CERT_ISSUER_DPKI_MAX) {
		ret = RVL_PKI_DPKI;
	} if (issuer >= RVL_CERT_ISSUER_PPKI_MIN && issuer <= RVL_CERT_ISSUER_PPKI_MAX) {
		ret = RVL_PKI_PPKI;
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

		// dpki (debug)
		(unsigned int)sizeof(Root_RVL_dpki),			// RVL_CERT_ISSUER_DPKI_ROOT
		(unsigned int)sizeof(Root_CA00000002),			// RVL_CERT_ISSUER_DPKI_CA
		(unsigned int)sizeof(Root_CA00000002_XS00000006),	// RVL_CERT_ISSUER_DPKI_TICKET
		(unsigned int)sizeof(Root_CA00000002_CP00000007),	// RVL_CERT_ISSUER_DPKI_TMD
		(unsigned int)sizeof(Root_CA00000002_MS00000003),	// RVL_CERT_ISSUER_DPKI_MS
		(unsigned int)sizeof(Root_CA00000002_XS00000004),	// RVL_CERT_ISSUER_DPKI_XS04
		(unsigned int)sizeof(Root_CA00000002_CP00000005),	// RVL_CERT_ISSUER_DPKI_CP05

		// ppki (retail)
		(unsigned int)sizeof(Root_RVL_ppki),			// RVL_CERT_ISSUER_PPKI_ROOT
		(unsigned int)sizeof(Root_CA00000001),			// RVL_CERT_ISSUER_PPKI_CA
		(unsigned int)sizeof(Root_CA00000001_XS00000003),	// RVL_CERT_ISSUER_PPKI_TICKET
		(unsigned int)sizeof(Root_CA00000001_CP00000004),	// RVL_CERT_ISSUER_PPKI_TMD

		// Wii U: Debug
		//(unsigned int)sizeof(Root_WUP_dpki),			// WUP_CERT_ISSUER_DPKI_ROOT (TODO)
		(unsigned int)sizeof(Root_CA00000004),			// WUP_CERT_ISSUER_DPKI_CA
		(unsigned int)sizeof(Root_CA00000004_XS0000000f),	// WUP_CERT_ISSUER_DPKI_TICKET
		(unsigned int)sizeof(Root_CA00000004_CP00000010),	// WUP_CERT_ISSUER_DPKI_TMD
		(unsigned int)sizeof(Root_CA00000004_SP0000000e),	// WUP_CERT_ISSUER_DPKI_SP
	};

	assert(issuer > RVL_CERT_ISSUER_UNKNOWN && issuer < RVL_CERT_ISSUER_MAX);
	if (issuer <= RVL_CERT_ISSUER_UNKNOWN || issuer >= RVL_CERT_ISSUER_MAX) {
		errno = ERANGE;
		return 0;
	}
	return cert_sizes[issuer];
}
