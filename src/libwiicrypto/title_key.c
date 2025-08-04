/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * title_key.c: Title key management.                                      *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "title_key.h"

#include "aesw.h"
#include "cert_store.h"
#include "sig_tools.h"

// C includes
#include <errno.h>
#include <string.h>

/**
 * Decrypt the title key.
 * TODO: Pass in an aesw context for less overhead.
 *
 * @param ticket	[in] Ticket.
 * @param titleKey	[out] Output buffer for the title key. (Must be 16 bytes.)
 * @param crypto_type	[out] Encryption type. (See RVL_CryptoType_e.)
 * @return 0 on success; non-zero on error.
 */
int decrypt_title_key(const RVL_Ticket *ticket, uint8_t *titleKey, uint8_t *crypto_type)
{
	const uint8_t *commonKey;
	uint8_t iv[16];	// based on Title ID

	// TODO: Error checking.
	// TODO: Pass in an aesw context for less overhead.
	AesCtx *aesw;

	// Check the 'from' key.
	if (!strncmp(ticket->issuer,
	    RVL_Cert_Issuers[RVL_CERT_ISSUER_PPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Retail
		// TODO: Heuristic for invalid key indexes? (retail vs. Korean)
		switch (ticket->common_key_index) {
			case 0:
			default:
				commonKey = RVL_AES_Keys[RVL_KEY_RETAIL];
				*crypto_type = RVL_CryptoType_Retail;
				break;
			case 1:
				commonKey = RVL_AES_Keys[RVL_KEY_KOREAN];
				*crypto_type = RVL_CryptoType_Korean;
				break;
			case 2:
				commonKey = RVL_AES_Keys[vWii_KEY_RETAIL];
				*crypto_type = RVL_CryptoType_vWii;
				break;
		}
	}
	else if (!strncmp(ticket->issuer,
		 RVL_Cert_Issuers[RVL_CERT_ISSUER_DPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Debug
		// TODO: Heuristic for invalid key indexes? (debug vs. Korean)
		switch (ticket->common_key_index) {
			case 0:
			default:
				commonKey = RVL_AES_Keys[RVL_KEY_DEBUG];
				*crypto_type = RVL_CryptoType_Debug;
				break;
			case 1:
				// TODO: RVL_CryptoType_Korean_Debug?
				commonKey = RVL_AES_Keys[RVL_KEY_KOREAN_DEBUG];
				*crypto_type = RVL_CryptoType_Korean;
				break;
			case 2:
				commonKey = RVL_AES_Keys[vWii_KEY_DEBUG];
				*crypto_type = RVL_CryptoType_vWii_Debug;
				break;
		}
	}
	else
	{
		// Unknown issuer.
		// TODO: RVTH_ERROR_ISSUER_UNKNOWN? Can't use it in libwiicrypto...
		errno = EIO;
		return -EIO;
	}

	// Initialize the AES context.
	errno = 0;
	aesw = aesw_new();
	if (!aesw) {
		int ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	}

	// IV is the 64-bit title ID, followed by zeroes.
	memcpy(iv, &ticket->title_id, 8);
	memset(&iv[8], 0, 8);

	// Decrypt the key with the original common key.
	memcpy(titleKey, ticket->enc_title_key, 16);
	aesw_set_key(aesw, commonKey, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_decrypt(aesw, titleKey, 16);

	// We're done here
	aesw_free(aesw);
	return 0;
}
