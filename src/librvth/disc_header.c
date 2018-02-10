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
#include "reader.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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

// Volume group and partition table.
// NOTE: Only reading the first partition table,
// which we're assuming is stored directly after
// the volume group table.
// FIXME: Handle other cases when using direct device access on Windows.
typedef union _ptbl_t {
	uint8_t u8[RVTH_BLOCK_SIZE];
	struct {
		RVL_VolumeGroupTable vgtbl;
		RVL_PartitionTableEntry ptbl[15];
	};
} ptbl_t;

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
 * Find the game partition in a Wii disc image.
 * @param pt	[in] Partition table.
 * @return Game partition LBA (relative to start of disc), or 0 on error.
 */
static uint32_t rvth_find_GamePartition_int(const ptbl_t *pt)
{
	unsigned int i;
	uint32_t ptcount;
	int64_t addr;

	// Game partition is always in volume group 0.
	// TODO: Use uint32_t arithmetic instead of int64_t?
	addr = ((int64_t)be32_to_cpu(pt->vgtbl.vg[0].addr) << 2);
	if (addr != (RVL_VolumeGroupTable_ADDRESS + sizeof(pt->vgtbl))) {
		// Partition table offset isn't supported right now.
		return 0;
	}

	ptcount = be32_to_cpu(pt->vgtbl.vg[0].count);
	if (ptcount > ARRAY_SIZE(pt->ptbl)) {
		// Can't check this many partitions.
		// Reduce it to the maximum we can check.
		ptcount = ARRAY_SIZE(pt->ptbl);
	}
	for (i = 0; i < ptcount; i++) {
		if (pt->ptbl[i].type == cpu_to_be32(0)) {
			// Found the game partition.
			return (be32_to_cpu(pt->ptbl[i].addr) / (RVTH_BLOCK_SIZE/4));
		}
	}

	// No game partition found...
	return 0;
}

/**
 * Find the game partition in a Wii disc image.
 * @param reader	[in] Reader*
 * @return Game partition LBA (relative to start of reader), or 0 on error.
 */
uint32_t rvth_find_GamePartition(Reader *reader)
{
	// Assuming this is a valid Wii disc image.
	uint32_t lba_size;
	ptbl_t pt;

	assert(reader != NULL);
	if (!reader) {
		return 0;
	}

	// Get the volume group table.
	lba_size = reader_read(reader, &pt, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 1);
	if (lba_size != 1) {
		// Read error.
		return 0;
	}

	return rvth_find_GamePartition_int(&pt);
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
 * @param discHeader	[out] GCN disc header. (Not filled in if empty or unknown types.)
 * @param pIsDeleted	[out,opt] Set to true if the image appears to be "deleted".
 * @return Bank type, or negative POSIX error code. (See RvtH_BankType_e.)
 */
int rvth_disc_header_get(RefFile *f_img, uint32_t lba_start, GCN_DiscHeader *discHeader, bool *pIsDeleted)
{
	int ret = 0;	// errno setting
	size_t size;
	bool isDeleted = false;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
		ptbl_t pt;
	} sbuf;
	uint32_t game_lba;
	int bankType;

	// Wii partition header.
	RVL_PartitionHeader *pthdr = NULL;
	int64_t data_offset;

	assert(f_img != NULL);
	assert(discHeader != NULL);
	if (!f_img || !discHeader) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Clear the returned discHeader struct initially.
	memset(discHeader, 0, sizeof(*discHeader));

	// Read the disc header.
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		goto end;
	}
	errno = 0;
	size = ref_read(sbuf.u8, 1, sizeof(sbuf.u8), f_img);
	if (size != sizeof(sbuf.u8)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	ret = rvth_disc_header_identify(&sbuf.gcn);
	if (ret < 0) {
		// Error...
		goto end;
	}

	if (ret > RVTH_BankType_Unknown) {
		// Known magic found.
		memcpy(discHeader, &sbuf.gcn, sizeof(*discHeader));
		goto end;
	}

	// If unknown, check if the entire sector is empty.
	if (ret == RVTH_BankType_Unknown) {
		if (rvth_is_block_empty(sbuf.u8, sizeof(sbuf.u8))) {
			// Empty sector.
			ret = RVTH_BankType_Empty;
		}
	}

	if (ret != RVTH_BankType_Empty) {
		// Not empty.
		// TODO: Check for the Wii header anyway?
		memcpy(discHeader, &sbuf.gcn, sizeof(*discHeader));
		goto end;
	}

	// For Wii games, we can recover the disc header by reading the
	// Game Partition. The Game Partition header is identical, except
	// hash_verify and disc_noCrypt will always be 1. We'll need to
	// set those to 0 if the image is encrypted.
	bankType = ret;

	// Get the volume group table.
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start) + RVL_VolumeGroupTable_ADDRESS, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = ref_read(sbuf.u8, 1, sizeof(sbuf.u8), f_img);
	if (size != sizeof(sbuf.u8)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	// Find the game partition.
	game_lba = rvth_find_GamePartition_int(&sbuf.pt);
	if (game_lba == 0) {
		// No game partition.
		ret = bankType;
		goto end;
	}

	// Found the game partition.
	// Read the partition header.
	errno = 0;
	pthdr = malloc(sizeof(*pthdr));
	if (!pthdr) {
		// Cannot allocate memory.
		ret = -errno;
		if (ret == 0) {
			ret = -ENOMEM;
		}
		goto end;
	}
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start + game_lba), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = ref_read(pthdr, 1, sizeof(*pthdr), f_img);
	if (size != sizeof(*pthdr)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	// Data offset.
	data_offset = (int64_t)be32_to_cpu(pthdr->data_offset) << 2;
	if (data_offset < (int64_t)sizeof(*pthdr)) {
		// Invalid offset.
		ret = bankType;
		goto end;
	}

	// Read the first LBA of the partition.
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start + game_lba) + data_offset, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = ref_read(sbuf.u8, 1, sizeof(sbuf.u8), f_img);
	if (size != sizeof(sbuf.u8)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	// If the LBA has Wii magic, we have a valid unencrypted disc header.
	if (sbuf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
		// Unencrypted disc header.
		// TODO: Check the hash_verify/disc_noCrypt values.
		// For now, always setting them to 1.
		sbuf.gcn.hash_verify = 1;
		sbuf.gcn.disc_noCrypt = 1;
		memcpy(discHeader, &sbuf.gcn, sizeof(*discHeader));
		ret = RVTH_BankType_Wii_SL;
		isDeleted = true;
		goto end;
	}

	// May need to decrypt.
	// TODO: Get encryption key and decrypt.

end:
	free(pthdr);
	if (ret < 0) {
		errno = -ret;
	}
	if (pIsDeleted) {
		*pIsDeleted = isDeleted;
	}
	return ret;
}
