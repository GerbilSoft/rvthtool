/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * ptbl.c: Load and manage a Wii disc's partition tables.                  *
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

#include "ptbl.h"
#include "gcn_structs.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Volume group and partition tables.
typedef union _ptbl_t {
	uint8_t u8[RVTH_BLOCK_SIZE*2];
	struct {
		RVL_VolumeGroupTable vgtbl;
		RVL_PartitionTableEntry ptbl[31];
	};
} ptbl_t;

/**
 * qsort() comparison function for pt_entry_t.
 * @param a
 * @param b
 * @return
 */
static int compar_pt_entry_t(const void *a, const void *b)
{
	const pt_entry_t *const pt_a = (const pt_entry_t*)a;
	const pt_entry_t *const pt_b = (const pt_entry_t*)b;
	if (pt_a->lba_start < pt_b->lba_start) {
		return -1;
	} else if (pt_a->lba_start > pt_b->lba_start) {
		return 1;
	}
	return 0;
}

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
int rvth_ptbl_load(RvtH_BankEntry *entry)
{
	ptbl_t pt;			// On-disc partition table.
	pt_entry_t *ptbl = NULL;	// In-memory partition table.
	uint32_t lba_size;
	unsigned int pt_total;
	unsigned int pt_total_proc = 0;	// Partitions actually processed.
	unsigned int vg_idx, pt_idx;	// Current indexes.

	assert(entry != NULL);
	assert(entry->reader != NULL);
	if (!entry || !entry->reader) {
		return -EINVAL;
	}

	// Bank must be Wii SL or DL.
	if (entry->type != RVTH_BankType_Wii_SL &&
	    entry->type != RVTH_BankType_Wii_DL)
	{
		// Not a Wii disc image.
		return RVTH_ERROR_NOT_WII_IMAGE;
	}

	if (entry->ptbl) {
		// Partition table is already loaded.
		return 0;
	}

	// Load the volume group table and partition table from the disc image.
	errno = 0;
	lba_size = reader_read(entry->reader, &pt,
		BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS),
		BYTES_TO_LBA(sizeof(pt)));
	if (lba_size != BYTES_TO_LBA(sizeof(pt))) {
		// Read error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}

	// Count the total number of partitions.
	pt_total = be32_to_cpu(pt.vgtbl.vg[0].count) +
		   be32_to_cpu(pt.vgtbl.vg[1].count) +
		   be32_to_cpu(pt.vgtbl.vg[2].count) +
		   be32_to_cpu(pt.vgtbl.vg[3].count);
	if (pt_total == 0) {
		// No partitions...
		return 0;
	} else if (pt_total >= ARRAY_SIZE(pt.ptbl)) {
		// Too many partitions.
		errno = EIO;
		return -EIO;
	}

	// Allocate memory for the partition table.
	errno = 0;
	ptbl = calloc(pt_total, sizeof(*ptbl));
	if (!ptbl) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		return -errno;
	}

	// Process the partition table.
	for (vg_idx = 0; vg_idx < ARRAY_SIZE(pt.vgtbl.vg); vg_idx++) {
		// Partition indexes for the current volume group.
		unsigned int ptcount;			// Number of partitions in the current VG.
		// PTE pointers. (first, one past the last, current)
		RVL_PartitionTableEntry *pte_start, *pte_end, *pte;
		// Partition table address.
		int64_t ptbl_addr;

		// Partition index adjustment.
		// Normally 0, but if remove_updates is true, this is
		// incremented if an update partition is encountered.
		// TODO: Zero out the update partitions.
		unsigned int pt_adj = 0;

		ptcount = be32_to_cpu(pt.vgtbl.vg[vg_idx].count);
		if (ptcount == 0) {
			// No partitions in this volume group.
			continue;
		}

		// TODO: Use uint32_t arithmetic instead of int64_t?
		ptbl_addr = ((int64_t)be32_to_cpu(pt.vgtbl.vg[vg_idx].addr) << 2);
		if (ptbl_addr < (int64_t)(RVL_VolumeGroupTable_ADDRESS + sizeof(pt.vgtbl))) {
			// Partition table starts *before* the volume group table.
			// Something's wrong, but we'll just skip it for now.
			continue;
		}

		pte_start = &pt.ptbl[((ptbl_addr - (RVL_VolumeGroupTable_ADDRESS + sizeof(pt.vgtbl))) / sizeof(RVL_PartitionTableEntry))];
		pte_end = pte_start + ptcount;
		if (pte_end >= &pt.ptbl[ARRAY_SIZE(pt.ptbl)]) {
			// Partition table is too big.
			// Something's wrong, but we'll just skip it for now.
			continue;
		}

		// Process the partitions.
		for (pte = pte_start; pte < pte_end; pte++) {
			ptbl[pt_total_proc].lba_start = (be32_to_cpu(pte->addr) / (RVTH_BLOCK_SIZE/4));
			ptbl[pt_total_proc].lba_len = 0;	// will be calculated later
			ptbl[pt_total_proc].type = be32_to_cpu(pte->type);
			ptbl[pt_total_proc].vg = vg_idx;
			ptbl[pt_total_proc].pt = (unsigned int)(pte - pte_start);
			ptbl[pt_total_proc].pt_orig = ptbl[pt_total_proc].pt + pt_adj;
			pt_total_proc++;
		}
	}

	// Sort the partition table by lba_start.
	qsort(ptbl, pt_total_proc, sizeof(*ptbl), compar_pt_entry_t);

	// Calculate the size of each partition.
	// TODO: Use the data size in the partition header if it's available?
	for (pt_idx = 0; pt_idx < pt_total_proc-1; pt_idx++) {
		ptbl[pt_idx].lba_len = ptbl[pt_idx+1].lba_start - ptbl[pt_idx].lba_start;
	}
	// Last partition.
	ptbl[pt_total_proc-1].lba_len = ptbl[pt_total_proc-1].lba_start - entry->lba_len;

	// Save the original per-table addresses and counts.
	for (vg_idx = 0; vg_idx < 4; vg_idx++) {
		entry->vg_orig.vg[vg_idx].addr = be32_to_cpu(pt.vgtbl.vg[vg_idx].addr);
		entry->vg_orig.vg[vg_idx].count = (uint8_t)be32_to_cpu(pt.vgtbl.vg[vg_idx].count);
	}

	// Partition table has been loaded.
	entry->ptbl = ptbl;
	entry->pt_count = pt_total_proc;
	return 0;
}

/**
 * Remove update partitions from a Wii disc image's partition table.
 *
 * This function loads the partition table if it hasn't been loaded
 * yet, then removes all entries with type == 1.
 *
 * @param entry		[in] RvtH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_RemoveUpdates(RvtH_BankEntry *entry)
{
	int ret;
	unsigned int i;
	pt_entry_t *pte;

	assert(entry != NULL);
	assert(entry->reader != NULL);
	if (!entry || !entry->reader) {
		return -EINVAL;
	}

	// Make sure the partition table is loaded.
	ret = rvth_ptbl_load(entry);
	if (ret != 0) {
		// Error loading the partition table.
		return ret;
	}

	// Look for update partitions.
	pte = entry->ptbl;
	for (i = 0; i < entry->pt_count; i++, pte++) {
		if (pte->type != 1) {
			// Not an update partition.
			continue;
		}

		// Found an update partition.
		// Shift all other partitions over.
		if (i+1 < entry->pt_count) {
			memmove(pte, pte+1, (entry->pt_count-i-1) * sizeof(*pte));
		}
		// Decrement the counters.
		i--; pte--; entry->pt_count--;
	}

	// Update partitions removed.
	return 0;
}

/**
 * Write the partition table back to the disc image.
 * The corresponding RvtH object must be writable.
 *
 * If the partition table hasn't been loaded yet, this function does nothing.
 *
 * @param entry		[in] RvtH_BankEntry*
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_ptbl_write(RvtH_BankEntry *entry)
{
	ptbl_t pt;
	RVL_PartitionTableEntry *ptptr[4];
	const pt_entry_t *pte;
	unsigned int vg_idx, pt_idx;
	uint32_t lba_size;

	assert(entry != NULL);
	assert(entry->reader != NULL);
	if (!entry || !entry->reader) {
		return -EINVAL;
	}

	if (!entry->ptbl) {
		// Partition table hasn't been loaded.
		// Nothing to do.
		return 0;
	}

	// Create a new partition table.
	memset(&pt, 0, sizeof(pt));
	pt_idx = 0;
	for (vg_idx = 0; vg_idx < ARRAY_SIZE(pt.vgtbl.vg); vg_idx++) {
		const uint32_t ptbyte =
			(entry->vg_orig.vg[vg_idx].addr << 2) - RVL_VolumeGroupTable_ADDRESS - sizeof(pt.vgtbl);

		pt.vgtbl.vg[vg_idx].count = 0;	// counted later
		pt.vgtbl.vg[vg_idx].addr = cpu_to_be32(entry->vg_orig.vg[vg_idx].addr);
		ptptr[vg_idx] = &pt.ptbl[ptbyte / sizeof(pt.ptbl[0])];
		pt_idx += entry->vg_orig.vg[vg_idx].count;
	}

	// Copy the partition information to the table.
	pte = entry->ptbl;
	for (pt_idx = 0; pt_idx < entry->pt_count; pt_idx++, pte++) {
		assert(pte->vg < ARRAY_SIZE(pt.vgtbl.vg));
		pt.vgtbl.vg[pte->vg].count++;
		ptptr[pte->vg]->addr = cpu_to_be32(pte->lba_start * (RVTH_BLOCK_SIZE/4));
		ptptr[pte->vg]->type = cpu_to_be32(pte->type);
		ptptr[pte->vg]++;
	}

	// Zero out empty volume groups.
	for (vg_idx = 0; vg_idx < ARRAY_SIZE(pt.vgtbl.vg); vg_idx++) {
		if (pt.vgtbl.vg[vg_idx].count == 0) {
			pt.vgtbl.vg[vg_idx].addr = 0;
		} else {
			pt.vgtbl.vg[vg_idx].count = cpu_to_be32(pt.vgtbl.vg[vg_idx].count);
		}
	}

	// Write the partition information to the disc image.
	lba_size = reader_write(entry->reader, &pt,
		BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS),
		BYTES_TO_LBA(sizeof(pt)));
	if (lba_size != BYTES_TO_LBA(sizeof(pt))) {
		// Write error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}

	// Partition table updated.
	return 0;
}

/**
 * Find the game partition in a Wii disc image.
 * @param entry		[in] RvtH_BankEntry*
 * @return Game partition entry, or NULL on error.
 */
const pt_entry_t *rvth_ptbl_find_game(RvtH_BankEntry *entry)
{
	unsigned int i;
	int ret;
	const pt_entry_t *pte;

	assert(entry != NULL);
	assert(entry->reader != NULL);
	if (!entry || !entry->reader) {
		errno = EINVAL;
		return NULL;
	}

	// Bank must be Wii SL or DL.
	if (entry->type != RVTH_BankType_Wii_SL &&
	    entry->type != RVTH_BankType_Wii_DL)
	{
		// Not a Wii disc image.
		errno = EINVAL;
		return NULL;
	}

	// Make sure the partition table is loaded.
	ret = rvth_ptbl_load(entry);
	if (ret != 0 || entry->pt_count == 0 || !entry->ptbl) {
		// Unable to load the partition table.
		return NULL;
	}

	// Find the game partition in Volume Group 0.
	pte = entry->ptbl;
	for (i = 0; i < entry->pt_count; i++, pte++) {
		if (pte->vg == 0 && pte->type == 0) {
			// Found the game partition.
			return pte;
		}
	}

	// Not found.
	return NULL;
}
