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

#ifndef __RVTHTOOL_LIBRVTH_CERT_H__
#define __RVTHTOOL_LIBRVTH_CERT_H__

#include "common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signature status.
// Based on Wiimm's ISO Tools' cert_stat_t:
// - https://github.com/mirror/wiimms-iso-tools/blob/8f1b6ecb0cbac3bdc83ceaeb7cff3c9375974485/src/libwbfs/cert.h
// TODO: Sig_Status to String function.
typedef enum {
	// Signature is valid.
	SIG_STATUS_OK = 0,

	// Signature error mask.
	SIG_ERROR_MASK = (0xFF),

	// Signature is invalid.
	// Check SIG_FAIL_MASK for the exact error.
	SIG_ERROR_INVALID = 1,

	// Unsupported signature type.
	SIG_ERROR_UNSUPPORTED_SIGNATURE_TYPE = 2,

	// Unknown issuer.
	SIG_ERROR_UNKNOWN_ISSUER = 3,

	// Signature does not match the declared type.
	SIG_ERROR_WRONG_TYPE_DECLARATION = 4,

	// Signature's magic number does not match the issuer.
	SIG_ERROR_WRONG_MAGIC_NUMBER = 5,

	// The following values indicate the signature was
	// processed, but is not valid or is fakesigned.
	SIG_FAIL_MASK		= (0xFF << 8),

	// Signature base data is incorrect.
	SIG_FAIL_BASE_ERROR	= (1 << (0+8)),

	// Hash is incorrect.
	SIG_FAIL_HASH_ERROR	= (1 << (1+8)),

	// Hash is fakesigned. (starts with 0x00)
	SIG_FAIL_HASH_FAKE	= (1 << (2+8)),
} Sig_Status;

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
int cert_verify(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_CERT_STORE_H__ */
