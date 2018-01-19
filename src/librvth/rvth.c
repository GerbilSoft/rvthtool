/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.c: RVT-H image handler.                                            *
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

#include "config.librvth.h"

#include "rvth.h"
#include "byteswap.h"
#include "nhcd_structs.h"
#include "gcn_structs.h"

// Disc image readers.
#include "reader_plain.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
# include <windows.h>
# include <io.h>
# include <winioctl.h>
#elif defined(HAVE_FTRUNCATE)
# include <unistd.h>
# include <sys/types.h>
#endif

// RVT-H main struct.
struct _RvtH {
	// Reference-counted FILE*.
	RefFile *f_img;

	// Number of banks.
	// - RVT-H system or disk image: 8
	// - Standalone disc image: 1
	unsigned int bank_count;

	// BankEntry objects.
	RvtH_BankEntry *entries;
};

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
 * Trim a game title.
 * @param title Game title.
 * @param size Size of game title.
 */
static void trim_title(char *title, int size)
{
	size--;
	for (; size >= 0; size--) {
		if (title[size] != ' ')
			break;
		title[size] = 0;
	}
}

/**
 * Parse an RVT-H timestamp.
 * @param nhcd_timestamp Pointer to NHCD timestamp string.
 * @return Timestamp, or -1 if invalid.
 */
static time_t rvth_parse_timestamp(const char *nhcd_timestamp)
{
	// Date format: "YYYYMMDD"
	// Time format: "HHMMSS"
	unsigned int ymd = 0;
	unsigned int hms = 0;
	unsigned int i;
	struct tm ymdtime;

	// Convert the date to an unsigned integer.
	for (i = 0; i < 8; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		ymd *= 10;
		ymd += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be higher than 19000101.
	// - Must be lower than 99991231.
	if (unlikely(ymd < 19000101 || ymd > 99991231)) {
		// Invalid date.
		return -1;
	}

	// Convert the time to an unsigned integer.
	for (i = 8; i < 14; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		hms *= 10;
		hms += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be lower than 235959.
	if (unlikely(hms > 235959)) {
		// Invalid time.
		return -1;
	}

	// Convert to Unix time.
	// NOTE: struct tm has some oddities:
	// - tm_year: year - 1900
	// - tm_mon: 0 == January
	ymdtime.tm_year = (ymd / 10000) - 1900;
	ymdtime.tm_mon  = ((ymd / 100) % 100) - 1;
	ymdtime.tm_mday = ymd % 100;

	ymdtime.tm_hour = (hms / 10000);
	ymdtime.tm_min  = (hms / 100) % 100;
	ymdtime.tm_sec  = hms % 100;

	// tm_wday and tm_yday are output variables.
	ymdtime.tm_wday = 0;
	ymdtime.tm_yday = 0;
	ymdtime.tm_isdst = 0;

	// If conversion fails, this will return -1.
	return timegm(&ymdtime);
}

/**
 * Find the game partition in a Wii disc image.
 * @param reader	[in] Reader*
 * @return Game partition LBA (relative to start of reader), or 0 on error.
 */
uint32_t rvth_find_GamePartition(Reader *reader)
{
	// Assuming this is a valid Wii disc image.
	int64_t addr;
	unsigned int ptcount, i;
	size_t size;

	// Volume group and partition table.
	// NOTE: Only reading the first partition table,
	// which we're assuming is stored directly after
	// the volume group table.
	// FIXME: Handle other cases when using direct device access on Windows.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		struct {
			RVL_VolumeGroupTable vgtbl;
			RVL_PartitionTableEntry ptbl[16];
		};
	} pt;

	// Get the volume group table.
	size = reader_read(reader, &pt, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 1);
	if (size != 1) {
		// Read error.
		return 0;
	}

	// Game partition is always in volume group 0.
	addr = ((int64_t)be32_to_cpu(pt.vgtbl.vg[0].addr) << 2);
	if (addr != (RVL_VolumeGroupTable_ADDRESS + sizeof(pt.vgtbl))) {
		// Partition table offset isn't supported right now.
		return 0;
	}

	ptcount = be32_to_cpu(pt.vgtbl.vg[0].count);
	for (i = 0; i < ptcount; i++) {
		if (be32_to_cpu(pt.ptbl[i].type == 0)) {
			// Found the game partition.
			return (be32_to_cpu(pt.ptbl[i].addr) / (RVTH_BLOCK_SIZE/4));
		}
	}

	// No game partition found...
	return 0;
}

/**
 * Set the crypto_type and sig_type fields in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @param discHeader	[in] GCN_DiscHeader
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_init_BankEntry_crypto(RvtH_BankEntry *entry, const GCN_DiscHeader *discHeader)
{
	uint32_t lba_game;	// LBA of the Game Partition.
	size_t size;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		RVL_Ticket ticket;
	} sector_buf;

	// Certificate issuers.
	static const char issuer_retail[] = "Root-CA00000001-XS00000003";
	static const char issuer_debug[]  = "Root-CA00000002-XS00000006";

	assert(entry->reader != NULL);

	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty bank.
			entry->crypto_type = RVTH_CryptoType_Unknown;
			entry->sig_type = RVTH_SigType_Unknown;
			return RVTH_ERROR_BANK_EMPTY;
		case RVTH_BankType_Unknown:
		default:
			// Unknown bank.
			entry->crypto_type = RVTH_CryptoType_Unknown;
			entry->sig_type = RVTH_SigType_Unknown;
			return RVTH_ERROR_BANK_UNKNOWN;
		case RVTH_BankType_GCN:
			// GameCube image. No encryption.
			entry->crypto_type = RVTH_CryptoType_None;
			// TODO: Technically no signature either...
			entry->sig_type = RVTH_SigType_Unknown;
			return 0;
		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			entry->crypto_type = RVTH_CryptoType_Unknown;
			entry->sig_type = RVTH_SigType_Unknown;
			return RVTH_ERROR_BANK_DL_2;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Wii disc image.
			break;
	}

	// Check for an unencrypted RVT-H image.
	if (discHeader->hash_verify != 0 && discHeader->disc_noCrypt != 0) {
		// Unencrypted.
		// TODO: I haven't seen any images where only one of these
		// is zero or non-zero, but that may show up...
		entry->crypto_type = RVTH_CryptoType_None;
	}

	// Find the game partition.
	// TODO: Error checking.
	lba_game = rvth_find_GamePartition(entry->reader);
	if (lba_game == 0) {
		// No game partition...
		return RVTH_ERROR_NO_GAME_PARTITION;
	}

	// Found the game partition.

	// Read the ticket.
	size = reader_read(entry->reader, sector_buf.u8, lba_game, 1);
	if (size != 1) {
		// Error reading the ticket.
		return -EIO;
	}

	// Check the signature issuer.
	// This indicates debug vs. retail.
	if (!memcmp(sector_buf.ticket.signature_issuer, issuer_retail, sizeof(issuer_retail))) {
		// Retail certificate.
		entry->sig_type = RVTH_SigType_Retail;
	} else if (!memcmp(sector_buf.ticket.signature_issuer, issuer_debug, sizeof(issuer_debug))) {
		// Debug certificate.
		entry->sig_type = RVTH_SigType_Debug;
	} else {
		// Unknown certificate.
		entry->sig_type = RVTH_SigType_Unknown;
	}

	// TODO: Check for invalid and/or fakesigned tickets.

	// If encrypted, set the crypto type.
	// NOTE: Retail + unencrypted is usually an error.
	if (entry->crypto_type != RVTH_CryptoType_None) {
		switch (entry->sig_type) {
			case RVTH_SigType_Retail:
				// Retail may be either Common Key or Korean Key.
				switch (sector_buf.ticket.common_key_index) {
					case 0:
						entry->crypto_type = RVTH_CryptoType_Retail;
						break;
					case 1:
						entry->crypto_type = RVTH_CryptoType_Korean;
						break;
					default:
						entry->crypto_type = RVTH_CryptoType_Unknown;
						break;
				}
				break;

			case RVTH_SigType_Debug:
				// There's only one debug key.
				if (sector_buf.ticket.common_key_index == 0) {
					entry->crypto_type = RVTH_CryptoType_Debug;
				} else {
					entry->crypto_type = RVTH_CryptoType_Unknown;
				}
				break;

			default:
				// Should not happen...
				entry->crypto_type = RVTH_CryptoType_Unknown;
				break;
		}
	}

	return 0;
}

/**
 * Initialize an RVT-H bank entry from an opened HDD image.
 * @param entry			[out] RvtH_BankEntry
 * @param f_img			[in] RefFile*
 * @param type			[in] Bank type. (See RvtH_BankType_e.)
 * @param lba_start		[in] Starting LBA.
 * @param lba_len		[in] Length, in LBAs.
 * @param nhcd_timestamp	[in] Timestamp string pointer from the bank table.
 * @return 0 on success; negative POSIX error code on error.
 */
static int rvth_init_BankEntry(RvtH_BankEntry *entry, RefFile *f_img,
	uint8_t type, uint32_t lba_start, uint32_t lba_len,
	const char *nhcd_timestamp)
{
	int ret;
	size_t size;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
	} sector_buf;

	// Initialize the standard properties.
	memset(entry, 0, sizeof(*entry));
	entry->lba_start = lba_start;
	entry->lba_len = lba_len;
	entry->type = type;
	entry->timestamp = -1;	// Default to "no timestamp".

	assert(entry->type < RVTH_BankType_MAX);
	if (type == RVTH_BankType_Unknown ||
	    type >= RVTH_BankType_MAX)
	{
		// Unknown entry type.
		// Don't bother reading anything else.
		return 0;
	}

	// Read the GCN disc header.
	// TODO: For non-deleted banks, verify the magic number?
	errno = 0;
	ret = ref_seeko(f_img, (int64_t)lba_start * NHCD_BLOCK_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}
	size = ref_read(sector_buf.u8, 1, sizeof(sector_buf.u8), f_img);
	if (size != sizeof(sector_buf.u8)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}

	// If the entry type is Empty, check for GCN/Wii magic.
	if (type == RVTH_BankType_Empty) {
		if (sector_buf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
			// Wii disc image.
			// TODO: Detect SL vs. DL.
			// TODO: Actual disc image size?
			type = RVTH_BankType_Wii_SL;
			entry->is_deleted = true;
		} else if (sector_buf.gcn.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
			// GameCube disc image.
			// TODO: Actual disc image size?
			type = RVTH_BankType_GCN;
			entry->is_deleted = true;
		} else {
			// Check for GameCube NDDEMO.
			if (!memcmp(&sector_buf.gcn, nddemo_header, sizeof(nddemo_header))) {
				// NDDEMO header found.
				type = RVTH_BankType_GCN;
				entry->is_deleted = true;
			} else {
				// Probably actually empty...
			}
		}
		entry->type = type;
	}

	if (lba_len == 0) {
		// Use the default LBA length based on bank type.
		switch (type) {
			default:
			case RVTH_BankType_Empty:
			case RVTH_BankType_Unknown:
			case RVTH_BankType_Wii_SL:
			case RVTH_BankType_Wii_DL_Bank2:
				// Full bank.
				lba_len = NHCD_BANK_WII_SL_SIZE_RVTR_LBA;
				break;

			case RVTH_BankType_Wii_DL:
				// Dual-layer bank.
				lba_len = NHCD_BANK_WII_DL_SIZE_RVTR_LBA;
				break;

			case RVTH_BankType_GCN:
				// GameCube disc image.
				lba_len = NHCD_BANK_GCN_DL_SIZE_NR_LBA;
				break;
		}
		entry->lba_len = lba_len;
	}

	if (type == RVTH_BankType_Empty) {
		// We're done here.
		return 0;
	}

	// Initialize the disc image reader.
	// NOTE: Always the plain reader for RVT-H HDD images.
	entry->reader = reader_plain_open(f_img, lba_start, lba_len);

	// Copy the game ID.
	memcpy(entry->id6, sector_buf.gcn.id6, 6);

	// Read the disc title.
	memcpy(entry->game_title, sector_buf.gcn.game_title, 64);
	// Remove excess spaces.
	trim_title(entry->game_title, 64);

	// Parse the timestamp.
	if (nhcd_timestamp) {
		entry->timestamp = rvth_parse_timestamp(nhcd_timestamp);
	}

	// Initialize the encryption status.
	rvth_init_BankEntry_crypto(entry, &sector_buf.gcn);

	// We're done here.
	return 0;
}

/**
 * Get a string description of an error number.
 * - Negative: POSIX error. (strerror())
 * - Positive; RVT-H error. (RvtH_Errors)
 * @param err Error number.
 * @return String description.
 */
const char *rvth_error(int err)
{
	static const char *const errtbl[] = {
		// tr: RVTH_ERROR_SUCCESS
		"Success",
		// tr: RVTH_ERROR_NHCD_TABLE_MAGIC
		"Bank table magic is incorrect",
		// tr: RVTH_ERROR_BANK_UNKNOWN
		"Bank status is unknown",
		// tr: RVTH_ERROR_BANK_EMPTY
		"Bank is empty",
		// tr: RVTH_ERROR_BANK_DL_2
		"Bank is second bank of a dual-layer image",
		// tr: RVTH_ERROR_NOT_A_DEVICE
		"Operation can only be performed on a device, not an image file",
		// tr: RVTH_ERROR_BANK_IS_DELETED
		"Bank is deleted",
		// tr: RVTH_ERROR_BANK_NOT_DELETED
		"Bank is not deleted",
		// tr: RVTH_ERROR_NOT_HDD_IMAGE
		"RVT-H object is not an HDD image"
		// tr: RVTH_ERROR_NO_GAME_PARTITION
		"Wii game partition not found"
	};

	if (err < 0) {
		return strerror(-err);
	} else if (err >= ARRAY_SIZE(errtbl)) {
		return "(unknown)";
	}

	return errtbl[err];
}

/**
 * Open a Wii or GameCube disc image.
 * @param f_img	[in] RefFile*
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
static RvtH *rvth_open_gcm(RefFile *f_img, int *pErr)
{
	RvtH *rvth;
	RvtH_BankEntry *entry;
	int ret;
	size_t size;

	int64_t len;
	uint8_t type;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
	} sector_buf;

	// TODO: Detect CISO and WBFS.

	// Get the file length.
	// FIXME: This is obtained in rvth_open().
	// Pass it as a parameter?
	ret = ref_seeko(f_img, 0, SEEK_END);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}
	len = ref_tello(f_img);

	// Rewind back to the beginning of the file.
	ret = ref_seeko(f_img, 0, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}

	// Check the disc header.
	errno = 0;
	size = ref_read(sector_buf.u8, 1, sizeof(sector_buf.u8), f_img);
	if (size != sizeof(sector_buf.u8)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Check for GameCube and/or Wii magic numbers.
	if (sector_buf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
		// Wii disc image.
		if (len > LBA_TO_BYTES(NHCD_BANK_WII_SL_SIZE_RVTR_LBA)) {
			// Dual-layer Wii disc image.
			// TODO: Check for retail vs. RVT-R?
			type = RVTH_BankType_Wii_DL;
		} else {
			// Single-layer Wii disc image.
			type = RVTH_BankType_Wii_SL;
		}
	} else if (sector_buf.gcn.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
		// GameCube disc image.
		type = RVTH_BankType_GCN;
	} else {
		// Check for GameCube NDDEMO.
		if (!memcmp(&sector_buf.gcn, nddemo_header, sizeof(nddemo_header))) {
			// NDDEMO header found.
			type = RVTH_BankType_GCN;
		} else {
			// Not supported.
			ref_close(f_img);
			errno = EIO;
			if (pErr) {
				*pErr = -EIO;
			}
			return NULL;
		}
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Allocate memory for a single RvtH_BankEntry object.
	rvth->bank_count = 1;
	rvth->entries = calloc(1, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	rvth->f_img = f_img;
	entry = rvth->entries;
	entry->lba_start = 0;
	entry->lba_len = BYTES_TO_LBA(len);
	entry->type = type;
	entry->is_deleted = false;

	// Initialize the disc image reader.
	// TODO: Handle CISO and WBFS for standalone disc images.
	entry->reader = reader_plain_open(f_img, entry->lba_start, entry->lba_len);

	// Copy the game ID.
	memcpy(entry->id6, sector_buf.gcn.id6, 6);

	// Read the disc title.
	memcpy(entry->game_title, sector_buf.gcn.game_title, 64);
	// Remove excess spaces.
	trim_title(entry->game_title, 64);

	// Timestamp.
	// TODO: Get the timestamp from the file.
	entry->timestamp = -1;

	// Initialize the encryption status.
	rvth_init_BankEntry_crypto(entry, &sector_buf.gcn);

	// Disc image loaded.
	return rvth;
}

/**
 * Open an RVT-H disk image.
 * @param f_img	[in] RefFile*
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
static RvtH *rvth_open_hdd(RefFile *f_img, int *pErr)
{
	NHCD_BankTable_Header nhcd_header;
	RvtH *rvth;
	RvtH_BankEntry *rvth_entry;
	int ret;
	unsigned int i;
	int64_t addr;
	size_t size;

	// Check the bank table header.
	errno = 0;
	ret = ref_seeko(f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}
	errno = 0;
	size = ref_read(&nhcd_header, 1, sizeof(nhcd_header), f_img);
	if (size != sizeof(nhcd_header)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Check the magic number.
	if (nhcd_header.magic != be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Incorrect magic number.
		ref_close(f_img);
		if (pErr) {
			*pErr = RVTH_ERROR_NHCD_TABLE_MAGIC;
		}
		errno = EIO;
		return NULL;
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Allocate memory for the 8 RvtH_BankEntry objects.
	rvth->bank_count = 8;
	rvth->entries = calloc(8, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	rvth->f_img = f_img;
	rvth_entry = rvth->entries;
	addr = (uint32_t)(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA) + NHCD_BLOCK_SIZE);
	for (i = 0; i < rvth->bank_count; i++, rvth_entry++, addr += 512) {
		NHCD_BankEntry nhcd_entry;
		uint32_t lba_start = 0, lba_len = 0;
		uint8_t type = RVTH_BankType_Unknown;

		if (i > 0 && (rvth_entry-1)->type == RVTH_BankType_Wii_DL) {
			// Second bank for a dual-layer Wii image.
			rvth_entry->type = RVTH_BankType_Wii_DL_Bank2;
			rvth_entry->timestamp = -1;
			continue;
		}

		errno = 0;
		ret = ref_seeko(f_img, addr, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		errno = 0;
		size = ref_read(&nhcd_entry, 1, sizeof(nhcd_entry), f_img);
		if (size != sizeof(nhcd_entry)) {
			// Short read.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		// Check the type.
		switch (be32_to_cpu(nhcd_entry.type)) {
			default:
				// Unknown bank type...
				type = RVTH_BankType_Unknown;
				break;
			case NHCD_BankType_Empty:
				// "Empty" bank. May have a deleted image.
				type = RVTH_BankType_Empty;
				break;
			case NHCD_BankType_GCN:
				// GameCube
				type = RVTH_BankType_GCN;
				break;
			case NHCD_BankType_Wii_SL:
				// Wii (single-layer)
				type = RVTH_BankType_Wii_SL;
				break;
			case NHCD_BankType_Wii_DL:
				// Wii (dual-layer)
				// TODO: Cannot start in Bank 8.
				type = RVTH_BankType_Wii_DL;
				break;
		}

		// For valid types, use the listed LBAs if they're non-zero.
		if (type >= RVTH_BankType_GCN) {
			lba_start = be32_to_cpu(nhcd_entry.lba_start);
			lba_len = be32_to_cpu(nhcd_entry.lba_len);
		}

		if (lba_start == 0 || lba_len == 0) {
			// Invalid LBAs. Use the default starting offset.
			// Bank size will be determined by rvth_init_BankEntry().
			lba_start = NHCD_BANK_1_START_LBA + (NHCD_BANK_SIZE_LBA * i);
			lba_len = 0;
		}

		// Initialize the bank entry.
		rvth_init_BankEntry(rvth_entry, f_img, type,
			lba_start, lba_len, nhcd_entry.timestamp);
	}

	// RVT-H image loaded.
	return rvth;
}

/**
 * Open an RVT-H disk image, GameCube disc image, or Wii disc image.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
RvtH *rvth_open(const TCHAR *filename, int *pErr)
{
	RefFile *f_img;
	int ret;
	int64_t len;

	// Open the disk image.
	f_img = ref_open(filename);
	if (!f_img) {
		// Could not open the file.
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}

	// Determine if this is an HDD image or a disc image.
	ret = ref_seeko(f_img, 0, SEEK_END);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}
	len = ref_tello(f_img);
	if (len == 0) {
		// File is empty.
		errno = EIO;
		if (pErr) {
			*pErr = -EIO;
		}
		return NULL;
	} else if (len <= 2*LBA_TO_BYTES(NHCD_BANK_SIZE_LBA)) {
		// Two banks or less.
		// This is most likely a standalone disc image.
		return rvth_open_gcm(f_img, pErr);
	} else {
		// More than two banks.
		// This is most likely an RVT-H HDD image.
		return rvth_open_hdd(f_img, pErr);
	}
}

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth)
{
	unsigned int i;

	if (!rvth)
		return;

	// Close all bank entry files.
	// RefFile has a reference count, so we have to clear the count.
	for (i = 0; i < rvth->bank_count; i++) {
		if (rvth->entries[i].reader) {
			reader_close(rvth->entries[i].reader);
		}
	}

	// Free the bank entries array.
	free(rvth->entries);

	// Clear the main reference.
	if (rvth->f_img) {
		ref_close(rvth->f_img);
	}

	free(rvth);
}

/**
 * Get the number of banks in an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 * @return Number of banks.
 */
unsigned int rvth_get_BankCount(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return 0;
	}

	return rvth->bank_count;
}

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range or empty.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr)
{
	if (!rvth) {
		errno = EINVAL;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	} else if (bank >= rvth->bank_count) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	}

	// Check if the bank is empty.
	if (rvth->entries[bank].type == RVTH_BankType_Empty) {
		if (pErr) {
			*pErr = RVTH_ERROR_BANK_EMPTY;
		}
		return NULL;
	}

	return &rvth->entries[bank];
}

/**
 * Check if a block is empty.
 * @param block Block.
 * @param size Block size. (Must be a multiple of 64 bytes.)
 * @return True if the block is all zeroes; false if not.
 */
static bool is_block_empty(const uint8_t *block, unsigned int size)
{
	// Process the block using 64-bit pointers.
	const uint64_t *block64 = (const uint64_t*)block;
	unsigned int i;
	assert(size % 64 == 0);
	for (i = size/8/8; i > 0; i--, block64 += 8) {
		uint64_t x = block64[0];
		x |= block64[1];
		x |= block64[2];
		x |= block64[3];
		x |= block64[4];
		x |= block64[5];
		x |= block64[6];
		x |= block64[7];
		if (x != 0) {
			// Non-zero block.
			return false;
		}
	}

	// Block is all zeroes.
	return true;
}

/**
 * Extract a disc image from the RVT-H disk image.
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return 0 on success; negative POSIX error code or positive RVT-H error code on error.
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback)
{
	FILE *f_extract;
	const RvtH_BankEntry *entry;
	uint8_t *buf;
	unsigned int lba_count, lba_progress;
	unsigned int lba_nonsparse;	// Last LBA written that wasn't sparse.
	unsigned int sprs;		// Sparse counter.

#ifdef _WIN32
	wchar_t root_dir[4];		// Root directory.
	wchar_t *p_root_dir;		// Pointer to root_dir, or NULL if relative.
	DWORD dwFileSystemFlags;	// Flags from GetVolumeInformation().
	BOOL bRet;
#else /* !_WIN32 */
	int ret;
#endif /* _WIN32 */

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->bank_count) {
		errno = ERANGE;
		return -ERANGE;
	}

	// Check if the bank can be extracted.
	entry = &rvth->entries[bank];
	switch (entry->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be extracted.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	// Process 1 MB at a time.
	#define BUF_SIZE 1048576
	#define LBA_COUNT_BUF (BUF_SIZE / NHCD_BLOCK_SIZE)
	errno = 0;
	buf = malloc(BUF_SIZE);
	if (!buf) {
		// Error allocating memory.
		return -errno;
	}

	errno = 0;
	f_extract = _tfopen(filename, _T("wb"));
	if (!f_extract) {
		// Error opening the file.
		int err = errno;
		free(buf);
		errno = err;
		return -err;
	}

#if defined(_WIN32)
	// Check if the file system supports sparse files.
	// TODO: Handle mount points?
	if (_istalpha(filename[0]) && filename[1] == _T(':') &&
	    (filename[2] == _T('\\') || filename[2] == _T('/')))
	{
		// Absolute pathname.
		root_dir[0] = filename[0];
		root_dir[1] = L':';
		root_dir[2] = L'\\';
		root_dir[3] = 0;
		p_root_dir = root_dir;
	}
	else
	{
		// Relative pathname.
		p_root_dir = NULL;
	}

	bRet = GetVolumeInformation(p_root_dir, NULL, 0, NULL, NULL,
		&dwFileSystemFlags, NULL, 0);
	if (bRet != 0 && (dwFileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)) {
		// File system supports sparse files.
		// Mark the file as sparse.
		HANDLE h_extract = (HANDLE)_get_osfhandle(_fileno(f_extract));
		if (h_extract != NULL && h_extract != INVALID_HANDLE_VALUE) {
			DWORD bytesReturned;
			FILE_SET_SPARSE_BUFFER fssb;
			fssb.SetSparse = TRUE;

			// TODO: Use SetEndOfFile() if this succeeds?
			// Note that SetEndOfFile() isn't guaranteed to fill the
			// resulting file with zero bytes...
			DeviceIoControl(h_extract, FSCTL_SET_SPARSE,
				&fssb, sizeof(fssb),
				NULL, 0, &bytesReturned, NULL);
		}
	}
#elif defined(HAVE_FTRUNCATE)
	// Set the file size.
	// NOTE: If the underlying file system doesn't support sparse files,
	// this may take a long time. (TODO: Check this.)
	ret = ftruncate(fileno(f_extract), (int64_t)entry->lba_len * RVTH_BLOCK_SIZE);
	if (ret != 0) {
		// Error setting the file size.
		// Allow all errors except for EINVAL or EFBIG,
		// since those mean the file system doesn't support
		// large files.
		int err = errno;
		if (err == EINVAL || err == EFBIG) {
			// File is too big.
			// TODO: Delete the file?
			fclose(f_extract);
			free(buf);
			errno = err;
			return -err;
		}
	}
#endif /* HAVE_FTRUNCATE */

	lba_progress = 0;
	lba_nonsparse = 0;
	for (lba_count = entry->lba_len; lba_count >= LBA_COUNT_BUF;
	     lba_count -= LBA_COUNT_BUF)
	{
		// TODO: Error handling.
		reader_read(entry->reader, buf, lba_progress, LBA_COUNT_BUF);

		// Check for empty 4 KB blocks.
		for (sprs = 0; sprs < BUF_SIZE; sprs += 4096) {
			if (is_block_empty(&buf[sprs], 4096)) {
				// 4 KB block is empty.
				fseeko(f_extract, 4096, SEEK_CUR);
			} else {
				// 4 KB block is not empty.
				fwrite(&buf[sprs], 1, 4096, f_extract);
				lba_nonsparse = lba_progress + (sprs / 512) + 7;
			}
		}

		if (callback) {
			lba_progress += LBA_COUNT_BUF;
			callback(lba_progress, entry->lba_len);
		}
	}

	// Process any remaining LBAs.
	if (lba_count > 0) {
		const unsigned int sz_left = lba_count * RVTH_BLOCK_SIZE;
		reader_read(entry->reader, buf, lba_progress, lba_count);

		// Check for empty 512-byte blocks.
		for (sprs = 0; sprs < sz_left; sprs += 512) {
			if (is_block_empty(&buf[sprs], 512)) {
				// 512-byte block is empty.
				fseeko(f_extract, 512, SEEK_CUR);
			} else {
				// 512-byte block is not empty.
				fwrite(&buf[sprs], 1, 512, f_extract);
				lba_nonsparse = lba_progress + (sprs / 512) + 7;
			}
		}

		if (callback) {
			callback(entry->lba_len, entry->lba_len);
		}
	}

	// lba_nonsparse should be equal to entry->lba_len - 1.
	if (lba_nonsparse != entry->lba_len - 1) {
		// Last LBA was sparse.
		// We'll need to write an actual zero block.
		// TODO: Maybe not needed if ftruncate() succeeded?
		// TODO: Check for errors.
		int64_t last_lba_pos = (int64_t)(entry->lba_len - 1) * RVTH_BLOCK_SIZE;
		memset(buf, 0, 512);
		fseeko(f_extract, last_lba_pos, SEEK_SET);
		fwrite(buf, 1, 512, f_extract);
	}

	// Finished extracting the disc image.
	fflush(f_extract);
	fclose(f_extract);
	free(buf);
	return 0;
}

/**
 * Make an RVT-H object writable.
 * @param rvth	[in] RVT-H disk image.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_make_writable(RvtH *rvth)
{
	if (ref_is_writable(rvth->f_img)) {
		// RVT-H is already writable.
		return 0;
	}

	// TODO: Allow making a disc image file writable.
	// (Single bank)

	// Make sure this is a device file.
	if (!ref_is_device(rvth->f_img)) {
		// This is not a device file.
		// Cannot make it writable.
		return RVTH_ERROR_NOT_A_DEVICE;
	}

	// Make this writable.
	return ref_make_writable(rvth->f_img);
}

/** Writing functions. **/

/**
 * Write a bank table entry to disk.
 * @param rvth	[in] RvtH device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_write_BankEntry(RvtH *rvth, unsigned int bank)
{
	int ret;
	size_t size;
	NHCD_BankEntry nhcd_entry;
	RvtH_BankEntry *rvth_entry;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (rvth->bank_count < 2) {
		// Single-bank image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Create a new NHCD bank entry.
	memset(&nhcd_entry, 0, sizeof(nhcd_entry));

	// Check the bank type.
	rvth_entry = &rvth->entries[bank];
	switch (rvth_entry->type) {
		// These bank entries can be written.
		case RVTH_BankType_Empty:
			nhcd_entry.type = cpu_to_be32(NHCD_BankType_Empty);
			break;
		case RVTH_BankType_GCN:
			nhcd_entry.type = cpu_to_be32(NHCD_BankType_GCN);
			break;
		case RVTH_BankType_Wii_SL:
			nhcd_entry.type = cpu_to_be32(NHCD_BankType_Wii_SL);
			break;
		case RVTH_BankType_Wii_DL:
			nhcd_entry.type = cpu_to_be32(NHCD_BankType_Wii_DL);
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	if (rvth_entry->type != RVTH_BankType_Empty &&
	    !rvth_entry->is_deleted)
	{
		// Non-empty/non-deleted bank.
		time_t now;
		struct tm tm_now;

		// Timestamp buffer.
		// We can't snprintf() directly to nhcd_entry.timestamp because
		// gcc will complain about buffer overflows, and will crash at
		// runtime on Gentoo Hardened.
		char tsbuf[16];

		// ASCII zero bytes.
		memset(nhcd_entry.all_zero, '0', sizeof(nhcd_entry.all_zero));

		// Timestamp.
		// TODO: Use localtime_r().
		now = time(NULL);
		tm_now = *localtime(&now);
		snprintf(tsbuf, sizeof(tsbuf), "%04d%02d%02d%02d%02d%02d",
			tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday,
			tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
		memcpy(nhcd_entry.timestamp, tsbuf, sizeof(nhcd_entry.timestamp));

		// LBA start and length.
		nhcd_entry.lba_start = cpu_to_be32(rvth_entry->lba_start);
		nhcd_entry.lba_len = cpu_to_be32(rvth_entry->lba_len);
	}

	// Write the bank entry.
	ret = ref_seeko(rvth->f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA + bank+1), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}
	size = ref_write(&nhcd_entry, 1, sizeof(nhcd_entry), rvth->f_img);
	if (size != sizeof(nhcd_entry)) {
		// Write error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}

	// Bank entry written successfully.
	return 0;
}

/**
 * Delete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_delete(RvtH *rvth, unsigned int bank)
{
	RvtH_BankEntry *rvth_entry;
	int ret;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (rvth->bank_count < 2) {
		// Single-bank image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Is the bank deleted?
	rvth_entry = &rvth->entries[bank];
	if (rvth_entry->is_deleted) {
		// Bank is already deleted.
		return RVTH_ERROR_BANK_IS_DELETED;
	}

	// Check the bank type.
	switch (rvth_entry->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be deleted.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	// Delete the bank and write the entry.
	rvth_entry->is_deleted = true;
	ret = rvth_write_BankEntry(rvth, bank);
	if (ret != 0) {
		// Error deleting the bank...
		rvth_entry->is_deleted = false;
	}
	return ret;
}

/**
 * Undelete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_undelete(RvtH *rvth, unsigned int bank)
{
	RvtH_BankEntry *rvth_entry;
	int ret;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (rvth->bank_count < 2) {
		// Single-bank image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Is the bank deleted?
	rvth_entry = &rvth->entries[bank];
	if (!rvth_entry->is_deleted) {
		// Bank is not deleted.
		return RVTH_ERROR_BANK_NOT_DELETED;
	}

	// Check the bank type.
	switch (rvth_entry->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be undeleted.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	// Delete the bank and write the entry.
	rvth_entry->is_deleted = false;
	ret = rvth_write_BankEntry(rvth, bank);
	if (ret != 0) {
		// Error undeleting the bank...
		rvth_entry->is_deleted = true;
	}
	return ret;
}
