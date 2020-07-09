/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * print-info.h: Print WAD information.                                    *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_WADRESIGN_PRINT_INFO_H__
#define __RVTHTOOL_WADRESIGN_PRINT_INFO_H__

#include <stdint.h>
#include <stdio.h>

#include "tcharx.h"

// TODO: Custom stdbool.x instead of libwiicrypto/common.h.
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Identify a WAD file's type.
 *
 * This is mostly for informational purposes, except for BroadOn WAD files,
 * in which case the format is slightly different.
 *
 * @param pBuf		[in] Header data.
 * @param buf_len	[in] Length of buf. (Should be at least 64.)
 * @param pIsBWF 	[out] `bool` to store if the WAD file is a BroadOn WAD file or not.
 * @return WAD file type as a string, or NULL on error.
 */
const char *identify_wad_type(const uint8_t *buf, size_t buf_len, bool *pIsBWF);

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
