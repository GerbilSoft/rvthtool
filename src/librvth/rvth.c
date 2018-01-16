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

#include "rvth.h"
#include "byteswap.h"
#include "nhcd_structs.h"
#include "gcn_structs.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// RVT-H main struct.
struct _RvtH {
	FILE *f_img;
	RvtH_BankEntry entries[NHCD_BANK_COUNT];
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
 * @param nhcd_entry NHCD bank entry.
 * @return Timestamp, or -1 if invalid.
 */
static time_t rvth_parse_timestamp(const NHCD_BankEntry *nhcd_entry)
{
	// Date format: "YYYYMMDD"
	// Time format: "HHMMSS"
	unsigned int ymd = 0;
	unsigned int hms = 0;
	unsigned int i;
	struct tm ymdtime;

	// Convert the date to an unsigned integer.
	for (i = 0; i < 8; i++) {
		if (unlikely(!isdigit(nhcd_entry->mdate[i]))) {
			// Invalid digit.
			return -1;
		}
		ymd *= 10;
		ymd += (nhcd_entry->mdate[i] & 0xF);
	}

	// Sanity checks:
	// - Must be higher than 19000101.
	// - Must be lower than 99991231.
	if (unlikely(ymd < 19000101 || ymd > 99991231)) {
		// Invalid date.
		return -1;
	}

	// Convert the time to an unsigned integer.
	for (i = 0; i < 6; i++) {
		if (unlikely(!isdigit(nhcd_entry->mtime[i]))) {
			// Invalid digit.
			return -1;
		}
		hms *= 10;
		hms += (nhcd_entry->mtime[i] & 0xF);
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
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename Filename.
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const char *filename)
{
	NHCD_BankTable_Header header;
	FILE *f_img;
	RvtH *rvth;
	RvtH_BankEntry *rvth_entry;
	int ret;
	unsigned int i;
	uint32_t addr;
	size_t size;

	// Open the disk image.
	// TODO: Unicode filename support on Windows.
	f_img = fopen(filename, "rb");
	if (!f_img) {
		// Could not open the file.
		return NULL;
	}

	// Check the bank table header.
	errno = 0;
	ret = fseeko(f_img, NHCD_BANKTABLE_ADDRESS, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		return NULL;
	}
	errno = 0;
	size = fread(&header, 1, sizeof(header), f_img);
	if (size != sizeof(header)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fclose(f_img);
		errno = err;
		return NULL;
	}

	// Allocate memory for the RvtH object
	rvth = calloc(1, sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		fclose(f_img);
		errno = err;
		return NULL;
	}

	rvth->f_img = f_img;
	rvth_entry = rvth->entries;
	addr = (uint32_t)(NHCD_BANKTABLE_ADDRESS + 512);
	for (i = 0; i < ARRAY_SIZE(rvth->entries); i++, rvth_entry++, addr += 512) {
		NHCD_BankEntry nhcd_entry;
		GCN_DiscHeader discHeader;

		if (i > 0 && (rvth_entry-1)->type == RVTH_BankType_Wii_DL) {
			// Second bank for a dual-layer Wii image.
			rvth_entry->type = RVTH_BankType_Wii_DL_Bank2;
			rvth_entry->timestamp = -1;
			continue;
		}

		errno = 0;
		ret = fseeko(f_img, addr, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			free(rvth);
			fclose(f_img);
			errno = err;
			return NULL;
		}

		errno = 0;
		size = fread(&nhcd_entry, 1, sizeof(nhcd_entry), f_img);
		if (size != sizeof(nhcd_entry)) {
			// Short read.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			free(rvth);
			fclose(f_img);
			errno = err;
			return NULL;
		}

		// Check the type.
		switch (be32_to_cpu(nhcd_entry.type)) {
			default:
				// Unknown bank type...
				rvth_entry->type = RVTH_BankType_Unknown;
				break;
			case 0:
				// "Empty" bank. May have a deleted image.
				rvth_entry->type = RVTH_BankType_Empty;
				break;
			case NHCD_BankType_GCN:
				// GameCube
				rvth_entry->type = RVTH_BankType_GCN;
				break;
			case NHCD_BankType_Wii_SL:
				// Wii (single-layer)
				rvth_entry->type = RVTH_BankType_Wii_SL;
				break;
			case NHCD_BankType_Wii_DL:
				// Wii (dual-layer)
				// TODO: Cannot start in Bank 8.
				rvth_entry->type = RVTH_BankType_Wii_DL;
				break;
		}

		// For valid types, use the listed LBAs if they're non-zero.
		if (rvth_entry->type >= RVTH_BankType_GCN) {
			rvth_entry->lba_start = be32_to_cpu(nhcd_entry.lba_start);
			rvth_entry->lba_len = be32_to_cpu(nhcd_entry.lba_len);
		}

		if (rvth_entry->lba_start == 0 || rvth_entry->lba_len == 0) {
			// Invalid LBAs. Use the default values.
			rvth_entry->lba_start = NHCD_BANK_1_START_LBA + (NHCD_BANK_SIZE_LBA * i);
			rvth_entry->lba_len = NHCD_BANK_SIZE_LBA;
		}

		if (rvth_entry->type == RVTH_BankType_Unknown) {
			// Unknown entry type.
			// Don't bother reading anything else.
			continue;
		}

		// Read the GCN disc header.
		// TODO: For non-deleted banks, verify the magic number?
		errno = 0;
		ret = fseeko(f_img, (int64_t)rvth_entry->lba_start * 512, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			free(rvth);
			fclose(f_img);
			errno = err;
			return NULL;
		}
		size = fread(&discHeader, 1, sizeof(discHeader), f_img);
		if (size != sizeof(discHeader)) {
			// Short read.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			free(rvth);
			fclose(f_img);
			errno = err;
			return NULL;
		}

		// If the entry type is Empty, check for GCN/Wii magic.
		if (rvth_entry->type == RVTH_BankType_Empty) {
			if (discHeader.magic_wii == cpu_to_be32(WII_MAGIC)) {
				// Wii disc image.
				// TODO: Detect SL vs. DL.
				// TODO: Actual disc image size?
				rvth_entry->type = RVTH_BankType_Wii_SL;
				rvth_entry->is_deleted = true;
			} else if (discHeader.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
				// GameCube disc image.
				// TODO: Actual disc image size?
				rvth_entry->type = RVTH_BankType_GCN;
				rvth_entry->is_deleted = true;
			} else {
				// Probably actually empty...
				rvth_entry->timestamp = -1;
				continue;
			}
		}

		// Copy the game ID.
		memcpy(rvth_entry->id6, discHeader.id6, 6);

		// Read the disc title.
		memcpy(rvth_entry->game_title, discHeader.game_title, 64);
		// Remove excess spaces.
		trim_title(rvth_entry->game_title, 64);

		// Parse the timestamp.
		rvth_entry->timestamp = rvth_parse_timestamp(&nhcd_entry);

		// TODO: Check encryption status.
	}

	// RVT-H image loaded.
	return rvth;
}

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *img)
{
	if (!img)
		return;

	if (img->f_img) {
		fclose(img->f_img);
	}
	free(img);
}

/**
 * Get a bank table entry.
 * @param rvth RVT-H disk image.
 * @param bank Bank number. (0-7)
 * @return Bank table entry, or NULL if out of range or empty.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank)
{
	if (!rvth) {
		errno = EINVAL;
		return NULL;
	} else if (bank >= ARRAY_SIZE(rvth->entries)) {
		errno = ERANGE;
		return NULL;
	}

	// Check if the bank is empty.
	if (rvth->entries[bank].type == RVTH_BankType_Empty) {
		return NULL;
	}

	return &rvth->entries[bank];
}

/**
 * Extract a disc image from the RVT-H disk image.
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return 0 on success; negative POSIX error code on error.
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const char *filename, RvtH_Progress_Callback callback)
{
	// TODO: Windows file I/O for sparse file handling.
	FILE *f_extract;
	const RvtH_BankEntry *entry;
	uint8_t *buf;
	unsigned int lba_count, lba_progress;
	int ret;

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= ARRAY_SIZE(rvth->entries)) {
		errno = ERANGE;
		return -ERANGE;
	}

	// Seek to the start of the disc image in the RVT-H.
	entry = &rvth->entries[bank];
	errno = 0;
	ret = fseeko(rvth->f_img, (int64_t)entry->lba_start * NHCD_BLOCK_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		return -errno;
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

	// TODO: Unicode filename support on Windows.
	errno = 0;
	f_extract = fopen(filename, "wb");
	if (!f_extract) {
		// Error opening the file.
		int err = errno;
		free(buf);
		errno = err;
		return -err;
	}


	lba_progress = 0;
	for (lba_count = entry->lba_len; lba_count >= LBA_COUNT_BUF;
	     lba_count -= LBA_COUNT_BUF)
	{
		// TODO: Error handling.
		// TODO: Sparse file handling.
		fread(buf, 1, BUF_SIZE, rvth->f_img);
		fwrite(buf, 1, BUF_SIZE, f_extract);

		if (callback) {
			lba_progress += LBA_COUNT_BUF;
			callback(lba_progress, entry->lba_len);
		}
	}

	// Process any remaining LBAs.
	fread(buf, 1, lba_count * NHCD_BLOCK_SIZE, rvth->f_img);
	fwrite(buf, 1, lba_count * NHCD_BLOCK_SIZE, f_extract);
	if (callback) {
		callback(entry->lba_len, entry->lba_len);
	}

	// Finished extracting the disc image.
	fclose(f_extract);
	free(buf);
	return 0;
}
