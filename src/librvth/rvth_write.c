/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.c: RVT-H write functions.                                          *
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
 * Create a writable disc image object.
 * @param filename	[in] Filename.
 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH on success; NULL on error. (check errno)
 */
RvtH *rvth_create_gcm(const TCHAR *filename, uint32_t lba_len, int *pErr)
{
	RvtH *rvth;
	RvtH_BankEntry *entry;
	RefFile *f_img;

	if (!filename || filename[0] == 0 || lba_len == 0) {
		// Invalid parameters.
		if (pErr) {
			*pErr = EINVAL;
		}
		errno = EINVAL;
		return NULL;
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		if (pErr) {
			*pErr = errno;
		}
		return NULL;
	}

	// Allocate memory for a single RvtH_BankEntry object.
	rvth->bank_count = 1;
	rvth->is_hdd = false;
	rvth->entries = calloc(1, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	// Attempt to create the file.
	f_img = ref_create(filename);
	if (!f_img) {
		// Error creating the file.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		free(rvth);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	rvth->f_img = f_img;
	entry = rvth->entries;
	entry->lba_start = 0;
	entry->lba_len = lba_len;
	entry->type = RVTH_BankType_Empty;
	entry->is_deleted = false;

	// Timestamp.
	// TODO: Update on write.
	entry->timestamp = time(NULL);

	// Initialize the disc image reader.
	entry->reader = reader_plain_open(f_img, entry->lba_start, entry->lba_len);

	// We're done here.
	return rvth;
}

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
	} else if (!rvth->is_hdd) {
		// Standalone disc image. No bank table.
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

	// If the bank entry is deleted, then it should be
	// all zeroes, so skip all of this.
	rvth_entry = &rvth->entries[bank];
	if (rvth_entry->is_deleted)
		goto skip_creating_bank_entry;

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

skip_creating_bank_entry:
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
			if (!is_block_empty(&buf[sprs], 4096)) {
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
			if (!is_block_empty(&buf[sprs], 512)) {
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
	} else if (!rvth->is_hdd) {
		// Standalone disc image. No bank table.
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
	ref_flush(rvth->f_img);
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
	} else if (!rvth->is_hdd) {
		// Standalone disc image. No bank table.
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
