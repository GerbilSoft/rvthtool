/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * disc_header.c: Read a GCN/Wii disc header and determine its type.       *
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

#include "disc_header.h"
#include "gcn_structs.h"
#include "rvth_p.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

// NDDEMO header.
// Used in early GameCube tech demos.
// Note the lack of a GameCube magic number.
static const uint8_t nddemo_header[64] = {
	0x30, 0x30, 0x00, 0x45, 0x30, 0x31, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x4E, 0x44, 0x44, 0x45, 0x4D, 0x4F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

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
int rvth_disc_header_identify(const GCN_DiscHeader *discHeader)
{
	assert(discHeader != NULL);
	if (!discHeader) {
		return -EINVAL;
	}

	if (discHeader->magic_wii == cpu_to_be32(WII_MAGIC)) {
		// Wii disc image.
		// TODO: Detect SL vs. DL.
		return RVTH_BankType_Wii_SL;
	} else if (discHeader->magic_gcn == cpu_to_be32(GCN_MAGIC)) {
		// GameCube disc image.
		return RVTH_BankType_GCN;
	}

	// Check for GameCube NDDEMO.
	if (!memcmp(discHeader, nddemo_header, sizeof(nddemo_header))) {
		// NDDEMO header found.
		return RVTH_BankType_GCN;
	}

	// Unknown...
	return RVTH_BankType_Unknown;
}

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
 * @param discHeader	[out] GCN disc header.
 * @param pIsDeleted	[out,opt] Set to true if the image appears to be "deleted".
 * @return Bank type, or negative POSIX error code. (See RvtH_BankType_e.)
 */
int rvth_disc_header_get(RefFile *f_img, uint32_t lba_start, GCN_DiscHeader *discHeader, bool *pIsDeleted)
{
	int ret;
	size_t size;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
	} sector_buf;

	assert(f_img != NULL);
	assert(discHeader != NULL);
	if (!f_img || !discHeader) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Read the disc header.
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno != EIO) {
			errno = EIO;
		}
		return -errno;
	}
	size = ref_read(sector_buf.u8, 1, sizeof(sector_buf.u8), f_img);
	if (size != sizeof(sector_buf.u8)) {
		// Read error.
		if (errno != EIO) {
			errno = EIO;
		}
		return -errno;
	}

	ret = rvth_disc_header_identify(&sector_buf.gcn);
	if (ret < 0) {
		// Error...
		errno = -ret;
		return ret;
	}

	if (ret > RVTH_BankType_Unknown) {
		// Known magic found.
		if (pIsDeleted) {
			pIsDeleted = false;
		}
		memcpy(discHeader, &sector_buf.gcn, sizeof(*discHeader));
		return ret;
	}

	// If unknown, check if the entire sector is empty.
	if (ret == RVTH_BankType_Unknown) {
		if (rvth_is_block_empty(sector_buf.u8, sizeof(sector_buf.u8))) {
			// Empty sector.
			ret = RVTH_BankType_Empty;
		}
	}

	if (ret != RVTH_BankType_Empty) {
		// Not empty.
		// TODO: Check for the Wii header anyway?
		if (pIsDeleted) {
			*pIsDeleted = false;
		}
		memcpy(discHeader, &sector_buf.gcn, sizeof(*discHeader));
		return ret;
	} else {
		// Empty. Clear the discHeader for now.
		memset(discHeader, 0, sizeof(*discHeader));
	}

	// TODO: Read the Wii partition table and see if we can
	// obtain the GCN header that way.
	return ret;
}
