/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * disc_header.cpp: Read a GCN/Wii disc header and determine its type.     *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "disc_header.hpp"
#include "nhcd_structs.h"
#include "rvth_enums.h"
#include "rvth.hpp"	// for RvtH::isBlockEmpty()

#include "RefFile.hpp"

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/aesw.h"
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/wii_structs.h"

// C includes.
#include <stdlib.h>

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>

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
	uint8_t u8[LBA_SIZE];
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
 *
 * Internal version for use with raw disc images, i.e.
 * before the RvtH_BankEntry has been created.
 *
 * @param pt	[in] Partition table.
 * @return Game partition LBA (relative to start of disc), or 0 on error.
 */
static uint32_t rvth_find_GamePartition_int(const ptbl_t *pt)
{
	// Game partition is always in volume group 0.
	// TODO: Use uint32_t arithmetic instead of int64_t?
	int64_t addr = ((int64_t)be32_to_cpu(pt->vgtbl.vg[0].addr) << 2);
	if (addr != (RVL_VolumeGroupTable_ADDRESS + sizeof(pt->vgtbl))) {
		// Partition table offset isn't supported right now.
		return 0;
	}

	uint32_t ptcount = be32_to_cpu(pt->vgtbl.vg[0].count);
	if (ptcount > ARRAY_SIZE(pt->ptbl)) {
		// Can't check this many partitions.
		// Reduce it to the maximum we can check.
		ptcount = ARRAY_SIZE(pt->ptbl);
	}
	for (unsigned int i = 0; i < ptcount; i++) {
		if (pt->ptbl[i].type == cpu_to_be32(0)) {
			// Found the game partition.
			return (be32_to_cpu(pt->ptbl[i].addr) / (LBA_SIZE/4));
		}
	}

	// No game partition found...
	return 0;
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
int rvth_disc_header_get(RefFile *f_img, uint32_t lba_start,
	GCN_DiscHeader *discHeader, bool *pIsDeleted)
{
	int ret = 0;	// errno setting
	size_t size;
	bool isDeleted = false;

	// Sector buffer.
	union {
		uint8_t u8[LBA_SIZE];
		GCN_DiscHeader gcn;
		ptbl_t pt;
	} sbuf;
	uint32_t game_lba;
	int bankType;

	// Wii partition header.
	RVL_PartitionHeader *pthdr = NULL;
	int64_t data_offset;
	const uint8_t *common_key;
	uint8_t title_key[16];
	uint8_t iv[16];
	AesCtx *aesw = NULL;

	assert(f_img != NULL);
	assert(discHeader != NULL);
	if (!f_img || !discHeader) {
		errno = EINVAL;
		return -EINVAL;
	}

	// Clear the returned discHeader struct initially.
	memset(discHeader, 0, sizeof(*discHeader));

	// Read the disc header.
	ret = f_img->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		goto end;
	}
	errno = 0;
	size = f_img->read(sbuf.u8, 1, sizeof(sbuf.u8));
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
		if (RvtH::isBlockEmpty(sbuf.u8, sizeof(sbuf.u8))) {
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
	ret = f_img->seeko(LBA_TO_BYTES(lba_start) + RVL_VolumeGroupTable_ADDRESS, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = f_img->read(sbuf.u8, 1, sizeof(sbuf.u8));
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
	pthdr = (RVL_PartitionHeader*)malloc(sizeof(*pthdr));
	if (!pthdr) {
		// Cannot allocate memory.
		ret = -errno;
		if (ret == 0) {
			ret = -ENOMEM;
		}
		goto end;
	}
	ret = f_img->seeko(LBA_TO_BYTES(lba_start + game_lba), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = f_img->read(pthdr, 1, sizeof(*pthdr));
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
	ret = f_img->seeko(LBA_TO_BYTES(lba_start + game_lba) + data_offset, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	errno = 0;
	size = f_img->read(sbuf.u8, 1, sizeof(sbuf.u8));
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

	// Try decrypting the partition header.
	switch (cert_get_issuer_from_name(pthdr->ticket.issuer)) {
		case RVL_CERT_ISSUER_PPKI_TICKET:
			// Retail certificate.
			switch (pthdr->ticket.common_key_index) {
				case 0:
				default:
					common_key = RVL_AES_Keys[RVL_KEY_RETAIL];
					break;
				case 1:
					common_key = RVL_AES_Keys[RVL_KEY_KOREAN];
					break;
				case 2:
					common_key = RVL_AES_Keys[vWii_KEY_RETAIL];
					break;
			}
			break;
		case RVL_CERT_ISSUER_DPKI_TICKET:
			// Debug certificate.
			switch (pthdr->ticket.common_key_index) {
				case 0:
				default:
					common_key = RVL_AES_Keys[RVL_KEY_DEBUG];
					break;
				case 1:
					common_key = RVL_AES_Keys[RVL_KEY_KOREAN_DEBUG];
					break;
				case 2:
					common_key = RVL_AES_Keys[vWii_KEY_DEBUG];
					break;
			}
			break;
		default:
			// Unknown issuer, or not valid for ticket.
			ret = bankType;
			goto end;
	}

	// Initialize the AES context.
	errno = 0;
	aesw = aesw_new();
	if (!aesw) {
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	// Decrypt the title key.
	memcpy(title_key, pthdr->ticket.enc_title_key, sizeof(title_key));
	memcpy(iv, &pthdr->ticket.title_id, 8);
	memset(&iv[8], 0, 8);
	aesw_set_key(aesw, common_key, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_decrypt(aesw, title_key, sizeof(title_key));

	// Read the next LBA. This contains encrypted hashes,
	// including the IV for the user data.
	errno = 0;
	size = f_img->read(sbuf.u8, 1, sizeof(sbuf.u8));
	if (size != sizeof(sbuf.u8)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}
	memcpy(iv, &sbuf.u8[0x3D0-0x200], sizeof(iv));

	// Read the first LBA of user data.
	errno = 0;
	size = f_img->read(sbuf.u8, 1, sizeof(sbuf.u8));
	if (size != sizeof(sbuf.u8)) {
		// Read error.
		ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		goto end;
	}

	// Decrypt the user data.
	// NOTE: Only decrypting 128 bytes.
	aesw_set_key(aesw, title_key, sizeof(title_key));
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_decrypt(aesw, sbuf.u8, 128);

	// If the LBA has Wii magic, we have a valid encrypted disc header.
	if (sbuf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
		// Encrypted disc header.
		// TODO: Check the hash_verify/disc_noCrypt values.
		// For now, always setting them to 0.
		sbuf.gcn.hash_verify = 0;
		sbuf.gcn.disc_noCrypt = 0;
		memcpy(discHeader, &sbuf.gcn, sizeof(*discHeader));
		isDeleted = true;
		ret = RVTH_BankType_Wii_SL;
		goto end;
	}

end:
	if (aesw) {
		aesw_free(aesw);
	}
	free(pthdr);
	if (ret < 0) {
		errno = -ret;
	}
	if (pIsDeleted) {
		*pIsDeleted = isDeleted;
	}
	return ret;
}
