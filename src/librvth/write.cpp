/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * write.cpp: RVT-H write functions.                                       *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_error.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// Disc image reader
#include "reader/Reader.hpp"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>
#include <ctime>

/**
 * Create a writable RVT-H disc image object.
 *
 * Check isOpen() after constructing the object to determine
 * if the file was opened successfully.
 *
 * @param filename	[in] Filename.
 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
RvtH::RvtH(const TCHAR *filename, uint32_t lba_len, int *pErr)
	: m_file(nullptr)
	, m_imageType(RVTH_ImageType_Unknown)
	, m_NHCD_status(NHCD_STATUS_UNKNOWN)
{
	RvtH_BankEntry *entry;

	if (!filename || filename[0] == 0 || lba_len == 0) {
		// Invalid parameters.
		if (pErr) {
			*pErr = EINVAL;
		}
		return;
	}

	int err = 0;	// errno setting
	errno = 0;

	// Allocate memory for a single RvtH_BankEntry object.
	m_entries.resize(1);
	m_imageType = RVTH_ImageType_GCM;

	// Attempt to create the file.
	m_file = new RefFile(filename, true);
	if (!m_file->isOpen()) {
		// Error creating the file.
		err = m_file->lastError();
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	entry = &m_entries[0];
	entry->lba_start = 0;
	entry->lba_len = lba_len;
	entry->type = RVTH_BankType_Empty;
	entry->is_deleted = false;

	// Timestamp.
	// TODO: Update on write.
	entry->timestamp = time(nullptr);

	// Initialize the disc image reader.
	entry->reader = Reader::open(m_file, entry->lba_start, entry->lba_len);
	if (!entry->reader) {
		// Error creating the disc image reader.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// We're done here.
	return;

fail:
	// Failed to create a GCM file.
	// TODO: Delete it in case it was partially created?
	if (m_file) {
		m_file->unref();
		m_file = nullptr;
	}
	if (pErr) {
		*pErr = -err;
	}
	errno = err;
}

/**
 * Delete a bank on an RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::deleteBank(unsigned int bank)
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
	int ret = this->makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Is the bank deleted?
	RvtH_BankEntry *const rvth_entry = &m_entries[bank];
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
	rvth_entry->timestamp = -1;
	ret = this->writeBankEntry(bank);
	m_file->flush();
	if (ret != 0) {
		// Error deleting the bank...
		rvth_entry->is_deleted = false;
	}
	return ret;
}

/**
 * Undelete a bank on an RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::undeleteBank(unsigned int bank)
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
	int ret = this->makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Is the bank deleted?
	RvtH_BankEntry *const rvth_entry = &m_entries[bank];
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
	uint8_t sbuf[LBA_SIZE];	// sector buffer
	uint32_t lba_size = rvth_entry->reader->read(sbuf, 0, 1);
	if (lba_size == 1) {
		// Check if the disc header is correct.
		if (memcmp(sbuf, &rvth_entry->discHeader, sizeof(rvth_entry->discHeader)) != 0) {
			// Disc header is incorrect.
			// Restore it.
			memcpy(sbuf, &rvth_entry->discHeader, sizeof(rvth_entry->discHeader));
			// TODO: Check for errors.
			rvth_entry->reader->write(sbuf, 0, 1);
		}
	}

	// Undelete the bank and write the entry.
	rvth_entry->is_deleted = false;
	ret = this->writeBankEntry(bank, &rvth_entry->timestamp);
	m_file->flush();
	if (ret != 0) {
		// Error undeleting the bank...
		rvth_entry->is_deleted = true;
	}
	return ret;
}
