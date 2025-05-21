/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.cpp: RVT-H image handler.                                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_p.hpp"
#include "rvth_error.h"

#include "nhcd_structs.h"
#include "disc_header.hpp"
#include "ptbl.h"
#include "bank_init.h"
#include "reader/Reader.hpp"

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"

#include "time_r.h"

// C includes.
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Open a Wii or GameCube disc image.
 * @param f_img	[in] RefFile*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::openGcm(RefFile *f_img)
{
	RvtH_BankEntry *entry;
	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	Reader *reader = nullptr;
	off64_t len;
	uint8_t type;

	// Disc header.
	union {
		GCN_DiscHeader gcn;
		uint8_t sbuf[LBA_SIZE];
	} discHeader;

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

	// Allocate memory for a single RvtH_BankEntry object.
	d_ptr->entries.resize(1);
	d_ptr->imageType = reader->type();

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	d_ptr->file = f_img->ref();
	d_ptr->nhcdStatus = NHCD_STATUS_MISSING;
	entry = d_ptr->entries.data();
	entry->lba_start = reader->lba_start();
	entry->lba_len = reader->lba_len();
	entry->type = type;
	entry->is_deleted = false;
	entry->reader = reader;

	// Timestamp. (using file mtime)
	// NOTE: RVT-H doesn't use timezones, so we need to
	// remove the local timezone offset.
	// TODO: _r() functions if available.
	{
		time_t mtime = f_img->mtime();
		if (mtime != -1) {
			struct tm tmbuf_local;
			mtime = timegm(localtime_r(&mtime, &tmbuf_local));
		}
		entry->timestamp = mtime;
	}

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
	return RVTH_ERROR_SUCCESS;

fail:
	// Failed to open the disc image.
	delete reader;
	if (d_ptr->file) {
		d_ptr->file->unref();
		d_ptr->file = nullptr;
	}
	if (err != 0) {
		errno = err;
	}
	return ret;
}

/**
 * Check for MBR and/or GPT.
 * @param f_img	[in] RefFile*
 * @param pMBR	[out] True if the HDD has an MBR.
 * @param pGPT	[out] True if the HDD has a GPT.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::checkMBR(RefFile *f_img, bool *pMBR, bool *pGPT)
{
	uint8_t sector_buffer[LBA_SIZE];
	*pMBR = false;
	*pGPT = false;

	// Read LBA 0.
	size_t size = f_img->seekoAndRead(LBA_TO_BYTES(0), SEEK_SET, sector_buffer, 1, sizeof(sector_buffer));
	if (size != sizeof(sector_buffer)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		return -err;
	}

	if (sector_buffer[0x1FE] == 0x55 && sector_buffer[0x1FF] == 0xAA) {
		// Found an MBR signature.
		*pMBR = true;

		// Check for a protective GPT partition.
		if (sector_buffer[0x1BE + 4] == 0xEE ||
		    sector_buffer[0x1CE + 4] == 0xEE ||
		    sector_buffer[0x1DE + 4] == 0xEE ||
		    sector_buffer[0x1EE + 4] == 0xEE)
		{
			// Found a protective GPT partition.
			*pGPT = true;
			return 0;
		}
	}

	// Check for "EFI PART" at LBA 1.
	// Note that LBA 1 might be either 512 or 4096, depending on
	// the drive's sector size. We'll check both.

	// Check 512. (512-byte sectors)
	size = f_img->seekoAndRead(512, SEEK_SET, sector_buffer, 1, sizeof(sector_buffer));
	if (size != sizeof(sector_buffer)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		return err;
	}

	if (!memcmp(&sector_buffer[0], "EFI PART", 8)) {
		// Found "EFI PART".
		*pGPT = true;
		return 0;
	}

	// Check 4096. (4k sectors)
	size = f_img->seekoAndRead(4096, SEEK_SET, sector_buffer, 1, sizeof(sector_buffer));
	if (size != sizeof(sector_buffer)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		return err;
	}

	if (!memcmp(&sector_buffer[0], "EFI PART", 8)) {
		// Found "EFI PART".
		*pGPT = true;
		return 0;
	}

	return 0;
}

/**
 * Open an RVT-H disk image.
 * @param f_img	[in] RefFile*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::openHDD(RefFile *f_img)
{
	RvtH_BankEntry *rvth_entry;
	uint32_t bankCount;
	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	off64_t addr;
	size_t size;

	// Check the bank table header.
	d_ptr->nhcdHeader.reset(new NHCD_BankTable_Header);
	size = f_img->seekoAndRead(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET,
		d_ptr->nhcdHeader.get(), 1, sizeof(NHCD_BankTable_Header));
	if (size != sizeof(NHCD_BankTable_Header)) {
		// Short read.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		d_ptr->nhcdHeader.reset();
		goto fail;
	}

	// Determine the device type.
	d_ptr->imageType = (f_img->isDevice()
		? RVTH_ImageType_HDD_Reader
		: RVTH_ImageType_HDD_Image);

	// Check the magic number.
	if (d_ptr->nhcdHeader->magic == be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Magic number is correct.
		d_ptr->nhcdStatus = NHCD_STATUS_OK;
	} else {
		// Incorrect magic number.
		// We'll continue with a default bank table.
		// HDD will be non-writable.
		uint32_t lba_start;

		// Check for MBR or GPT for better error reporting.
		bool hasMBR = false, hasGPT = false;
		ret = checkMBR(f_img, &hasMBR, &hasGPT);
		if (ret != 0) {
			// Error checking for MBR or GPT.
			err = -ret;
			goto fail;
		}

		if (hasGPT) {
			d_ptr->nhcdStatus = NHCD_STATUS_HAS_GPT;
		} else if (hasMBR) {
			d_ptr->nhcdStatus = NHCD_STATUS_HAS_MBR;
		} else {
			d_ptr->nhcdStatus = NHCD_STATUS_MISSING;
		}

		// Assuming a default 8-bank system.
		static const unsigned int bankCount_default = 8;
		d_ptr->entries.resize(bankCount_default);

		d_ptr->file = f_img->ref();
		rvth_entry = d_ptr->entries.data();
		lba_start = NHCD_BANK_START_LBA(0, 8);
		for (unsigned int i = 0; i < bankCount_default; i++, rvth_entry++, lba_start += NHCD_BANK_SIZE_LBA) {
			// Use "Empty" so we can try to detect the actual bank type.
			rvth_init_BankEntry(rvth_entry, f_img,
				RVTH_BankType_Empty,
				lba_start, NHCD_BANK_SIZE_LBA, 0);
		}

		// RVT-H image loaded.
		return RVTH_ERROR_SUCCESS;
	}

	// Get the bank count.
	bankCount = be32_to_cpu(d_ptr->nhcdHeader->bank_count);
	if (bankCount < 8 || bankCount > 32) {
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

	// Allocate memory for the RvtH_BankEntry objects.
	d_ptr->entries.resize(bankCount);

	d_ptr->file = f_img->ref();
	rvth_entry = d_ptr->entries.data();
	// FIXME: Why cast to uint32_t?
	addr = (uint32_t)(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA) + NHCD_BLOCK_SIZE);
	for (unsigned int i = 0; i < bankCount; i++, rvth_entry++, addr += 512) {
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
			lba_start = NHCD_BANK_START_LBA(i, bankCount);
			lba_len = 0;
		}

		// Initialize the bank entry.
		rvth_init_BankEntry(rvth_entry, f_img, type,
			lba_start, lba_len, nhcd_entry.timestamp);
	}

	// RVT-H image loaded.
	return RVTH_ERROR_SUCCESS;

fail:
	// Failed to open the HDD image.
	if (d_ptr->file) {
		d_ptr->file->unref();
		d_ptr->file = nullptr;
	}
	if (err != 0) {
		errno = err;
	}
	return ret;
}

/**
 * Open an RVT-H disk image, GameCube disc image, or Wii disc image.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
RvtH::RvtH(const TCHAR *filename, int *pErr)
	: d_ptr(new RvtHPrivate(this))
{
	// Open the disk image.
	RefFile *const f_img = new RefFile(filename);
	if (!f_img->isOpen()) {
		// Could not open the file.
		if (pErr) {
			*pErr = -f_img->lastError();
		}
		f_img->unref();
		return;
	}

	// Determine if this is an HDD image or a disc image.
	errno = 0;
	int64_t len = f_img->size();
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
		int err = openGcm(f_img);
		if (pErr) {
			*pErr = err;
		}
	} else {
		// More than two banks.
		// This is most likely an RVT-H HDD image.
		errno = 0;
		int err = openHDD(f_img);
		if (pErr) {
			*pErr = err;
		}
	}

	// If the RvtH object was opened, it will have
	// called f_img->ref() to increment the reference count.
	f_img->unref();
}

RvtH::~RvtH()
{
	// Close all bank entry files.
	// RefFile has a reference count, so we have to clear the count.
	for (unsigned int i = 0; i < bankCount(); i++) {
		delete d_ptr->entries[i].reader;
		free(d_ptr->entries[i].ptbl);
	}

	// Clear the main file reference.
	if (d_ptr->file) {
		d_ptr->file->unref();
	}
}

/**
 * Is the file open?
 * @return True if open; false if not.
 */
bool RvtH::isOpen(void) const
{
	return (d_ptr->file != nullptr);
}

/**
 * Get the number of banks in the RVT-H disk image.
 * @return Number of banks
 */
unsigned int RvtH::bankCount(void) const
{
	return d_ptr->bankCount();
}

/**
 * Is this RVT-H object an RVT-H Reader / HDD image, or a standalone disc image?
 * @param rvth RVT-H object
 * @return True if the RVT-H object is an RVT-H Reader / HDD image; false if it's a standalone disc image.
 */
bool RvtH::isHDD(void) const
{
	return d_ptr->isHDD();
}

/**
 * Get the RVT-H image type.
 * @param rvth RVT-H object
 * @return RVT-H image type
 */
RvtH_ImageType_e RvtH::imageType(void) const
{
	return d_ptr->imageType;
}

/**
 * Get the NHCD table status.
 *
 * For optical disc images, this is always NHCD_STATUS_MISSING.
 * For HDD images, this should be NHCD_STATUS_OK unless the
 * NHCD table was wiped or a PC-partitioned disk was selected.
 *
 * @return NHCD table status
 */
NHCD_Status_e RvtH::nhcd_status(void) const
{
	return d_ptr->nhcdStatus;
}

/**
 * Get the NHCD Table Header.
 * @return NHCD Bank Table Header
 */
NHCD_BankTable_Header *RvtH::nhcd_header(void) const
{
	return d_ptr->nhcdHeader.get();
}

/**
 * Get a bank table entry.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range.
 */
const RvtH_BankEntry *RvtH::bankEntry(unsigned int bank, int *pErr) const
{
	if (bank >= bankCount()) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -ERANGE;
		}
		return nullptr;
	}

	return &d_ptr->entries[bank];
}
