/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * sig_tools.c: Simplified signature handling.                             *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#include "sig_tools.h"
#include "cert.h"

// C includes.
#include <assert.h>

/**
 * Convert RVL_CryptoType_e to string.
 * TODO: i18n
 * @param cryptoType Crypto type.
 * @return Crypto type. (If unknown, returns "Unknown".)
 */
const char *RVL_CryptoType_toString(RVL_CryptoType_e cryptoType)
{
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
		"",
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
