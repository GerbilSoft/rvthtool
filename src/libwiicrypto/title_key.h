/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * title_key.h: Title key management.                                      *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "wii_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decrypt a Wii title key.
 * TODO: Pass in an aesw context for less overhead.
 *
 * @param ticket	[in] Ticket.
 * @param titleKey	[out] Output buffer for the title key. (Must be 16 bytes.)
 * @param crypto_type	[out] Encryption type. (See RVL_CryptoType_e.)
 * @return 0 on success; non-zero on error.
 */
int decrypt_title_key(const RVL_Ticket *ticket, uint8_t *titleKey, uint8_t *crypto_type);

#ifdef __cplusplus
}
#endif
