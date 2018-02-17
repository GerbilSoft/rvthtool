/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * ptbl.h: Load and manage a Wii disc's partition tables.                  *
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

#ifndef __RVTHTOOL_LIBRVTH_PTBL_H__
#define __RVTHTOOL_LIBRVTH_PTBL_H__

#include "rvth.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parsed partition table entry.
// Usually stored in an array with one empty entry. (lba_start == 0)
typedef struct _pt_entry_t {
	uint32_t lba_start;	// Starting LBA.
	uint32_t lba_len;	// Length, in LBAs.
	uint32_t type;		// Partition type.
	uint8_t vg;		// Volume group number.
	uint8_t pt;		// Partition number.
	uint8_t pt_orig;	// Original partition number.
				// This is only different from pt
				// if an update partition was
				// deleted and this partition was
				// shifted over.
} pt_entry_t;

struct _RvtH_BankEntry;

/**
 * Load the partition table from a Wii disc image.
 *
 * This function consolidates all of the partitions from all four
 * volume groups into a single array and sorts them by lba_start.
 * The resulting array is stored in the RvtH_BankEntry*.
 *
 * If the partition table was already loaded, this function does nothing.
 *
 * @param entry			[in] RVTH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_load(struct _RvtH_BankEntry *entry);

/**
 * Remove update partitions from a Wii disc image's partition table.
 *
 * This function loads the partition table if it hasn't been loaded
 * yet, then removes all entries with type == 1.
 *
 * @param entry			[in] RVTH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_RemoveUpdates(struct _RvtH_BankEntry *entry);

/**
 * Write the partition table back to the disc image.
 * The corresponding RvtH object must be writable.
 *
 * If the partition table hasn't been loaded yet, this function does nothing.
 *
 * @param entry			[in] RVTH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_write(struct _RvtH_BankEntry *entry);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_PTBL_H__ */
