/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_p.cpp: RVT-H image handler. (PRIVATE CLASS)                        *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "rvth.hpp"

// Enums
#include "rvth_enums.h"
#include "nhcd_structs.h"

// C++ STL classes
#include <memory>
#include <vector>

class RvtH;
class RvtHPrivate
{
public:
	explicit RvtHPrivate(RvtH *q);
	~RvtHPrivate() = default;

private:
	RvtH *const q_ptr;
	DISABLE_COPY(RvtHPrivate)

public:
	/** General utility functions **/

	/**
	 * Check if a block is empty.
	 * @param block Block
	 * @param size Block size (Must be a multiple of 64 bytes.)
	 * @return True if the block is all zeroes; false if not.
	 */
	static bool isBlockEmpty(const uint8_t *block, unsigned int size);

public:
	/** Constructor functions **/

	/**
	 * Open a Wii or GameCube disc image.
	 * @param f_img	[in] RefFile*
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int openGcm(RefFile *f_img);

	/**
	 * Check for MBR and/or GPT.
	 * @param f_img	[in] RefFile*
	 * @param pMBR	[out] True if the HDD has an MBR.
	 * @param pGPT	[out] True if the HDD has a GPT.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	static int checkMBR(RefFile *f_img, bool *pMBR, bool *pGPT);

	/**
	 * Open an RVT-H disk image.
	 * @param f_img	[in] RefFile*
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int openHDD(RefFile *f_img);

public:
	/** Accessors **/

	/**
	 * Get the number of banks in the RVT-H disk image.
	 * @return Number of banks.
	 */
	inline unsigned int bankCount(void) const
	{
		return static_cast<unsigned int>(entries.size());
	}

	/**
	 * Is this RVT-H object an RVT-H Reader / HDD image, or a standalone disc image?
	 * @param rvth RVT-H object
	 * @return True if the RVT-H object is an RVT-H Reader / HDD image; false if it's a standalone disc image.
	 */
	inline bool isHDD(void) const
	{
		switch (imageType) {
			case RVTH_ImageType_HDD_Reader:
			case RVTH_ImageType_HDD_Image:
				return true;

			default:
				break;
		}

		return false;
	}

public:
	/** Private functions **/

	/**
	 * Make the RVT-H object writable.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int makeWritable(void);

	/**
	 * Write a bank table entry to disk.
	 * @param bank		[in] Bank number. (0-7)
	 * @param pTimestamp	[out,opt] Timestamp written to the bank entry.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int writeBankEntry(unsigned int bank, time_t *pTimestamp = nullptr);

public:
	// Reference-counted FILE*
	RefFile *file;

	// Image type
	RvtH_ImageType_e imageType;

	// NHCD header status
	NHCD_Status_e nhcdStatus;

	// NHCD bank table header
	// NOTE: This will be nullptr for e.g. GCM disc images.
	std::unique_ptr<NHCD_BankTable_Header> nhcdHeader;

	// BankEntry objects
	// - RVT-H system or disk image: 8 (usually)
	// - Standalone disc image: 1
	std::vector<RvtH_BankEntry> entries;
};
