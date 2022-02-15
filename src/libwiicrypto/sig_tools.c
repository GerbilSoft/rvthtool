/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * sig_tools.c: Simplified signature handling.                             *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "sig_tools.h"
#include "cert.h"
#include "aesw.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <string.h>

/**
 * Convert RVL_CryptoType_e to string.
 * TODO: i18n
 * @param cryptoType Crypto type.
 * @return Crypto type. (If unknown, returns "Unknown".)
 */
const char *RVL_CryptoType_toString(RVL_CryptoType_e cryptoType)
{
	// FIXME: Add RVL_CryptoType_Korean_Debug and RVL_CryptoType_vWii_Debug?
	static const char *const crypto_type_tbl[] = {
		// tr: RVL_CryptoType_Unknown
		"Unknown",
		// tr: RVL_CryptoType_None
		"None",
		// tr: RVL_CryptoType_Debug
		"Debug",
		// tr: RVL_CryptoType_Retail
		"Retail",
		// tr: RVL_CryptoType_Korean
		"Korean",
		// tr: RVL_CryptoType_vWii
		"vWii",
	};

	assert(cryptoType >= RVL_CryptoType_Unknown);
	assert(cryptoType < RVL_CryptoType_MAX);
	if (cryptoType < RVL_CryptoType_Unknown || cryptoType >= RVL_CryptoType_MAX)
		return crypto_type_tbl[RVL_CryptoType_Unknown];
	return crypto_type_tbl[cryptoType];
}

/**
 * Convert RVL_SigType_e to string.
 * TODO: i18n
 * @param sigType Signature type.
 * @return Signature type. (If unknown, returns "Unknown".)
 */
const char *RVL_SigType_toString(RVL_SigType_e sigType)
{
	static const char *const sig_type_tbl[] = {
		// tr: RVL_SigType_Unknown
		"Unknown",
		// tr: RVL_SigType_Debug
		"Debug",
		// tr: RVL_SigType_Retail
		"Retail",
	};

	assert(sigType >= RVL_SigType_Unknown);
	assert(sigType < RVL_SigType_MAX);
	if (sigType < RVL_SigType_Unknown || sigType >= RVL_SigType_MAX)
		return sig_type_tbl[RVL_SigType_Unknown];
	return sig_type_tbl[sigType];
}

/**
 * Convert RVL_SigStatus_e to string.
 * TODO: i18n
 * @param sigStatus Signature status.
 * @return Signature status. (If unknown, returns "Unknown".)
 */
const char *RVL_SigStatus_toString(RVL_SigStatus_e sigStatus)
{
	static const char *const sig_status_tbl[] = {
		// tr: RVL_SigStatus_Unknown
		"Unknown",
		// tr: RVL_SigStatus_OK
		"OK",
		// tr: RVL_SigStatus_Invalid
		"INVALID",
		// tr: RVL_SigStatus_Fake
		"Fakesigned",
	};

	assert(sigStatus >= RVL_SigStatus_Unknown);
	assert(sigStatus < RVL_SigStatus_MAX);
	if (sigStatus < RVL_SigStatus_Unknown || sigStatus >= RVL_SigStatus_MAX)
		return sig_status_tbl[RVL_SigStatus_Unknown];
	return sig_status_tbl[sigStatus];
}

/**
 * Convert RVL_SigStatus_e to string.
 *
 * NOTE: This version is used for list-banks and returns a
 * string that can be appended to the signature status.
 * (Empty string for OK; space-prepended and parentheses for others.)
 *
 * TODO: i18n
 * @param sigStatus Signature status.
 * @return Signature status. (If unknown, returns "Unknown".)
 */
const char *RVL_SigStatus_toString_stsAppend(RVL_SigStatus_e sigStatus)
{
	static const char *const sig_status_tbl[] = {
		// tr: RVL_SigStatus_Unknown
		" (unknown)",
		// tr: RVL_SigStatus_OK
		" (realsigned)",
		// tr: RVL_SigStatus_Invalid
		" (INVALID)",
		// tr: RVL_SigStatus_Fake
		" (fakesigned)",
	};

	assert(sigStatus >= RVL_SigStatus_Unknown);
	assert(sigStatus < RVL_SigStatus_MAX);
	if (sigStatus < RVL_SigStatus_Unknown || sigStatus >= RVL_SigStatus_MAX)
		return sig_status_tbl[RVL_SigStatus_Unknown];
	return sig_status_tbl[sigStatus];
}

/**
 * Check the status of a Wii signature.
 * This is a wrapper around cert_verify().
 * @param data Data to verify.
 * @param size Size of data.
 * @return RVL_SigStatus_e
 */
RVL_SigStatus_e sig_verify(const uint8_t *data, size_t size)
{
	RVL_SigStatus_e sts;
	int ret = cert_verify(data, size);
	if (ret != 0) {
		// Signature verification error.
		if (ret > 0 && ((ret & SIG_ERROR_MASK) == SIG_ERROR_INVALID)) {
			// Invalid signature.
			sts = ((ret & SIG_FAIL_HASH_FAKE)
				? RVL_SigStatus_Fake
				: RVL_SigStatus_Invalid);
		} else {
			// Other error.
			sts = RVL_SigStatus_Unknown;
		}
	} else {
		// Signature is valid.
		sts = RVL_SigStatus_OK;
	}
	return sts;
}

/**
 * Re-encrypt a ticket's title key.
 * This will also change the issuer if necessary.
 *
 * NOTE: This function will NOT fakesign the ticket.
 * Call cert_fakesign_ticket() afterwards.
 * TODO: Real signing for debug.
 *
 * @param ticket Ticket.
 * @param toKey New key.
 * @return 0 on success; non-zero on error.
 */
int sig_recrypt_ticket(RVL_Ticket *ticket, RVL_AES_Keys_e toKey)
{
	// Common keys.
	RVL_AES_Keys_e fromKey;
	const uint8_t *key_from, *key_to;
	const char *issuer;
	uint8_t iv[16];	// based on Title ID

	// TODO: Error checking.
	// TODO: Pass in an aesw context for less overhead.
	AesCtx *aesw;

	// Check the 'from' key.
	if (!strncmp(ticket->issuer,
	    RVL_Cert_Issuers[RVL_CERT_ISSUER_PPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Wii: Retail.
		switch (ticket->common_key_index) {
			case 0:
			default:
				fromKey = RVL_KEY_RETAIL;
				break;
			case 1:
				fromKey = RVL_KEY_KOREAN;
				break;
			case 2:
				fromKey = vWii_KEY_RETAIL;
				break;
		}
	}
	else if (!strncmp(ticket->issuer,
		 RVL_Cert_Issuers[RVL_CERT_ISSUER_DPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Wii: Debug.
		switch (ticket->common_key_index) {
			case 0:
			default:
				fromKey = RVL_KEY_DEBUG;
				break;
			case 1:
				fromKey = RVL_KEY_KOREAN_DEBUG;
				break;
			case 2:
				fromKey = vWii_KEY_DEBUG;
				break;
		}
	}
	else if (!strncmp(ticket->issuer,
		 RVL_Cert_Issuers[WUP_CERT_ISSUER_PPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Wii U: Retail.
		fromKey = WUP_KEY_RETAIL;
	}
	else if (!strncmp(ticket->issuer,
		 RVL_Cert_Issuers[WUP_CERT_ISSUER_DPKI_TICKET], sizeof(ticket->issuer)))
	{
		// Wii U: Debug.
		fromKey = WUP_KEY_DEBUG;
	}
	else
	{
		// Unknown issuer.
		// NOTE: Should not happen...
		errno = EIO;
		return -EIO; /*RVTH_ERROR_ISSUER_UNKNOWN*/;
	}

	// TODO: Determine the 'from' key by checking the
	// original issuer and key index.
	if (fromKey == toKey) {
		// No recryption needed.
		return 0;
	}

	// Key data and 'To' issuer.
	key_from = RVL_AES_Keys[fromKey];
	key_to = RVL_AES_Keys[toKey];
	switch (toKey) {
		case RVL_KEY_RETAIL:
		case RVL_KEY_KOREAN:
		case vWii_KEY_RETAIL:
			issuer = RVL_Cert_Issuers[RVL_CERT_ISSUER_PPKI_TICKET];
			break;
		case RVL_KEY_DEBUG:
		case RVL_KEY_KOREAN_DEBUG:
		case vWii_KEY_DEBUG:
			issuer = RVL_Cert_Issuers[RVL_CERT_ISSUER_DPKI_TICKET];
			break;
		case WUP_KEY_RETAIL:
			issuer = RVL_Cert_Issuers[WUP_CERT_ISSUER_PPKI_TICKET];
			break;
		case WUP_KEY_DEBUG:
			issuer = RVL_Cert_Issuers[WUP_CERT_ISSUER_DPKI_TICKET];
			break;
		default:
			assert(!"Unsupported toKey value.");
			return -EINVAL;
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

	// Decrypt the title key with the original common key.
	aesw_set_key(aesw, key_from, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_decrypt(aesw, ticket->enc_title_key, sizeof(ticket->enc_title_key));

	// Encrypt the title key with the new common key.
	aesw_set_key(aesw, key_to, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_encrypt(aesw, ticket->enc_title_key, sizeof(ticket->enc_title_key));

	// Update the issuer.
	// NOTE: MSVC Secure Overloads will change strncpy() to strncpy_s(),
	// which doesn't clear the buffer. Hence, we'll need to explicitly
	// clear the buffer first.
	memset(ticket->issuer, 0, sizeof(ticket->issuer));
	strncpy(ticket->issuer, issuer, sizeof(ticket->issuer));

	// We're done here
	aesw_free(aesw);
	return 0;
}
