/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * ptbl.h: Load and manage a Wii disc's partition tables.                  *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "rvth.hpp"
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
 * @param entry		[in] RvtH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_load(struct _RvtH_BankEntry *entry);

/**
 * Remove update partitions from a Wii disc image's partition table.
 *
 * This function loads the partition table if it hasn't been loaded
 * yet, then removes all entries with type == 1.
 *
 * @param entry		[in] RvtH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_RemoveUpdates(struct _RvtH_BankEntry *entry);

/**
 * Write the partition table back to the disc image.
 * The corresponding RvtH object must be writable.
 *
 * If the partition table hasn't been loaded yet, this function does nothing.
 *
 * @param entry		[in] RvtH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_write(struct _RvtH_BankEntry *entry);

/**
 * Find the game partition in a Wii disc image.
 * @param entry		[in] RvtH_BankEntry*
 * @return Game partition entry, or NULL on error.
 */
const pt_entry_t *rvth_ptbl_find_game(RvtH_BankEntry *entry);

#ifdef __cplusplus
}
#endif
