/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * cert.h: Certificate management.                                         *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "rsaw.h"
#include "wii_structs.h"

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

	// The following values indicate the signature was
	// processed, but is not valid or is fakesigned.
	SIG_FAIL_MASK                   = (0xFF << 8),

	// PKCS#1 header is incorrect.
	SIG_FAIL_HEADER_ERROR           = (1 << (0+8)),

	// Padding is incorrect.
	SIG_FAIL_PADDING_ERROR          = (1 << (1+8)),

	// Missing 0x00 byte between padding and data.
	SIG_FAIL_NO_SEPARATOR_ERROR     = (1 << (2+8)),

	// DER type is incorrect.
	SIG_FAIL_DER_TYPE_ERROR         = (1 << (3+8)),

	// Hash is incorrect.
	SIG_FAIL_HASH_ERROR             = (1 << (4+8)),

	// Hash is fakesigned. (starts with 0x00)
	SIG_FAIL_HASH_FAKE              = (1 << (5+8)),
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

/**
 * Fakesign a ticket.
 *
 * NOTE: If changing the encryption type, the issuer and title key
 * must be updated *before* calling this function.
 *
 * @param ticket_u8 Ticket to fakesign.
 * @param size Size of ticket.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_fakesign_ticket(uint8_t *ticket_u8, size_t size);

/**
 * Fakesign a TMD.
 *
 * NOTE: If changing the encryption type, the issuer must be
 * updated *before* calling this function.
 *
 * @param tmd TMD to fakesign.
 * @param size Size of TMD.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_fakesign_tmd(uint8_t *tmd, size_t size);

/**
 * Sign a ticket or TMD with real encryption keys.
 *
 * NOTE: If changing the encryption type, the issuer must be
 * updated *before* calling this function.
 *
 * NOTE 2: For Wii, the full TMD must be signed.
 * For Wii U, only the TMD header is signed.
 *
 * @param data		[in/out] Ticket or TMD to fakesign.
 * @param size		[in] Size of ticket or TMD.
 * @param key		[in] RSA-2048 private key.
 * @return 0 on success; negative POSIX error code on error.
 */
int cert_realsign_ticketOrTMD(uint8_t *data, size_t size, const RSA2048PrivateKey *key);

#ifdef __cplusplus
}
#endif
