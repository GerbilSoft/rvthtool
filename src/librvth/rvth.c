/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.c: RVT-H image handler.                                            *
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

#include "config.librvth.h"

#include "rvth.h"
#include "byteswap.h"
#include "nhcd_structs.h"
#include "gcn_structs.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
# include <windows.h>
# include <io.h>
# include <winioctl.h>
#elif defined(HAVE_FTRUNCATE)
# include <unistd.h>
# include <sys/types.h>
#endif

// RVT-H main struct.
struct _RvtH {
	// Reference-counted FILE*.
	RefFile *f_img;

	// Number of banks.
	// - RVT-H system or disk image: 8
	// - Standalone disc image: 1
	unsigned int banks;

	// BankEntry objects.
	RvtH_BankEntry *entries;
};

/**
 * Trim a game title.
 * @param title Game title.
 * @param size Size of game title.
 */
static void trim_title(char *title, int size)
{
	size--;
	for (; size >= 0; size--) {
		if (title[size] != ' ')
			break;
		title[size] = 0;
	}
}

/**
 * Parse an RVT-H timestamp.
 * @param nhcd_timestamp Pointer to NHCD timestamp string.
 * @return Timestamp, or -1 if invalid.
 */
static time_t rvth_parse_timestamp(const char *nhcd_timestamp)
{
	// Date format: "YYYYMMDD"
	// Time format: "HHMMSS"
	unsigned int ymd = 0;
	unsigned int hms = 0;
	unsigned int i;
	struct tm ymdtime;

	// Convert the date to an unsigned integer.
	for (i = 0; i < 8; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		ymd *= 10;
		ymd += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be higher than 19000101.
	// - Must be lower than 99991231.
	if (unlikely(ymd < 19000101 || ymd > 99991231)) {
		// Invalid date.
		return -1;
	}

	// Convert the time to an unsigned integer.
	for (i = 8; i < 14; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		hms *= 10;
		hms += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be lower than 235959.
	if (unlikely(hms > 235959)) {
		// Invalid time.
		return -1;
	}

	// Convert to Unix time.
	// NOTE: struct tm has some oddities:
	// - tm_year: year - 1900
	// - tm_mon: 0 == January
	ymdtime.tm_year = (ymd / 10000) - 1900;
	ymdtime.tm_mon  = ((ymd / 100) % 100) - 1;
	ymdtime.tm_mday = ymd % 100;

	ymdtime.tm_hour = (hms / 10000);
	ymdtime.tm_min  = (hms / 100) % 100;
	ymdtime.tm_sec  = hms % 100;

	// tm_wday and tm_yday are output variables.
	ymdtime.tm_wday = 0;
	ymdtime.tm_yday = 0;
	ymdtime.tm_isdst = 0;

	// If conversion fails, this will return -1.
	return timegm(&ymdtime);
}

/**
 * Find the game partition in a Wii disc image.
 * @param f_img		[in] RefFile*
 * @param lba_start	[in] Starting LBA. (Should be 0 for standalone disc images.)
 * @return Game partition LBA (relative to start of f_img), or 0 on error.
 */
uint32_t rvth_find_GamePartition(RefFile *f_img, uint32_t lba_start)
{
	// Assuming this is a valid Wii disc image.
	RVL_VolumeGroupTable vgtbl;
	RVL_PartitionTableEntry ptbl[16];
	uint64_t addr;
	unsigned int ptcount, i;
	int ret;
	size_t size;

	// Get the volume group table.
	addr = ((int64_t)lba_start * RVTH_BLOCK_SIZE) + RVL_VolumeGroupTable_ADDRESS;
	ret = ref_seeko(f_img, addr, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		return 0;
	}
	size = ref_read(&vgtbl, 1, sizeof(vgtbl), f_img);
	if (size != sizeof(vgtbl)) {
		// Read error.
		return 0;
	}

	// Game partition is always in volume group 0.
	// Read up to 16 partitions for VG0.
	// NOTE: Addresses are rshifted by 2.
	ptcount = be32_to_cpu(vgtbl.vg[0].count);
	addr = ((int64_t)lba_start * RVTH_BLOCK_SIZE) + ((int64_t)be32_to_cpu(vgtbl.vg[0].addr) << 2);
	ret = ref_seeko(f_img, addr, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		return 0;
	}
	size = ref_read(ptbl, 1, sizeof(ptbl), f_img);
	if (size != sizeof(ptbl)) {
		// Read error.
		return 0;
	}

	for (i = 0; i < ptcount; i++) {
		if (be32_to_cpu(ptbl[i].type == 0)) {
			// Found the game partition.
			return lba_start + (be32_to_cpu(ptbl[i].addr) / (RVTH_BLOCK_SIZE/4));
		}
	}

	// No game partition found...
	return 0;
}

/**
 * Initialize an RVT-H bank entry from an opened disk or disc image.
 * @param entry			[out] RvtH_BankEntry
 * @param f_img			[in] RefFile*
 * @param type			[in] Bank type. (See RvtH_BankType_e.)
 * @param lba_start		[in] Starting LBA. (Should be 0 for standalone disc images.)
 * @param lba_len		[in] Ending LBA. (If 0, and lba_start is 0, uses the file size.)
 * @param nhcd_timestamp	[in] Timestamp string pointer from the bank table.
 * @return 0 on success; negative POSIX error code on error.
 */
static int rvth_init_BankEntry(RvtH_BankEntry *entry, RefFile *f_img,
	uint8_t type, uint32_t lba_start, uint32_t lba_len,
	const char *nhcd_timestamp)
{
	GCN_DiscHeader discHeader;
	int ret;
	size_t size;

	// TODO: Handle lba_start == 0 && lba_len == 0.
	// TODO: If lba_len == 0, initialize based on type.

	// Initialize the standard properties.
	memset(entry, 0, sizeof(*entry));
	entry->f_img = ref_dup(f_img);
	entry->lba_start = lba_start;
	entry->lba_len = lba_len;
	entry->type = type;
	entry->timestamp = -1;	// Default to "no timestamp".

	assert(entry->type < RVTH_BankType_MAX);
	if (type == RVTH_BankType_Unknown ||
	    type >= RVTH_BankType_MAX)
	{
		// Unknown entry type.
		// Don't bother reading anything else.
		return 0;
	}

	// Read the GCN disc header.
	// TODO: For non-deleted banks, verify the magic number?
	errno = 0;
	ret = ref_seeko(f_img, (int64_t)lba_start * NHCD_BLOCK_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}
	size = ref_read(&discHeader, 1, sizeof(discHeader), f_img);
	if (size != sizeof(discHeader)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}

	// If the entry type is Empty, check for GCN/Wii magic.
	if (type == RVTH_BankType_Empty) {
		if (discHeader.magic_wii == cpu_to_be32(WII_MAGIC)) {
			// Wii disc image.
			// TODO: Detect SL vs. DL.
			// TODO: Actual disc image size?
			type = RVTH_BankType_Wii_SL;
			entry->is_deleted = true;
		} else if (discHeader.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
			// GameCube disc image.
			// TODO: Actual disc image size?
			type = RVTH_BankType_GCN;
			entry->is_deleted = true;
		} else {
			// Probably actually empty...
		}
		entry->type = type;
	}

	if (type == RVTH_BankType_Empty) {
		// We're done here.
		return 0;
	}

	// Copy the game ID.
	memcpy(entry->id6, discHeader.id6, 6);

	// Read the disc title.
	memcpy(entry->game_title, discHeader.game_title, 64);
	// Remove excess spaces.
	trim_title(entry->game_title, 64);

	// Parse the timestamp.
	if (nhcd_timestamp) {
		entry->timestamp = rvth_parse_timestamp(nhcd_timestamp);
	}

	// Check encryption status.
	if (type == RVTH_BankType_Wii_SL || type == RVTH_BankType_Wii_DL) {
		uint32_t game_lba;	// LBA of the Game Partition.

		if (discHeader.hash_verify != 0 && discHeader.disc_noCrypt != 0) {
			// Unencrypted.
			// TODO: I haven't seen any images where only one of these
			// is zero or non-zero, but that may show up...
			entry->crypto_type = RVTH_CryptoType_None;
		}

		// Find the game partition.
		// TODO: Error checking.
		game_lba = rvth_find_GamePartition(f_img, lba_start);
		if (game_lba != 0) {
			// Found the game partition.
			static const char issuer_retail[] = "Root-CA00000001-XS00000003";
			static const char issuer_debug[]  = "Root-CA00000002-XS00000006";

			// Read the ticket.
			RVL_Ticket ticket;
			ref_seeko(f_img, (int64_t)game_lba * RVTH_BLOCK_SIZE, SEEK_SET);
			ref_read(&ticket, 1, sizeof(ticket), f_img);

			// Check the signature issuer.
			// This indicates debug vs. retail.
			if (!memcmp(ticket.signature_issuer, issuer_retail, sizeof(issuer_retail))) {
				// Retail certificate.
				entry->sig_type = RVTH_SigType_Retail;
			} else if (!memcmp(ticket.signature_issuer, issuer_debug, sizeof(issuer_debug))) {
				// Debug certificate.
				entry->sig_type = RVTH_SigType_Debug;
			} else {
				// Unknown certificate.
				entry->sig_type = RVTH_SigType_Unknown;
			}

			// TODO: Check for invalid and/or fakesigned tickets.

			// If encrypted, set the crypto type.
			// NOTE: Retail + unencrypted is usually an error.
			if (entry->crypto_type != RVTH_CryptoType_None) {
				switch (entry->sig_type) {
					case RVTH_SigType_Retail:
						// Retail may be either Common Key or Korean Key.
						switch (ticket.common_key_index) {
							case 0:
								entry->crypto_type = RVTH_CryptoType_Retail;
								break;
							case 1:
								entry->crypto_type = RVTH_CryptoType_Korean;
								break;
							default:
								entry->crypto_type = RVTH_CryptoType_Unknown;
								break;
						}
						break;

					case RVTH_SigType_Debug:
						// There's only one debug key.
						if (ticket.common_key_index == 0) {
							entry->crypto_type = RVTH_CryptoType_Debug;
						} else {
							entry->crypto_type = RVTH_CryptoType_Unknown;
						}
						break;

					default:
						// Should not happen...
						entry->crypto_type = RVTH_CryptoType_Unknown;
						break;
				}
			}
		}
	} else {
		// GameCube is unencrypted.
		entry->crypto_type = RVTH_CryptoType_None;
	}

	// We're done here.
	return 0;
}

/**
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const TCHAR *filename, int *pErr)
{
	NHCD_BankTable_Header header;
	RefFile *f_img;
	RvtH *rvth;
	RvtH_BankEntry *rvth_entry;
	int ret;
	unsigned int i;
	int64_t addr;
	size_t size;

	// Open the disk image.
	f_img = ref_open(filename);
	if (!f_img) {
		// Could not open the file.
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}

	/**
	 * TODO: Heuristics to determine if this is RVT-H vs. individual disc.
	 * - RVT-H will be either a block device or a >20 GB disk image.
	 *   - Note that RVT-H images may start with a disc image, even though
	 *     that image is likely corrupted and unusable.
	 * - Individual disc will be <= 8.0 GB and not a block device.
	 *   - If it's CISO or WBFS, it's an individual disc.
	 */

	// Check the bank table header.
	errno = 0;
	ret = ref_seeko(f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}
	errno = 0;
	size = ref_read(&header, 1, sizeof(header), f_img);
	if (size != sizeof(header)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Check the magic number.
	if (header.magic != be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Incorrect magic number.
		ref_close(f_img);
		if (pErr) {
			*pErr = RVTH_ERROR_NHCD_TABLE_MAGIC;
		}
		errno = EIO;
		return NULL;
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Allocate memory for the 8 RvtH_BankEntry objects.
	rvth->banks = 8;
	rvth->entries = calloc(8, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		ref_close(f_img);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	rvth->f_img = f_img;
	rvth_entry = rvth->entries;
	addr = (uint32_t)(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA) + NHCD_BLOCK_SIZE);
	for (i = 0; i < rvth->banks; i++, rvth_entry++, addr += 512) {
		NHCD_BankEntry nhcd_entry;
		uint32_t lba_start = 0, lba_len = 0;
		uint8_t type = RVTH_BankType_Unknown;

		if (i > 0 && (rvth_entry-1)->type == RVTH_BankType_Wii_DL) {
			// Second bank for a dual-layer Wii image.
			rvth_entry->type = RVTH_BankType_Wii_DL_Bank2;
			rvth_entry->timestamp = -1;
			continue;
		}

		errno = 0;
		ret = ref_seeko(f_img, addr, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		errno = 0;
		size = ref_read(&nhcd_entry, 1, sizeof(nhcd_entry), f_img);
		if (size != sizeof(nhcd_entry)) {
			// Short read.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		// Check the type.
		switch (be32_to_cpu(nhcd_entry.type)) {
			default:
				// Unknown bank type...
				type = RVTH_BankType_Unknown;
				break;
			case 0:
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
			// Invalid LBAs. Use the default values.
			lba_start = NHCD_BANK_1_START_LBA + (NHCD_BANK_SIZE_LBA * i);
			lba_len = NHCD_BANK_SIZE_LBA;
		}

		// Initialize the bank entry.
		rvth_init_BankEntry(rvth_entry, f_img, type,
			lba_start, lba_len, nhcd_entry.timestamp);
	}

	// RVT-H image loaded.
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
	for (i = 0; i < rvth->banks; i++) {
		if (rvth->entries[i].f_img) {
			ref_close(rvth->entries[i].f_img);
		}
	}

	// Free the bank entries array.
	free(rvth->entries);

	// Clear the main reference.
	if (rvth->f_img) {
		ref_close(rvth->f_img);
	}

	free(rvth);
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

	return rvth->banks;
}

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range or empty.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr)
{
	if (!rvth) {
		errno = EINVAL;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	} else if (bank >= rvth->banks) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	}

	// Check if the bank is empty.
	if (rvth->entries[bank].type == RVTH_BankType_Empty) {
		if (pErr) {
			*pErr = RVTH_ERROR_BANK_EMPTY;
		}
		return NULL;
	}

	return &rvth->entries[bank];
}

/**
 * Check if a block is empty.
 * @param block Block.
 * @param size Block size. (Must be a multiple of 64 bytes.)
 * @return True if the block is all zeroes; false if not.
 */
static bool is_block_empty(const uint8_t *block, unsigned int size)
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
 * Extract a disc image from the RVT-H disk image.
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return 0 on success; negative POSIX error code or positive RVT-H error code on error.
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback)
{
	FILE *f_extract;
	const RvtH_BankEntry *entry;
	uint8_t *buf;
	unsigned int lba_count, lba_progress;
	unsigned int lba_nonsparse;	// Last LBA written that wasn't sparse.
	unsigned int sprs;		// Sparse counter.
	int ret;

#ifdef _WIN32
	wchar_t root_dir[4];		// Root directory.
	wchar_t *p_root_dir;		// Pointer to root_dir, or NULL if relative.
	DWORD dwFileSystemFlags;	// Flags from GetVolumeInformation().
	BOOL bRet;
#endif /* _WIN32 */

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->banks) {
		errno = ERANGE;
		return -ERANGE;
	}

	// Check if the bank can be extracted.
	entry = &rvth->entries[bank];
	switch (entry->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be extracted.
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

	// Seek to the start of the disc image in the RVT-H.
	errno = 0;
	ret = ref_seeko(rvth->f_img, (int64_t)entry->lba_start * NHCD_BLOCK_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		return -errno;
	}

	// Process 1 MB at a time.
	#define BUF_SIZE 1048576
	#define LBA_COUNT_BUF (BUF_SIZE / NHCD_BLOCK_SIZE)
	errno = 0;
	buf = malloc(BUF_SIZE);
	if (!buf) {
		// Error allocating memory.
		return -errno;
	}

	errno = 0;
	f_extract = _tfopen(filename, _T("wb"));
	if (!f_extract) {
		// Error opening the file.
		int err = errno;
		free(buf);
		errno = err;
		return -err;
	}

#if defined(_WIN32)
	// Check if the file system supports sparse files.
	// TODO: Handle mount points?
	if (_istalpha(filename[0]) && filename[1] == _T(':') &&
	    (filename[2] == _T('\\') || filename[2] == _T('/')))
	{
		// Absolute pathname.
		root_dir[0] = filename[0];
		root_dir[1] = L':';
		root_dir[2] = L'\\';
		root_dir[3] = 0;
		p_root_dir = root_dir;
	}
	else
	{
		// Relative pathname.
		p_root_dir = NULL;
	}

	bRet = GetVolumeInformation(p_root_dir, NULL, 0, NULL, NULL,
		&dwFileSystemFlags, NULL, 0);
	if (bRet != 0 && (dwFileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)) {
		// File system supports sparse files.
		// Mark the file as sparse.
		HANDLE h_extract = (HANDLE)_get_osfhandle(_fileno(f_extract));
		if (h_extract != NULL && h_extract != INVALID_HANDLE_VALUE) {
			DWORD bytesReturned;
			FILE_SET_SPARSE_BUFFER fssb;
			fssb.SetSparse = TRUE;

			// TODO: Use SetEndOfFile() if this succeeds?
			// Note that SetEndOfFile() isn't guaranteed to fill the
			// resulting file with zero bytes...
			DeviceIoControl(h_extract, FSCTL_SET_SPARSE,
				&fssb, sizeof(fssb),
				NULL, 0, &bytesReturned, NULL);
		}
	}
#elif defined(HAVE_FTRUNCATE)
	// Set the file size.
	// NOTE: If the underlying file system doesn't support sparse files,
	// this may take a long time. (TODO: Check this.)
	ret = ftruncate(fileno(f_extract), (int64_t)entry->lba_len * RVTH_BLOCK_SIZE);
	if (ret != 0) {
		// Error setting the file size.
		// Allow all errors except for EINVAL or EFBIG,
		// since those mean the file system doesn't support
		// large files.
		int err = errno;
		if (err == EINVAL || err == EFBIG) {
			// File is too big.
			// TODO: Delete the file?
			fclose(f_extract);
			free(buf);
			errno = err;
			return -err;
		}
	}
#endif /* HAVE_FTRUNCATE */

	lba_progress = 0;
	lba_nonsparse = 0;
	for (lba_count = entry->lba_len; lba_count >= LBA_COUNT_BUF;
	     lba_count -= LBA_COUNT_BUF)
	{
		// TODO: Error handling.
		ref_read(buf, 1, BUF_SIZE, rvth->f_img);

		// Check for empty 4 KB blocks.
		for (sprs = 0; sprs < BUF_SIZE; sprs += 4096) {
			if (is_block_empty(&buf[sprs], 4096)) {
				// 4 KB block is empty.
				fseeko(f_extract, 4096, SEEK_CUR);
			} else {
				// 4 KB block is not empty.
				fwrite(&buf[sprs], 1, 4096, f_extract);
				lba_nonsparse = lba_progress + (sprs / 512) + 7;
			}
		}

		if (callback) {
			lba_progress += LBA_COUNT_BUF;
			callback(lba_progress, entry->lba_len);
		}
	}

	// Process any remaining LBAs.
	if (lba_count > 0) {
		const unsigned int sz_left = lba_count * RVTH_BLOCK_SIZE;
		ref_read(buf, 1, sz_left, rvth->f_img);

		// Check for empty 512-byte blocks.
		for (sprs = 0; sprs < sz_left; sprs += 512) {
			if (is_block_empty(&buf[sprs], 512)) {
				// 512-byte block is empty.
				fseeko(f_extract, 512, SEEK_CUR);
			} else {
				// 512-byte block is not empty.
				fwrite(&buf[sprs], 1, 512, f_extract);
				lba_nonsparse = lba_progress + (sprs / 512) + 7;
			}
		}

		if (callback) {
			callback(entry->lba_len, entry->lba_len);
		}
	}

	// lba_nonsparse should be equal to entry->lba_len - 1.
	if (lba_nonsparse != entry->lba_len - 1) {
		// Last LBA was sparse.
		// We'll need to write an actual zero block.
		// TODO: Maybe not needed if ftruncate() succeeded?
		// TODO: Check for errors.
		int64_t last_lba_pos = (int64_t)(entry->lba_len - 1) * RVTH_BLOCK_SIZE;
		memset(buf, 0, 512);
		fseeko(f_extract, last_lba_pos, SEEK_SET);
		fwrite(buf, 1, 512, f_extract);
	}

	// Finished extracting the disc image.
	fflush(f_extract);
	fclose(f_extract);
	free(buf);
	return 0;
}

/**
 * Make an RVT-H object writable.
 * @param rvth	[in] RVT-H disk image.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_make_writable(RvtH *rvth)
{
	if (ref_is_writable(rvth->f_img)) {
		// RVT-H is already writable.
		return 0;
	}

	// TODO: Allow making a disc image file writable.
	// (Single bank)

	// Make sure this is a device file.
	if (!ref_is_device(rvth->f_img)) {
		// This is not a device file.
		// Cannot make it writable.
		return RVTH_ERROR_NOT_A_DEVICE;
	}

	// Make this writable.
	return ref_make_writable(rvth->f_img);
}

/**
 * Delete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_delete(RvtH *rvth, unsigned int bank)
{
	int ret;
	size_t size;
	uint8_t buf[RVTH_BLOCK_SIZE];

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->banks) {
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Is the bank deleted?
	RvtH_BankEntry *entry = &rvth->entries[bank];
	if (entry->is_deleted) {
		// Bank is already deleted.
		return RVTH_ERROR_BANK_IS_DELETED;
	}

	// Check the bank type.
	switch (entry->type) {
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

	// Clear the bank data on disk.
	memset(buf, 0, sizeof(buf));
	ret = ref_seeko(entry->f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA + bank+1), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}
	size = ref_write(buf, 1, sizeof(buf), entry->f_img);
	if (size != sizeof(buf)) {
		// Write error.
		if (errno = 0) {
			errno = EIO;
		}
		return -errno;
	}

	// Bank is deleted.
	entry->is_deleted = true;
	return 0;
}
