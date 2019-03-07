/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * print-info.h: Print WAD information.                                    *
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

#ifndef __RVTHTOOL_WADRESIGN_PRINT_INFO_H__
#define __RVTHTOOL_WADRESIGN_PRINT_INFO_H__

#include <stdint.h>
#include <stdio.h>

#include "librvth/tcharx.h"

// TODO: Custom stdbool.x instead of libwiicrypto/common.h.
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Is an issuer retail or debug?
 * @param issuer RVL_Cert_Issuer
 * @return "Retail", "Debug", or "Unknown".
 */
const char *issuer_type(RVL_Cert_Issuer issuer);

/**
 * Identify a WAD file's type.
 *
 * This is mostly for informational purposes, except for early devkit WAD files,
 * in which case the format is slightly different.
 *
 * @param pBuf		[in] Header data.
 * @param buf_len	[in] Length of buf. (Should be at least 64.)
 * @param pIsEarly 	[out] `bool` to store if the WAD file is an early devkit WAD file or not.
 * @return WAD file type as a string, or NULL on error.
 */
const char *identify_wad_type(const uint8_t *buf, size_t buf_len, bool *pIsEarly);

/**
 * 'info' command. (internal function)
 * @param f_wad		[in] Opened WAD file.
 * @param wad_filename	[in] WAD filename. (for error messages)
 * @param verify	[in] If true, verify the contents.
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_wad_info_FILE(FILE *f_wad, const TCHAR *wad_filename, bool verify);

/**
 * 'info' command.
 * @param wad_filename	[in] WAD filename.
 * @param verify	[in] If true, verify the contents.
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_wad_info(const TCHAR *wad_filename, bool verify);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_WADRESIGN_PRINT_INFO_H__ */
