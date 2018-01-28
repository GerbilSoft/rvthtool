/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * priv_key_store.h: Private key store.                                    *
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

#ifndef __RVTHTOOL_LIBRVTH_PRIV_KEY_STORE_H__
#define __RVTHTOOL_LIBRVTH_PRIV_KEY_STORE_H__

#include <stdint.h>
#include "rsaw.h"

#ifdef __cplusplus
extern "C" {
#endif

// These private keys consist of p, q, a, b, and c.
// Technically, only p and q are important, but calculating
// a, b, and c is a pain.

extern const RSA2048PrivateKey rvth_privkey_debug_ticket;
extern const RSA2048PrivateKey rvth_privkey_debug_tmd;

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_PRIV_KEY_STORE_H__ */
