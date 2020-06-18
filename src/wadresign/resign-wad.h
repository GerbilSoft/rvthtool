/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * resign-wad.h: Re-sign a WAD file.                                       *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_WADRESIGN_RESIGN_WAD_H__
#define __RVTHTOOL_WADRESIGN_RESIGN_WAD_H__

#include "tcharx.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WAD format.
 */
typedef enum {
	WAD_Format_Standard	= 0,	// Standard (.wad)
	WAD_Format_BroadOn	= 1,	// BroadOn WAD Format (.bwf)
} WAD_Format_e;

/**
 * 'resign' command.
 * @param src_wad	[in] Source WAD.
 * @param dest_wad	[in] Destination WAD.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @param output_format	[in] Output format. (-1 for default)
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int resign_wad(const TCHAR *src_wad, const TCHAR *dest_wad, int recrypt_key, int output_format);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_WADRESIGN_PRINT_INFO_H__ */
