/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_extract.c: RVT-H extract, import, and post-processing functions.   *
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
#include "rvth_p.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// Disc image readers.
#include "reader_plain.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to a writable standalone disc image.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_gcm(RvtH *rvth_dest, const RvtH *rvth_src, unsigned int bank_src, RvtH_Progress_Callback callback)
{
	const RvtH_BankEntry *entry_src;
	uint8_t *buf;
	uint32_t lba_copy_len;	// Total number of LBAs to copy. (entry_src->lba_len)
	uint32_t lba_count;
	uint32_t lba_buf_max;	// Highest LBA that can be written using the buffer.
	uint32_t lba_nonsparse;	// Last LBA written that wasn't sparse.
	unsigned int sprs;		// Sparse counter.
	int ret;

	// Destination disc image.
	RvtH_BankEntry *entry_dest;

	if (!rvth_dest || !rvth_src) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank_src >= rvth_src->bank_count) {
		errno = ERANGE;
		return -ERANGE;
	} else if (rvth_dest->is_hdd || rvth_dest->bank_count != 1) {
		// Destination is not a standalone disc image.
		// Copying to HDDs will be handled differently.
		errno = EIO;
		return RVTH_ERROR_IS_HDD_IMAGE;
	}

	// Check if the source bank can be extracted.
	entry_src = &rvth_src->entries[bank_src];
	switch (entry_src->type) {
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
	#define LBA_COUNT_BUF BYTES_TO_LBA(BUF_SIZE)
	errno = 0;
	buf = malloc(BUF_SIZE);
	if (!buf) {
		// Error allocating memory.
		return -errno;
	}

	// Make this a sparse file.
	entry_dest = &rvth_dest->entries[0];
	ret = ref_make_sparse(rvth_dest->f_img, LBA_TO_BYTES(entry_dest->lba_len));
	if (ret != 0) {
		// Error managing the sparse file.
		// TODO: Delete the file?
		free(buf);
		return ret;
	}

	// Copy the bank table information.
	memcpy(entry_dest->id6, entry_src->id6, sizeof(entry_dest->id6));
	memcpy(entry_dest->game_title, entry_src->game_title, sizeof(entry_dest->game_title));
	entry_dest->type	= entry_src->type;
	entry_dest->disc_number	= entry_src->disc_number;
	entry_dest->revision	= entry_src->revision;
	entry_dest->region_code	= entry_src->region_code;
	entry_dest->is_deleted	= false;

	// Timestamp.
	if (entry_src->timestamp >= 0) {
		entry_dest->timestamp = entry_src->timestamp;
	} else {
		entry_dest->timestamp = time(NULL);
	}

	// Number of LBAs to copy.
	lba_copy_len = entry_src->lba_len;

	// TODO: Optimize seeking? (reader_write() seeks every time.)
	lba_buf_max = entry_dest->lba_len & ~(LBA_COUNT_BUF-1);
	lba_nonsparse = 0;
	for (lba_count = 0; lba_count < lba_buf_max; lba_count += LBA_COUNT_BUF) {
		if (callback) {
			callback(lba_count, lba_copy_len);
		}

		// TODO: Error handling.
		reader_read(entry_src->reader, buf, lba_count, LBA_COUNT_BUF);

		// Check for empty 4 KB blocks.
		for (sprs = 0; sprs < BUF_SIZE; sprs += 4096) {
			if (!rvth_is_block_empty(&buf[sprs], 4096)) {
				// 4 KB block is not empty.
				lba_nonsparse = lba_count + (sprs / 512);
				ret = reader_write(entry_dest->reader, &buf[sprs], lba_nonsparse, 8);
				lba_nonsparse += 7;
			}
		}
	}

	// Process any remaining LBAs.
	if (lba_count < lba_copy_len) {
		const unsigned int lba_left = lba_copy_len - lba_count;
		const unsigned int sz_left = (unsigned int)BYTES_TO_LBA(lba_left);

		if (callback) {
			callback(lba_count, lba_copy_len);
		}
		reader_read(entry_src->reader, buf, lba_count, lba_left);

		// Check for empty 512-byte blocks.
		for (sprs = 0; sprs < sz_left; sprs += 512) {
			if (!rvth_is_block_empty(&buf[sprs], 512)) {
				// 512-byte block is not empty.
				lba_nonsparse = lba_count + (sprs / 512);
				reader_write(entry_dest->reader, &buf[sprs], lba_nonsparse, 1);
			}
		}
	}

	if (callback) {
		callback(lba_copy_len, lba_copy_len);
	}

	// lba_nonsparse should be equal to lba_copy_len-1.
	if (lba_nonsparse != lba_copy_len-1) {
		// Last LBA was sparse.
		// We'll need to write an actual zero block.
		// TODO: Maybe not needed if ftruncate() succeeded?
		// TODO: Check for errors.
		memset(buf, 0, 512);
		reader_write(entry_dest->reader, buf, lba_copy_len-1, 1);
	}

	// Finished extracting the disc image.
	reader_flush(entry_dest->reader);
	free(buf);
	return 0;
}

/**
 * Extract a disc image from the RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_create_gcm() and rvth_copy_to_gcm().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback)
{
	RvtH *rvth_dest;
	const RvtH_BankEntry *entry;
	int ret;

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Create a standalone disc image.
	entry = &rvth->entries[bank];
	ret = 0;
	rvth_dest = rvth_create_gcm(filename, entry->lba_len, &ret);
	if (!rvth_dest) {
		// Error creating the standalone disc image.
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	}

	// Copy the bank from the source image to the destination GCM.
	ret = rvth_copy_to_gcm(rvth_dest, rvth, bank, callback);
	rvth_close(rvth_dest);
	return ret;
}

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to an RVT-H system.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param bank_dest	[out] Destination bank number. (0-7)
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_hdd(RvtH *rvth_dest, unsigned int bank_dest, const RvtH *rvth_src,
	unsigned int bank_src, RvtH_Progress_Callback callback)
{
	const RvtH_BankEntry *entry_src;
	uint32_t lba_copy_len;	// Total number of LBAs to copy. (entry_src->lba_len)
	uint32_t lba_count;
	uint8_t *buf;
	int ret;

	// Destination disc image.
	RvtH_BankEntry *entry_dest;

	if (!rvth_dest || !rvth_src) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank_src >= rvth_src->bank_count ||
		   bank_dest >= rvth_dest->bank_count)
	{
		errno = ERANGE;
		return -ERANGE;
	} else if (!rvth_dest->is_hdd) {
		// Destination is not an HDD.
		errno = EIO;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	}

	// Check if the source bank can be imported.
	entry_src = &rvth_src->entries[bank_src];
	switch (entry_src->type) {
		case RVTH_BankType_GCN:
			// Bank can be imported.
			break;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can only be imported if it's
			// either unencrypted or Debug crypto.
			// TODO: Encryption conversion.
			if (entry_src->crypto_type != RVTH_CryptoType_None &&
			    entry_src->crypto_type != RVTH_CryptoType_Debug)
			{
				// Cannot import this image.
				errno = EIO;
				return RVTH_ERROR_IS_RETAIL_CRYPTO;
			}

			// Bank can be imported.
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

	// Source image length cannot be larger than a single bank.
	// TODO: Dual-layer Wii support.
	if (entry_src->lba_len > NHCD_BANK_SIZE_LBA) {
		return RVTH_ERROR_IMAGE_TOO_BIG;
	} else if (bank_dest == 0) {
		// Special handling for bank 1 if the bank table is extended.
		// TODO: entry_dest->lba_len should be the full bank size
		// if the bank is empty or deleted.
		// TODO: Add a separate field, lba_max_len?
		if (rvth_get_BankCount(rvth_dest) > 8) {
			// Image cannot be larger than NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA.
			if (entry_src->lba_len > NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA) {
				return RVTH_ERROR_IMAGE_TOO_BIG;
			}
		}
	}

	// Destination bank must be either empty or deleted.
	entry_dest = &rvth_dest->entries[bank_dest];
	if (entry_dest->type != RVTH_BankType_Empty &&
	    !entry_dest->is_deleted)
	{
		return RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED;
	}

	// Make the destination RVT-H object writable.
	ret = rvth_make_writable(rvth_dest);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// If no reader is set up for the destination bank, set one up now.
	if (!entry_dest->reader) {
		entry_dest->reader = reader_plain_open(rvth_dest->f_img,
			entry_dest->lba_start, entry_dest->lba_len);
		if (!entry_dest->reader) {
			// Cannot create a reader...
			return -errno;
		}
	}

	// Process 1 MB at a time.
	#define BUF_SIZE 1048576
	#define LBA_COUNT_BUF BYTES_TO_LBA(BUF_SIZE)
	errno = 0;
	buf = malloc(BUF_SIZE);
	if (!buf) {
		// Error allocating memory.
		return -errno;
	}

	// Copy the bank table information.
	memcpy(entry_dest->id6, entry_src->id6, sizeof(entry_dest->id6));
	memcpy(entry_dest->game_title, entry_src->game_title, sizeof(entry_dest->game_title));
	entry_dest->type	= entry_src->type;
	entry_dest->disc_number	= entry_src->disc_number;
	entry_dest->revision	= entry_src->revision;
	entry_dest->region_code	= entry_src->region_code;
	entry_dest->is_deleted	= false;

	// Timestamp.
	if (entry_src->timestamp >= 0) {
		entry_dest->timestamp = entry_src->timestamp;
	} else {
		entry_dest->timestamp = time(NULL);
	}

	// NOTE: We're only writing up to the source image file size.
	// There's no point in wiping the rest of the bank.
	lba_copy_len = entry_src->lba_len;

	// TODO: Special indicator.
	// TODO: Optimize seeking? (reader_write() seeks every time.)
	for (lba_count = 0; lba_count < lba_copy_len; lba_count += LBA_COUNT_BUF) {
		if (callback) {
			callback(lba_count, lba_copy_len);
		}

		// TODO: Error handling.
		reader_read(entry_src->reader, buf, lba_count, LBA_COUNT_BUF);
		reader_write(entry_dest->reader, buf, lba_count, LBA_COUNT_BUF);
	}

	// Process any remaining LBAs.
	if (lba_count < lba_copy_len) {
		const unsigned int lba_left = lba_copy_len - lba_count;
		reader_read(entry_src->reader, buf, lba_count, lba_left);
		reader_write(entry_dest->reader, buf, lba_count, lba_left);
	}

	if (callback) {
		callback(lba_copy_len, lba_copy_len);
	}

	// Flush the buffers.
	reader_flush(entry_dest->reader);

	// Update the bank table.
	// TODO: Check for errors.
	rvth_write_BankEntry(rvth_dest, bank_dest);

	// Finished importing the disc image.
	free(buf);
	return 0;
}

/**
 * Import a disc image into an RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_open() and rvth_copy_to_hdd().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Source GCM filename.
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_import(RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback)
{
	RvtH *rvth_src;
	int ret;

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Open the standalone disc image.
	ret = 0;
	rvth_src = rvth_open(filename, &ret);
	if (!rvth_src) {
		// Error opening the standalone disc image.
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	} else if (rvth_src->is_hdd || rvth_src->bank_count > 1) {
		// Not a standalone disc image.
		rvth_close(rvth_src);
		errno = EINVAL;
		return RVTH_ERROR_IS_HDD_IMAGE;
	} else if (rvth_src->bank_count == 0) {
		// Unrecognized file format.
		// TODO :Distinguish between unrecognized and no banks.
		errno = EINVAL;
		return RVTH_ERROR_NO_BANKS;
	}

	// Copy the bank from the source GCM to the HDD.
	// TODO: HDD to HDD?
	// NOTE: `bank` parameter starts at 0, not 1.
	ret = rvth_copy_to_hdd(rvth, bank, rvth_src, 0, callback);
	rvth_close(rvth_src);
	return ret;
}
