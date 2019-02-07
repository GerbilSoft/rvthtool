/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * disc_header.hpp: Read a GCN/Wii disc header and determine its type.     *
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

#ifndef __RVTHTOOL_LIBRVTH_DISC_HEADER_H__
#define __RVTHTOOL_LIBRVTH_DISC_HEADER_H__

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

#endif /* __RVTHTOOL_LIBRVTH_DISC_HEADER_H__ */
