/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * sig_tools.h: Simplified signature handling.                             *
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

#ifndef __RVTHTOOL_LIBWIICRYPTO_SIG_TOOLS_H__
#define __RVTHTOOL_LIBWIICRYPTO_SIG_TOOLS_H__

#include "wii_structs.h"
#include "cert_store.h"

// C includes.
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Encryption types.
typedef enum {
	RVL_CryptoType_Unknown	= 0,	// Unknown encryption.
	RVL_CryptoType_None	= 1,	// No encryption.
	RVL_CryptoType_Debug	= 2,	// RVT-R encryption.
	RVL_CryptoType_Retail	= 3,	// Retail encryption.
	RVL_CryptoType_Korean	= 4,	// Korean retail encryption.

	RVL_CryptoType_MAX
} RVL_CryptoType_e;

// Signature types.
typedef enum {
	RVL_SigType_Unknown	= 0,	// Unknown signature.
	RVL_SigType_Debug	= 1,	// Debug signature.
	RVL_SigType_Retail	= 2,	// Retail signature.

	RVL_SigType_MAX
} RVL_SigType_e;

// Signature status.
typedef enum {
	RVL_SigStatus_Unknown	= 0,	// Unknown signature status.
	RVL_SigStatus_OK	= 1,	// Valid
	RVL_SigStatus_Invalid	= 2,	// Invalid
	RVL_SigStatus_Fake	= 3,	// Fakesigned

	RVL_SigStatus_MAX
} RVL_SigStatus_e;

/**
 * Convert RVL_CryptoType_e to string.
 * TODO: i18n
 * @param cryptoType Crypto type.
 * @return Crypto type. (If unknown, returns "Unknown".)
 */
const char *RVL_CryptoType_toString(RVL_CryptoType_e cryptoType);

/**
 * Convert RVL_SigType_e to string.
 * TODO: i18n
 * @param sigType Signature type.
 * @return Signature type. (If unknown, returns "Unknown".)
 */
const char *RVL_SigType_toString(RVL_SigType_e sigType);

/**
 * Convert RVL_SigStatus_e to string.
 * TODO: i18n
 * @param sigStatus Signature status.
 * @return Signature status. (If unknown, returns "Unknown".)
 */
const char *RVL_SigStatus_toString(RVL_SigStatus_e sigStatus);

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
const char *RVL_SigStatus_toString_stsAppend(RVL_SigStatus_e sigStatus);

/**
 * Check the status of a Wii signature.
 * This is a wrapper around cert_verify().
 * @param data Data to verify.
 * @param size Size of data.
 * @return RVL_SigStatus_e
 */
RVL_SigStatus_e sig_verify(const uint8_t *data, size_t size);

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
int sig_recrypt_ticket(RVL_Ticket *ticket, RVL_AES_Keys_e toKey);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBWIICRYPTO_SIG_TOOLS_H__ */
