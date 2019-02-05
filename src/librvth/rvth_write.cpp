/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_write.cpp: RVT-H write functions.                                  *
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

#include "rvth.h"
#include "rvth_p.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// Disc image reader.
#include "reader.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Create a writable disc image object.
 * @param filename	[in] Filename.
 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH on success; nullptr on error. (check errno)
 */
RvtH *rvth_create_gcm(const TCHAR *filename, uint32_t lba_len, int *pErr)
{
	RvtH *rvth = nullptr;
	RefFile *f_img = nullptr;
	RvtH_BankEntry *entry;
	int err = 0;	// errno setting

	if (!filename || filename[0] == 0 || lba_len == 0) {
		// Invalid parameters.
		err = EINVAL;
		goto fail;
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = (RvtH*)calloc(1, sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			errno = ENOMEM;
		}
		goto fail;
	}

	// Allocate memory for a single RvtH_BankEntry object.
	rvth->bank_count = 1;
	rvth->type = RVTH_ImageType_GCM;
	rvth->entries = (RvtH_BankEntry*)calloc(1, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto fail;
	};

	// Attempt to create the file.
	f_img = new RefFile(filename, true);
	if (!f_img->isOpen()) {
		// Error creating the file.
		err = f_img->lastError();
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	rvth->f_img = f_img->ref();
	entry = rvth->entries;
	entry->lba_start = 0;
	entry->lba_len = lba_len;
	entry->type = RVTH_BankType_Empty;
	entry->is_deleted = false;

	// Timestamp.
	// TODO: Update on write.
	entry->timestamp = time(nullptr);

	// Initialize the disc image reader.
	entry->reader = reader_open(f_img, entry->lba_start, entry->lba_len);
	if (!entry->reader) {
		// Error creating the disc image reader.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// We're done here.
	f_img->unref();
	return rvth;

fail:
	// Failed to create a GCM file.
	// TODO: Delete it in case it was partially created?
	rvth_close(rvth);
	if (f_img) {
		f_img->unref();
	}
	if (pErr) {
		*pErr = -err;
	}
	errno = err;
	return nullptr;
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
	} else if (!rvth_is_hdd(rvth)) {
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
	rvth->f_img->flush();
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

	// Sector buffer.
	uint8_t sbuf[LBA_SIZE];
	uint32_t lba_size;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (!rvth_is_hdd(rvth)) {
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

	// Restore the disc header if necessary.
	lba_size = reader_read(rvth_entry->reader, sbuf, 0, 1);
	if (lba_size == 1) {
		// Check if the disc header is correct.
		if (memcmp(sbuf, &rvth_entry->discHeader, sizeof(rvth_entry->discHeader)) != 0) {
			// Disc header is incorrect.
			// Restore it.
			memcpy(sbuf, &rvth_entry->discHeader, sizeof(rvth_entry->discHeader));
			// TODO: Check for errors.
			reader_write(rvth_entry->reader, sbuf, 0, 1);
		}
	}

	// Undelete the bank and write the entry.
	rvth_entry->is_deleted = false;
	ret = rvth_write_BankEntry(rvth, bank);
	if (ret != 0) {
		// Error undeleting the bank...
		rvth_entry->is_deleted = true;
	}
	return ret;
}
