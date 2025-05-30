/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * priv_key_store.h: Private key store.                                    *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include "rsaw.h"

#ifdef __cplusplus
extern "C" {
#endif

// These private keys consist of p, q, a, b, and c.
// Technically, only p and q are important, but calculating
// a, b, and c is a pain.

extern const RSA2048PrivateKey rvth_privkey_RVL_dpki_ticket;
extern const RSA2048PrivateKey rvth_privkey_RVL_dpki_tmd;

extern const RSA2048PrivateKey rvth_privkey_WUP_dpki_ticket;
extern const RSA2048PrivateKey rvth_privkey_WUP_dpki_tmd;

#ifdef __cplusplus
}
#endif
