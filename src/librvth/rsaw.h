/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rsaw.h: RSA encryption wrapper functions.                               *
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

#ifndef __RVTHTOOL_LIBRVTH_RSAW_H__
#define __RVTHTOOL_LIBRVTH_RSAW_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decrypt an RSA signature.
 * @param buf		[out] Output buffer. (Must be `size` bytes.)
 * @param modulus	[in] Public key modulus. (Must be `size` bytes.)
 * @param exponent	[in] Public key exponent.
 * @param sig		[in] Signature. (Must be `size` bytes.)
 * @param size		[in] Signature size. (256 for RSA-2048; 512 for RSA-4096.)
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_decrypt_signature(uint8_t *buf, const uint8_t *modulus,
	uint32_t exponent, const uint8_t *sig, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RSAW_H__ */
