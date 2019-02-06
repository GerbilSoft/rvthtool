/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.cpp: RVT-H image handler.                                          *
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

#include "nhcd_structs.h"
#include "disc_header.h"
#include "ptbl.h"
#include "bank_init.h"
#include "reader/Reader.hpp"

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"

// C includes.
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Open a Wii or GameCube disc image.
 * @param f_img	[in] RefFile*
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
static RvtH *rvth_open_gcm(RefFile *f_img, int *pErr)
{
	RvtH *rvth = nullptr;
	RvtH_BankEntry *entry;
	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	Reader *reader = nullptr;
	int64_t len;
	uint8_t type;

	// Disc header.
	union {
		GCN_DiscHeader gcn;
		uint8_t sbuf[LBA_SIZE];
	} discHeader;

	// TODO: Detect CISO and WBFS.

	// Get the file length.
	// FIXME: This is obtained in rvth_open().
	// Pass it as a parameter?
	ret = f_img->seeko(0, SEEK_END);
	if (ret != 0) {
		// Seek error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto fail;
	}
	len = f_img->tello();

	// Rewind back to the beginning of the file.
	ret = f_img->seeko(0, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto fail;
	}

	// Initialize the disc image reader.
	// We need to do this before anything else in order to
	// handle CISO and WBFS images.
	reader = Reader::open(f_img, 0, BYTES_TO_LBA(len));
	if (!reader) {
		// Unable to open the reader.
		goto fail;
	}

	// Read the GCN disc header.
	// NOTE: Since this is a standalone disc image, we'll just
	// read the header directly.
	ret = reader->read(discHeader.sbuf, 0, 1);
	if (ret < 0) {
		// Error...
		err = -ret;
		goto fail;
	}

	// Identify the disc type.
	type = rvth_disc_header_identify(&discHeader.gcn);
	if (type == RVTH_BankType_Wii_SL &&
	    reader->lba_len() > NHCD_BANK_WII_SL_SIZE_RVTR_LBA)
	{
		// Dual-layer image.
		type = RVTH_BankType_Wii_DL;
	}

	// Allocate memory for the RvtH object
	rvth = (RvtH*)calloc(1, sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;	// NOTE: Standalone
		}
		ret = -err;
		goto fail;
	}

	// Allocate memory for a single RvtH_BankEntry object.
	rvth->bank_count = 1;
	rvth->type = reader->type();
	rvth->entries = (RvtH_BankEntry*)calloc(1, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto fail;
	};

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	rvth->f_img = f_img->ref();
	rvth->has_NHCD = false;
	entry = rvth->entries;
	entry->lba_start = reader->lba_start();
	entry->lba_len = reader->lba_len();
	entry->type = type;
	entry->is_deleted = false;
	entry->reader = reader;

	// Timestamp.
	// TODO: Get the timestamp from the file.
	entry->timestamp = -1;

	if (type != RVTH_BankType_Empty) {
		// Copy the disc header.
		memcpy(&entry->discHeader, &discHeader.gcn, sizeof(entry->discHeader));

		// TODO: Error handling.
		// Initialize the region code.
		rvth_init_BankEntry_region(entry);
		// Initialize the encryption status.
		rvth_init_BankEntry_crypto(entry);
		// Initialize the AppLoader error status.
		rvth_init_BankEntry_AppLoader(entry);
	}

	// Disc image loaded.
	return rvth;

fail:
	// Failed to open the disc image.
	if (reader) {
		delete reader;
	}

	rvth_close(rvth);
	if (pErr) {
		*pErr = ret;
	}
	if (err != 0) {
		errno = err;
	}
	return nullptr;
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
	RvtH *rvth = nullptr;
	RvtH_BankEntry *rvth_entry;
	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	unsigned int i;
	int64_t addr;
	size_t size;

	// Check the bank table header.
	ret = f_img->seeko(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto fail;
	}
	size = f_img->read(&nhcd_header, 1, sizeof(nhcd_header));
	if (size != sizeof(nhcd_header)) {
		// Short read.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto fail;
	}

	// Allocate memory for the RvtH object
	rvth = (RvtH*)calloc(1, sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto fail;
	}

	// Determine the device type.
	rvth->type = (f_img->isDevice()
		? RVTH_ImageType_HDD_Reader
		: RVTH_ImageType_HDD_Image);

	// Check the magic number.
	if (nhcd_header.magic == be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Magic number is correct.
		rvth->has_NHCD = true;
	} else {
		// Incorrect magic number.
		// We'll continue with a default bank table.
		// HDD will be non-writable.
		uint32_t lba_start;

		rvth->has_NHCD = false;
		rvth->bank_count = 8;
		rvth->entries = (RvtH_BankEntry*)calloc(rvth->bank_count, sizeof(RvtH_BankEntry));
		if (!rvth->entries) {
			// Error allocating memory.
			err = errno;
			if (err == 0) {
				err = ENOMEM;
			}
			ret = -err;
			goto fail;
		}

		rvth->f_img = f_img->ref();
		rvth_entry = rvth->entries;
		lba_start = NHCD_BANK_START_LBA(0, 8);
		for (i = 0; i < rvth->bank_count; i++, rvth_entry++, lba_start += NHCD_BANK_SIZE_LBA) {
			// Use "Empty" so we can try to detect the actual bank type.
			rvth_init_BankEntry(rvth_entry, f_img,
				RVTH_BankType_Empty,
				lba_start, NHCD_BANK_SIZE_LBA, 0);
		}

		// RVT-H image loaded.
		return rvth;
	}

	// Get the bank count.
	rvth->bank_count = be32_to_cpu(nhcd_header.bank_count);
	if (rvth->bank_count < 8 || rvth->bank_count > 32) {
		// Bank count is either too small or too large.
		// RVT-H systems are set to 8 banks at the factory,
		// but we're supporting up to 32 in case the user
		// has modified it.
		// TODO: More extensive "extra bank" testing.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto fail;
	}

	// Allocate memory for the 8 RvtH_BankEntry objects.
	rvth->entries = (RvtH_BankEntry*)calloc(rvth->bank_count, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto fail;
	};

	rvth->f_img = f_img->ref();
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

		ret = f_img->seeko(addr, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			ret = -err;
			goto fail;
		}
		size = f_img->read(&nhcd_entry, 1, sizeof(nhcd_entry));
		if (size != sizeof(nhcd_entry)) {
			// Short read.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			ret = -err;
			goto fail;
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
			lba_start = NHCD_BANK_START_LBA(i, rvth->bank_count);
			lba_len = 0;
		}

		// Initialize the bank entry.
		rvth_init_BankEntry(rvth_entry, f_img, type,
			lba_start, lba_len, nhcd_entry.timestamp);
	}

	// RVT-H image loaded.
	return rvth;

fail:
	// Failed to open the HDD image.
	rvth_close(rvth);
	if (pErr) {
		*pErr = ret;
	}
	if (err != 0) {
		errno = err;
	}
	return nullptr;
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
	RvtH *rvth = nullptr;
	int64_t len;

	// Open the disk image.
	f_img = new RefFile(filename);
	if (!f_img->isOpen()) {
		// Could not open the file.
		if (pErr) {
			*pErr = -f_img->lastError();
		}
		f_img->unref();
		return nullptr;
	}

	// Determine if this is an HDD image or a disc image.
	errno = 0;
	len = f_img->size();
	if (len <= 0) {
		// File is empty and/or an I/O error occurred.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
	} else if (len <= 2*LBA_TO_BYTES(NHCD_BANK_SIZE_LBA)) {
		// Two banks or less.
		// This is most likely a standalone disc image.
		errno = 0;
		rvth = rvth_open_gcm(f_img, pErr);
	} else {
		// More than two banks.
		// This is most likely an RVT-H HDD image.
		errno = 0;
		rvth = rvth_open_hdd(f_img, pErr);
	}

	// If the RvtH object was opened, it will have
	// called f_img->ref() to increment the reference count.
	f_img->unref();
	return rvth;
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
			delete rvth->entries[i].reader;
		}
		free(rvth->entries[i].ptbl);
	}

	// Free the bank entries array.
	free(rvth->entries);

	// Clear the main reference.
	if (rvth->f_img) {
		rvth->f_img->unref();
	}

	free(rvth);
}

/**
 * Is this RVT-H object an RVT-H Reader / HDD image, or a standalone disc image?
 * @param rvth RVT-H object.
 * @return True if the RVT-H object is an RVT-H Reader / HDD image; false if it's a standalone disc image.
 */
bool rvth_is_hdd(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return false;
	}

	switch (rvth->type) {
		case RVTH_ImageType_HDD_Reader:
		case RVTH_ImageType_HDD_Image:
			return true;

		case RVTH_ImageType_Unknown:
		case RVTH_ImageType_GCM:
		case RVTH_ImageType_GCM_SDK:
		default:
			return false;
	}

	assert(!"Should not get here!");
	return false;
}

/**
 * Is an NHCD table present?
 *
 * This is always false for disc images,
 * and false for wiped RVT-H devices and disk images.
 *
 * @param rvth RVT-H object.
 * @return True if the RVT-H object has an NHCD table; false if not.
 */
bool rvth_has_NHCD(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return false;
	}

	return rvth->has_NHCD;
}

/**
 * Get the RVT-H image type.
 * @param rvth RVT-H object.
 * @return RVT-H image type.
 */
RvtH_ImageType_e rvth_get_ImageType(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return RVTH_ImageType_Unknown;
	}

	return rvth->type;
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
 * @return Bank table entry, or NULL if out of range.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr)
{
	if (!rvth) {
		errno = EINVAL;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return nullptr;
	} else if (bank >= rvth->bank_count) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -ERANGE;
		}
		return nullptr;
	}

	return &rvth->entries[bank];
}
