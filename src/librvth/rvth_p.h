/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_p.h: RVT-H image handler. (PRIVATE HEADER)                         *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_P_H__
#define __RVTHTOOL_LIBRVTH_RVTH_P_H__

#include "common.h"

#include "rvth.h"
#include "ref_file.h"

// RVT-H main struct.
struct _RvtH {
	// Reference-counted FILE*.
	RefFile *f_img;

	// Number of banks.
	// - RVT-H system or disk image: 8
	// - Standalone disc image: 1
	unsigned int bank_count;

	// Is this an HDD image?
	bool is_hdd;

	// BankEntry objects.
	RvtH_BankEntry *entries;
};

/**
 * Make an RVT-H object writable.
 * @param rvth	[in] RVT-H disk image.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_make_writable(RvtH *rvth);

/**
 * Check if a block is empty.
 * @param block Block.
 * @param size Block size. (Must be a multiple of 64 bytes.)
 * @return True if the block is all zeroes; false if not.
 */
bool rvth_is_block_empty(const uint8_t *block, unsigned int size);

/**
 * Write a bank table entry to disk.
 * @param rvth	[in] RvtH device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_write_BankEntry(RvtH *rvth, unsigned int bank);

#endif /* __RVTHTOOL_LIBRVTH_RVTH_P_H__ */
