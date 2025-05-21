/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * disc_header.hpp: Read a GCN/Wii disc header and determine its type.     *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "libwiicrypto/common.h"

#include <stdint.h>

class RefFile;
struct _GCN_DiscHeader;

/**
 * Check the magic numbers in a GCN/Wii disc header.
 *
 * NOTE: This function cannot currently distinguish between Wii SL
 * and Wii DL images.
 *
 * NOTE 2: This function will not check if all values are 0.
 * It will return RVTH_BankType_Unknown if the header is empty.
 *
 * @param discHeader	[in] GCN disc header.
 * @return Bank type, or negative POSIX error code. (See RvtH_BankType_e.)
 */
int rvth_disc_header_identify(const struct _GCN_DiscHeader *discHeader);

/**
 * Read a GCN/Wii disc header and determine its type.
 *
 * On some RVT-H firmware versions, pressing the "flush" button zeroes
 * out the first 16 KB of the bank in addition to clearing the bank entry.
 * This 16 KB is only the disc header for Wii games, so that can be
 * reconstructed by reading the beginning of the Game Partition.
 *
 * NOTE: This function cannot currently distinguish between Wii SL
 * and Wii DL images.
 *
 * @param f_img		[in] Disc image file.
 * @param lba_start	[in] Starting LBA.
 * @param discHeader	[out] GCN disc header. (Not filled in if empty or unknown types.)
 * @param pIsDeleted	[out,opt] Set to true if the image appears to be "deleted".
 * @return Bank type, or negative POSIX error code. (See RvtH_BankType_e.)
 */
int rvth_disc_header_get(RefFile *f_img, uint32_t lba_start,
	struct _GCN_DiscHeader *discHeader, bool *pIsDeleted);
