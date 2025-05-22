/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_p.cpp: RVT-H image handler. (PRIVATE CLASS)                        *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_p.hpp"
#include "rvth_error.h"

#include "RefFile.hpp"
#include "rvth_time.h"
#include "disc_header.hpp"

#include "byteswap.h"
#include "nhcd_structs.h"
#include "bank_init.h"
#include "reader/Reader.hpp"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

RvtHPrivate::RvtHPrivate(RvtH *q)
	: q_ptr(q)
	, imageType(RVTH_ImageType_Unknown)
	, nhcdStatus(NHCD_STATUS_UNKNOWN)
{ }

RvtHPrivate::~RvtHPrivate()
{
	// Close all bank entry files.
	// RefFile has a reference count, so we have to clear the count.
	for (RvtH_BankEntry &entry : entries) {
		delete entry.reader;
		free(entry.ptbl);
	}
}

/** General utility functions **/

/**
 * Check if a block is empty.
 * @param block Block
 * @param size Block size (Must be a multiple of 64 bytes.)
 * @return True if the block is all zeroes; false if not.
 */
bool RvtHPrivate::isBlockEmpty(const uint8_t *block, unsigned int size)
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

/** Constructor functions **/

/**
 * Open a Wii or GameCube disc image.
 * @param f_img	[in] RefFile*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtHPrivate::openGcm(const RefFilePtr &f_img)
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
	entries.resize(1);
	imageType = reader->type();

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	file = f_img;
	nhcdStatus = NHCD_STATUS_MISSING;
	entry = entries.data();
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
	file.reset();
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
int RvtHPrivate::checkMBR(RefFile *f_img, bool *pMBR, bool *pGPT)
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
 * @param f_img	[in] RefFile
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtHPrivate::openHDD(const RefFilePtr &f_img)
{
	RvtH_BankEntry *rvth_entry;
	uint32_t bankCount;
	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	off64_t addr;
	size_t size;

	// Check the bank table header.
	nhcdHeader.reset(new NHCD_BankTable_Header);
	size = f_img->seekoAndRead(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET,
		nhcdHeader.get(), 1, sizeof(NHCD_BankTable_Header));
	if (size != sizeof(NHCD_BankTable_Header)) {
		// Short read.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		nhcdHeader.reset();
		goto fail;
	}

	// Determine the device type.
	imageType = (f_img->isDevice()
		? RVTH_ImageType_HDD_Reader
		: RVTH_ImageType_HDD_Image);

	// Check the magic number.
	if (nhcdHeader->magic == be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Magic number is correct.
		nhcdStatus = NHCD_STATUS_OK;
	} else {
		// Incorrect magic number.
		// We'll continue with a default bank table.
		// HDD will be non-writable.
		uint32_t lba_start;

		// Check for MBR or GPT for better error reporting.
		bool hasMBR = false, hasGPT = false;
		ret = checkMBR(f_img.get(), &hasMBR, &hasGPT);
		if (ret != 0) {
			// Error checking for MBR or GPT.
			err = -ret;
			goto fail;
		}

		if (hasGPT) {
			nhcdStatus = NHCD_STATUS_HAS_GPT;
		} else if (hasMBR) {
			nhcdStatus = NHCD_STATUS_HAS_MBR;
		} else {
			nhcdStatus = NHCD_STATUS_MISSING;
		}

		// Assuming a default 8-bank system.
		static const unsigned int bankCount_default = 8;
		entries.resize(bankCount_default);

		file = f_img;
		rvth_entry = entries.data();
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
	bankCount = be32_to_cpu(nhcdHeader->bank_count);
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
	entries.resize(bankCount);

	file = f_img;
	rvth_entry = entries.data();
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
	file.reset();
	if (err != 0) {
		errno = err;
	}
	return ret;
}

/** Private functions **/

/**
 * Make the RVT-H object writable.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtHPrivate::makeWritable(void)
{
	if (file->isWritable()) {
		// RVT-H is already writable.
		return 0;
	}

	// TODO: Allow making a disc image file writable.
	// (Single bank)

	// Make sure this is a device file.
	if (!file->isDevice()) {
		// This is not a device file.
		// Cannot make it writable.
		return RVTH_ERROR_NOT_A_DEVICE;
	}

	// If we're using a fake NHCD table, we can't write to it.
	if (nhcdStatus != NHCD_STATUS_OK) {
		// Fake NHCD table is in use.
		return RVTH_ERROR_NHCD_TABLE_MAGIC;
	}

	// Make this writable.
	return file->makeWritable();
}

/**
 * Write a bank table entry to disk.
 * @param bank		[in] Bank number. (0-7)
 * @param pTimestamp	[out,opt] Timestamp written to the bank entry.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtHPrivate::writeBankEntry(unsigned int bank, time_t *pTimestamp)
{
	if (!isHDD()) {
		// Standalone disc image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= bankCount()) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	int ret = makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Create a new NHCD bank entry.
	NHCD_BankEntry nhcd_entry;
	memset(&nhcd_entry, 0, sizeof(nhcd_entry));

	// If the bank entry is deleted, then it should be
	// all zeroes, so skip all of this.
	RvtH_BankEntry *const rvth_entry = &entries[bank];
	if (!rvth_entry->is_deleted) {
		// Bank entry is not deleted.
		// Construct the NHCD bank entry.

		// Check the bank type.
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

		if (rvth_entry->type != RVTH_BankType_Empty) {
			// ASCII zero bytes.
			memset(nhcd_entry.all_zero, '0', sizeof(nhcd_entry.all_zero));

			// Timestamp.
			rvth_timestamp_create(nhcd_entry.timestamp, sizeof(nhcd_entry.timestamp), time(nullptr));
			if (pTimestamp) {
				// Convert the timestamp back.
				// NOTE: We can't just use the value from time(nullptr) because
				// that's in UTC/GMT, and we need localtime here.
				*pTimestamp = rvth_timestamp_parse(nhcd_entry.timestamp);
			}

			// LBA start and length.
			nhcd_entry.lba_start = cpu_to_be32(rvth_entry->lba_start);
			nhcd_entry.lba_len = cpu_to_be32(rvth_entry->lba_len);
		}
	}

	// Write the bank entry.
	ret = file->seeko(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA + bank+1), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}
	size_t size = file->write(&nhcd_entry, 1, sizeof(nhcd_entry));
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
