/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_p.cpp: RVT-H image handler. (PRIVATE FUNCTIONS)                    *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"

#include "RefFile.hpp"
#include "rvth_time.h"
#include "rvth_error.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

/**
 * Make the RVT-H object writable.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::makeWritable(void)
{
	if (m_file->isWritable()) {
		// RVT-H is already writable.
		return 0;
	}

	// TODO: Allow making a disc image file writable.
	// (Single bank)

	// Make sure this is a device file.
	if (!m_file->isDevice()) {
		// This is not a device file.
		// Cannot make it writable.
		return RVTH_ERROR_NOT_A_DEVICE;
	}

	// If we're using a fake NHCD table, we can't write to it.
	if (m_NHCD_status != NHCD_STATUS_OK) {
		// Fake NHCD table is in use.
		return RVTH_ERROR_NHCD_TABLE_MAGIC;
	}

	// Make this writable.
	return m_file->makeWritable();
}

/**
 * Check if a block is empty.
 * @param block Block.
 * @param size Block size. (Must be a multiple of 64 bytes.)
 * @return True if the block is all zeroes; false if not.
 */
bool RvtH::isBlockEmpty(const uint8_t *block, unsigned int size)
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
 * Write a bank table entry to disk.
 * @param bank		[in] Bank number. (0-7)
 * @param pTimestamp	[out,opt] Timestamp written to the bank entry.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::writeBankEntry(unsigned int bank, time_t *pTimestamp)
{
	if (!isHDD()) {
		// Standalone disc image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= m_bankCount) {
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
	RvtH_BankEntry *const rvth_entry = &m_entries[bank];
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
	ret = m_file->seeko(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA + bank+1), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}
	size_t size = m_file->write(&nhcd_entry, 1, sizeof(nhcd_entry));
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
