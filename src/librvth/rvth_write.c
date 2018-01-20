/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.c: RVT-H write functions.                                          *
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
#include "rvth_p.h"

#include "byteswap.h"
#include "nhcd_structs.h"

#include <assert.h>
#include <errno.h>
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

#ifdef _WIN32
	wchar_t root_dir[4];		// Root directory.
	wchar_t *p_root_dir;		// Pointer to root_dir, or NULL if relative.
	DWORD dwFileSystemFlags;	// Flags from GetVolumeInformation().
	BOOL bRet;
#else /* !_WIN32 */
	int ret;
#endif /* _WIN32 */

	if (!rvth || !filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->bank_count) {
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
		reader_read(entry->reader, buf, lba_progress, LBA_COUNT_BUF);

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
		reader_read(entry->reader, buf, lba_progress, lba_count);

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
 * Write a bank table entry to disk.
 * @param rvth	[in] RvtH device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_write_BankEntry(RvtH *rvth, unsigned int bank)
{
	int ret;
	size_t size;
	NHCD_BankEntry nhcd_entry;
	RvtH_BankEntry *rvth_entry;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (!rvth->is_hdd) {
		// Standalone disc image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		return ret;
	}

	// Create a new NHCD bank entry.
	memset(&nhcd_entry, 0, sizeof(nhcd_entry));

	// Check the bank type.
	rvth_entry = &rvth->entries[bank];
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

	if (rvth_entry->type != RVTH_BankType_Empty &&
	    !rvth_entry->is_deleted)
	{
		// Non-empty/non-deleted bank.
		time_t now;
		struct tm tm_now;

		// Timestamp buffer.
		// We can't snprintf() directly to nhcd_entry.timestamp because
		// gcc will complain about buffer overflows, and will crash at
		// runtime on Gentoo Hardened.
		char tsbuf[16];

		// ASCII zero bytes.
		memset(nhcd_entry.all_zero, '0', sizeof(nhcd_entry.all_zero));

		// Timestamp.
		// TODO: Use localtime_r().
		now = time(NULL);
		tm_now = *localtime(&now);
		snprintf(tsbuf, sizeof(tsbuf), "%04d%02d%02d%02d%02d%02d",
			tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday,
			tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
		memcpy(nhcd_entry.timestamp, tsbuf, sizeof(nhcd_entry.timestamp));

		// LBA start and length.
		nhcd_entry.lba_start = cpu_to_be32(rvth_entry->lba_start);
		nhcd_entry.lba_len = cpu_to_be32(rvth_entry->lba_len);
	}

	// Write the bank entry.
	ret = ref_seeko(rvth->f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA + bank+1), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return -errno;
	}
	size = ref_write(&nhcd_entry, 1, sizeof(nhcd_entry), rvth->f_img);
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

/**
 * Delete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_delete(RvtH *rvth, unsigned int bank)
{
	RvtH_BankEntry *rvth_entry;
	int ret;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (rvth->is_hdd) {
		// Standalone disc image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
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
	rvth_entry = &rvth->entries[bank];
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
	ret = rvth_write_BankEntry(rvth, bank);
	if (ret != 0) {
		// Error deleting the bank...
		rvth_entry->is_deleted = false;
	}
	return ret;
}

/**
 * Undelete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_undelete(RvtH *rvth, unsigned int bank)
{
	RvtH_BankEntry *rvth_entry;
	int ret;

	if (!rvth) {
		errno = EINVAL;
		return -EINVAL;
	} else if (rvth->is_hdd) {
		// Standalone disc image. No bank table.
		errno = EINVAL;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
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
	rvth_entry = &rvth->entries[bank];
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

	// Delete the bank and write the entry.
	rvth_entry->is_deleted = false;
	ret = rvth_write_BankEntry(rvth, bank);
	if (ret != 0) {
		// Error undeleting the bank...
		rvth_entry->is_deleted = true;
	}
	return ret;
}
