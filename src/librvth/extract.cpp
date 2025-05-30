/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * extract.cpp: RVT-H extract and import functions.                        *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_p.hpp"
#include "rvth_error.h"

#include "ptbl.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// Disc image reader.
#include "reader/Reader.hpp"

// libwiicrypto
#include "libwiicrypto/sig_tools.h"

// C includes
#include <stdlib.h>
#include <sys/stat.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>
#include <ctime>

// C++ includes
#include <memory>
#include <string>
using std::unique_ptr;
using std::string;
using std::wstring;

// for disk free space
#ifdef _WIN32
#  include <windows.h>
#else /* !_WIN32 */
#  include <sys/statvfs.h>
#endif /* _WIN32 */

// Buffer size
static constexpr unsigned int BUF_SIZE = 1U * 1024U * 1024U;
static constexpr unsigned int LBA_COUNT_BUF = BYTES_TO_LBA(BUF_SIZE);

/**
 * Get the free disk space on the volume containing `filename`.
 * @param filename Filename.
 * @return Free disk space, in LBAs.
 */
static int64_t getDiskFreeSpace_lba(const TCHAR *filename)
{
	tstring path(filename);
	int64_t freeSpace_lba = 0;
	int ret;

	// Check if the file exists already.
	// If it does, it will be overwritten, so we can
	// add its size to the free space.
#ifdef _WIN32
	struct _stati64 sbuf;
	ret = ::_tstati64(filename, &sbuf);
#else /* !_WIN32 */
	struct stat sbuf;
	ret = ::stat(filename, &sbuf);
#endif /* _WIN32 */
	if (ret == 0) {
		// POSIX has st_blocks, which is *allocated* 512-byte blocks.
		// Windows does not, so use the full size on Windows.
		// TODO: CMake check? (https://cmake.org/cmake/help/v3.0/module/CheckStructHasMember.html)
#ifdef _WIN32
		freeSpace_lba  = static_cast<int64_t>(sbuf.st_size) / 512;
		freeSpace_lba += (sbuf.st_size % 512 != 0);
#else /* !_WIN32 */
		freeSpace_lba  = static_cast<int64_t>(sbuf.st_blocks);
#endif /* _WIN32 */
	}

#ifdef _WIN32
	// Remove the filename portion.
	size_t slash_pos = path.rfind(_T('\\'));
	if (slash_pos != string::npos && slash_pos != path.size()-1) {
		path.resize(slash_pos + 1);
	} else {
		// No filename. Assume it's a relative path,
		// and use the current directory.
		path = _T(".");
	}

	ULARGE_INTEGER freeBytesAvailableToCaller;
	BOOL bRet = GetDiskFreeSpaceEx(path.c_str(),
		&freeBytesAvailableToCaller,
		nullptr, nullptr);
	if (!bRet) {
		// TODO: Convert Win32 error code to POSIX.
		//DWORD dwErr = GetLastError();
		return -EIO;
	}

	// Convert the free space to RVT-H LBAs.
	freeSpace_lba += (static_cast<int64_t>(freeBytesAvailableToCaller.QuadPart) / LBA_SIZE);
#else /* !_WIN32 */
	// Remove the filename portion.
	size_t slash_pos = path.rfind(_T('/'));
	if (slash_pos != string::npos && slash_pos != path.size()-1) {
		path.resize(slash_pos + 1);
	} else {
		// No filename. Assume it's a relative path,
		// and use the current directory.
		path = _T(".");
	}

	struct statvfs svbuf;
	ret = statvfs(path.c_str(), &svbuf);
	if (ret != 0) {
		// Error...
		return -errno;
	}

	// Convert the free space from file system blocks to RVT-H LBAs.
	freeSpace_lba += (static_cast<int64_t>(svbuf.f_bavail) * (svbuf.f_frsize / LBA_SIZE));
#endif /* _WIN32 */

	return freeSpace_lba;
}

/**
 * Copy a bank from this RVT-H HDD or standalone disc image to a writable standalone disc image.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::copyToGcm(RvtH *rvth_dest, unsigned int bank_src, RvtH_Progress_Callback callback, void *userdata)
{
	uint32_t lba_copy_len;	// Total number of LBAs to copy. (entry_src->lba_len)
	uint32_t lba_count;
	uint32_t lba_buf_max;	// Highest LBA that can be written using the buffer.
	uint32_t lba_nonsparse;	// Last LBA written that wasn't sparse.

	// Callback state.
	RvtH_Progress_State state;

	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	// Destination disc image.
	RvtH_BankEntry *entry_dest;

	if (!rvth_dest) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank_src >= bankCount()) {
		errno = ERANGE;
		return -ERANGE;
	} else if (rvth_dest->isHDD() || rvth_dest->bankCount() != 1) {
		// Destination is not a standalone disc image.
		// Copying to HDDs will be handled differently.
		errno = EIO;
		return RVTH_ERROR_IS_HDD_IMAGE;
	}

	// Check if the source bank can be extracted.
	const RvtH_BankEntry *const entry_src = &d_ptr->entries[bank_src];
	switch (entry_src->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be extracted.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			errno = EIO;
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			errno = ENOENT;
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			errno = EIO;
			return RVTH_ERROR_BANK_DL_2;
	}

	// Allocate the memory buffer.
	uint8_t *const buf = (uint8_t*)malloc(BUF_SIZE);
	if (!buf) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto end;
	}

	// FIXME: If the file existed and wasn't 0 bytes,
	// either truncate it or don't do sparse writes.

	// Make this a sparse file.
	entry_dest = &rvth_dest->d_ptr->entries[0];
	ret = rvth_dest->d_ptr->file->makeSparse(LBA_TO_BYTES(entry_dest->lba_len));
	if (ret != 0) {
		// Error managing the sparse file.
		// TODO: Delete the file?
		err = rvth_dest->d_ptr->file->lastError();
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto end;
	}

	// Copy the bank table information.
	entry_dest->type	= entry_src->type;
	entry_dest->region_code	= entry_src->region_code;
	entry_dest->is_deleted	= false;
	entry_dest->crypto_type	= entry_src->crypto_type;
	entry_dest->ios_version	= entry_src->ios_version;
	entry_dest->ticket	= entry_src->ticket;
	entry_dest->tmd		= entry_src->tmd;

	// Copy the disc header.
	memcpy(&entry_dest->discHeader, &entry_src->discHeader, sizeof(entry_dest->discHeader));

	// Timestamp.
	if (entry_src->timestamp >= 0) {
		entry_dest->timestamp = entry_src->timestamp;
	} else {
		entry_dest->timestamp = time(nullptr);
	}

	// Number of LBAs to copy.
	lba_copy_len = entry_src->lba_len;

	if (callback) {
		// Initialize the callback state.
		state.rvth = this;
		state.rvth_gcm = rvth_dest;
		state.bank_rvth = bank_src;
		state.bank_gcm = 0;
		state.type = RVTH_PROGRESS_EXTRACT;
		state.lba_processed = 0;
		state.lba_total = lba_copy_len;
	}

	// TODO: Optimize seeking? (Reader::write() seeks every time.)
	lba_buf_max = entry_dest->lba_len & ~(LBA_COUNT_BUF-1);
	lba_nonsparse = 0;
	for (lba_count = 0; lba_count < lba_buf_max; lba_count += LBA_COUNT_BUF) {
		if (callback) {
			bool bRet;
			state.lba_processed = lba_count;
			bRet = callback(&state, userdata);
			if (!bRet) {
				// Stop processing.
				err = ECANCELED;
				ret = -ECANCELED;
				goto end;
			}
		}

		// TODO: Error handling.
		entry_src->reader->read(buf, lba_count, LBA_COUNT_BUF);

		if (lba_count == 0) {
			// Make sure we copy the disc header in if the
			// header was zeroed by the RVT-H's "Flush" function.
			// TODO: Move this outside of the `for` loop.
			// TODO: Also check for NDDEMO?
			const GCN_DiscHeader *const origHdr = (const GCN_DiscHeader*)buf;
			if (origHdr->magic_wii != be32_to_cpu(WII_MAGIC) &&
			    origHdr->magic_gcn != be32_to_cpu(GCN_MAGIC))
			{
				// Missing magic number. Need to restore the disc header.
				memcpy(buf, &entry_src->discHeader, sizeof(entry_src->discHeader));
			}
		}

		// Check for empty 4 KB blocks.
		for (unsigned int sprs = 0; sprs < BUF_SIZE; sprs += 4096) {
			if (!d_ptr->isBlockEmpty(&buf[sprs], 4096)) {
				// 4 KB block is not empty.
				lba_nonsparse = lba_count + (sprs / 512);
				entry_dest->reader->write(&buf[sprs], lba_nonsparse, 8);
				//entry_dest->reader->flush();
				lba_nonsparse += 7;
			}
		}
	}

	// Process any remaining LBAs.
	if (lba_count < lba_copy_len) {
		const unsigned int lba_left = lba_copy_len - lba_count;
		const unsigned int sz_left = static_cast<unsigned int>(LBA_TO_BYTES(lba_left));

		if (callback) {
			bool bRet;
			state.lba_processed = lba_count;
			bRet = callback(&state, userdata);
			if (!bRet) {
				// Stop processing.
				err = ECANCELED;
				ret = -ECANCELED;
				goto end;
			}
		}
		entry_src->reader->read(buf, lba_count, lba_left);

		// Check for empty 512-byte blocks.
		for (unsigned int sprs = 0; sprs < sz_left; sprs += 512) {
			if (!d_ptr->isBlockEmpty(&buf[sprs], 512)) {
				// 512-byte block is not empty.
				lba_nonsparse = lba_count + (sprs / 512);
				entry_dest->reader->write(&buf[sprs], lba_nonsparse, 1);
				//entry_dest->reader->flush();
			}
		}
	}

	if (callback) {
		bool bRet;
		state.lba_processed = lba_copy_len;
		bRet = callback(&state, userdata);
		if (!bRet) {
			// Stop processing.
			err = ECANCELED;
			ret = -ECANCELED;
			goto end;
		}
	}

	// lba_nonsparse should be equal to lba_copy_len-1.
	if (lba_nonsparse != lba_copy_len-1) {
		// Last LBA was sparse.
		// We'll need to write an actual zero block.
		// TODO: Maybe not needed if ftruncate() succeeded?
		// TODO: Check for errors.
		memset(buf, 0, 512);
		entry_dest->reader->write(buf, lba_copy_len-1, 1);
		//entry_dest->reader->flush();
	}

	// Flush the destination device.
	entry_dest->reader->flush();

end:
	free(buf);
	if (err != 0) {
		errno = err;
	}
	return ret;
}

/**
 * Extract a disc image from this RVT-H disk image.
 * Compatibility wrapper; this function creates a new RvtH
 * using the GCM constructor and then copyToGcm().
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param recrypt_key	[in] Key for recryption. (-1 for default; otherwise, see RVL_CryptoType_e)
 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::extract(unsigned int bank, const TCHAR *filename,
	int recrypt_key, unsigned int flags, RvtH_Progress_Callback callback, void *userdata)
{
	if (!filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= bankCount()) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// TODO: If recryption is needed, validate parts of the partitions,
	// e.g. certificate chain length, before copying.

	// TODO: If recrypt_key == the original key,
	// handle it as -1.

	// Create a standalone disc image.
	RvtH_BankEntry *const entry = &d_ptr->entries[bank];
	const bool unenc_to_enc = (entry->type >= RVTH_BankType_Wii_SL &&
				   entry->crypto_type == RVL_CryptoType_None &&
				   recrypt_key > RVL_CryptoType_Unknown);
	uint32_t gcm_lba_len;
	if (unenc_to_enc) {
		// Converting from unencrypted to encrypted.
		// Need to convert 31k sectors to 32k.
		uint32_t lba_tmp;
		const pt_entry_t *game_pte = rvth_ptbl_find_game(entry);
		if (!game_pte) {
			// No game partition...
			errno = EIO;
			return RVTH_ERROR_NO_GAME_PARTITION;
		}

		// TODO: Read the partition header to determine the data offset.
		// Assuming 0x8000 partition header size for now.
		lba_tmp = game_pte->lba_len - BYTES_TO_LBA(0x8000);
		gcm_lba_len = (lba_tmp / 3968 * 4096);
		if (lba_tmp % 3968 != 0) {
			gcm_lba_len += 4096;
		}
		// Assuming 0x8000 header + 0x18000 H3 table.
		gcm_lba_len += BYTES_TO_LBA(0x20000) + game_pte->lba_start;
	} else {
		// Use the bank size as-is.
		gcm_lba_len = entry->lba_len;
	}

	if (flags & RVTH_EXTRACT_PREPEND_SDK_HEADER) {
		if (entry->type == RVTH_BankType_GCN) {
			// FIXME: Not supported.
			errno = ENOTSUP;
			return RVTH_ERROR_NDEV_GCN_NOT_SUPPORTED;
		}
		// Prepend 32k to the GCM.
		gcm_lba_len += BYTES_TO_LBA(32768);
	}

	// Check that we have enough free disk space.
	// NOTE: We're not checking for sparse sectors.
	int64_t diskFreeSpace_lba = getDiskFreeSpace_lba(filename);
	if (diskFreeSpace_lba < 0) {
		// Error...
		int ret = static_cast<int>(diskFreeSpace_lba);
		errno = -ret;
		return ret;
	} else if (diskFreeSpace_lba < gcm_lba_len) {
		// Not enough free disk space.
		errno = ENOSPC;
		return -ENOSPC;
	}

	int ret = 0;
	unique_ptr<RvtH> rvth_dest(new RvtH(filename, gcm_lba_len, &ret));
	if (!rvth_dest->isOpen()) {
		// Error creating the standalone disc image.
		errno = EIO;
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	}

	if (flags & RVTH_EXTRACT_PREPEND_SDK_HEADER) {
		// Prepend 32k to the GCM.
		size_t size;
		Reader *const reader = rvth_dest->d_ptr->entries[0].reader;
		uint8_t *const sdk_header = static_cast<uint8_t*>(calloc(1, SDK_HEADER_SIZE_BYTES));
		if (!sdk_header) {
			int ret = -errno;
			if (ret == 0) {
				ret = -ENOMEM;
			}
			return ret;
		}

		// TODO: Get headers for GC1L and NN2L.
		// TODO: Optimize by using 32-bit writes?
		switch (entry->type) {
			case RVTH_BankType_GCN:
				// FIXME; GameCube GCM seems to use the same values,
				// but it doesn't load with NDEV.
				// Checksum field is always 0xAB0B.
				// TODO: Delete the file?
				return RVTH_ERROR_NDEV_GCN_NOT_SUPPORTED;
			case RVTH_BankType_Wii_SL:
			case RVTH_BankType_Wii_DL:
				// 0x0000: FF FF 00 00
				sdk_header[0x0000] = 0xFF;
				sdk_header[0x0001] = 0xFF;
				// 0x082C: 00 00 E0 06
				sdk_header[0x082E] = 0xE0;
				sdk_header[0x082F] = 0x06;
				// TODO: Checksum at 0x0830? (If 00 00, seems to work for all discs.)
				// 0x0844: 01 00 00 00
				sdk_header[0x0844] = 0x01;
				break;
			default:
				// Should not get here...
				assert(!"Incorrect bank type.");
				free(sdk_header);
				return RVTH_ERROR_BANK_UNKNOWN;
		}

		size = reader->write(sdk_header, 0, SDK_HEADER_SIZE_LBA);
		reader->flush();
		if (size != SDK_HEADER_SIZE_LBA) {
			// Write error.
			int ret = -errno;
			if (ret == 0) {
				ret = -EIO;
			}
			free(sdk_header);
			return ret;
		}
		free(sdk_header);

		// Remove the SDK header from the reader's offsets.
		reader->lba_adjust(SDK_HEADER_SIZE_LBA);
	}

	// Copy the bank from the source image to the destination GCM.
	if (unenc_to_enc) {
		ret = copyToGcm_doCrypt(rvth_dest.get(), bank, callback, userdata);
	} else {
		ret = copyToGcm(rvth_dest.get(), bank, callback, userdata);
	}
	if (ret == 0 && recrypt_key > RVL_CryptoType_Unknown) {
		// Recrypt the disc image.
		if (entry->crypto_type != recrypt_key) {
			ret = rvth_dest->recryptWiiPartitions(0,
				static_cast<RVL_CryptoType_e>(recrypt_key), callback, userdata);
		}
	}

	return ret;
}

/**
 * Copy a bank from this HDD or standalone disc image to an RVT-H system.
 * @param rvth_dest	[in] Destination RvtH object.
 * @param bank_dest	[in] Destination bank number. (0-7)
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::copyToHDD(RvtH *rvth_dest, unsigned int bank_dest,
	unsigned int bank_src, RvtH_Progress_Callback callback, void *userdata)
{
	uint32_t lba_copy_len;	// Total number of LBAs to copy. (entry_src->lba_len)
	uint32_t lba_count;
	uint32_t lba_buf_max;	// Highest LBA that can be written using the buffer.

	// Callback state
	RvtH_Progress_State state;

	int ret = 0;	// errno or RvtH_Errors

	if (!rvth_dest) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank_src >= bankCount() ||
		   bank_dest >= rvth_dest->bankCount())
	{
		errno = ERANGE;
		return -ERANGE;
	} else if (!rvth_dest->isHDD()) {
		// Destination is not an HDD.
		errno = EIO;
		return RVTH_ERROR_NOT_HDD_IMAGE;
	}

	// Check if the source bank can be imported.
	const RvtH_BankEntry *const entry_src = &d_ptr->entries[bank_src];
	switch (entry_src->type) {
		case RVTH_BankType_GCN:
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be imported.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			errno = EIO;
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			errno = ENOENT;
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			errno = EIO;
			return RVTH_ERROR_BANK_DL_2;
	}

	// Get the bank count of the destination RVT-H device.
	const unsigned int bank_count_dest = rvth_dest->bankCount();
	// Destination bank entry.
	RvtH_BankEntry *const entry_dest = &rvth_dest->d_ptr->entries[bank_dest];

	// Source image length cannot be larger than a single bank.
	RvtH_BankEntry *entry_dest2 = nullptr;
	if (entry_src->type == RVTH_BankType_Wii_DL) {
		// Special cases for DL:
		// - Destination bank must not be the last bank.
		// - For extended bank tables, destination bank must not be the first bank.
		// - Both the selected bank and the next bank must be empty or deleted.
		if (bank_count_dest > 8) {
			// Extended bank table.
			if (bank_dest == 0) {
				// Cannot use bank 0.
				errno = EINVAL;
				return RVTH_ERROR_IMPORT_DL_EXT_NO_BANK1;
			}
		}

		// Cannot use the last bank for DL images.
		if (bank_dest == bank_count_dest-1) {
			errno = EINVAL;
			return RVTH_ERROR_IMPORT_DL_LAST_BANK;
		}

		// Check that the first bank is empty or deleted.
		// NOTE: Checked below, but we should check this before
		// checking the second bank.
		if (entry_dest->type != RVTH_BankType_Empty &&
		    !entry_dest->is_deleted)
		{
			errno = EEXIST;
			return RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED;
		}

		// Check that the second bank is empty or deleted.
		entry_dest2 = &rvth_dest->d_ptr->entries[bank_dest+1];
		if (entry_dest2->type != RVTH_BankType_Empty &&
		    !entry_dest2->is_deleted)
		{
			errno = EEXIST;
			return RVTH_ERROR_BANK2DL_NOT_EMPTY_OR_DELETED;
		}

		// Verify that the two banks are contiguous.
		// FIXME: This should always be the case except for bank 1 on
		// devices with non-extended bank tables. lba_len is reduced
		// if the bank originally had a GameCube image, so we can't
		// check this right now.
		/*if (entry_dest->lba_start + entry_dest->lba_len != entry_dest2->lba_start) {
			// Not contiguous.
			errno = EIO;
			return RVTH_ERROR_IMPORT_DL_NOT_CONTIGUOUS;
		}*/

		// Verify that the image fits in two banks.
		if (entry_src->lba_len > NHCD_BANK_SIZE_LBA*2) {
			// Image is too big.
			errno = ENOSPC;
			return RVTH_ERROR_IMAGE_TOO_BIG;
		}
	} else if (entry_src->lba_len > NHCD_BANK_SIZE_LBA) {
		// Single-layer image is too big for this bank.
		errno = ENOSPC;
		return RVTH_ERROR_IMAGE_TOO_BIG;
	} else if (bank_dest == 0) {
		// Special handling for bank 1 if the bank table is extended.
		// TODO: entry_dest->lba_len should be the full bank size
		// if the bank is empty or deleted.
		// TODO: Add a separate field, lba_max_len?
		if (bank_count_dest > 8) {
			// Image cannot be larger than NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA.
			if (entry_src->lba_len > NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA) {
				errno = ENOSPC;
				return RVTH_ERROR_IMAGE_TOO_BIG;
			}
		}
	}

	// Destination bank must be either empty or deleted.
	if (entry_dest->type != RVTH_BankType_Empty &&
	    !entry_dest->is_deleted)
	{
		errno = EEXIST;
		return RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED;
	}

	// Make the destination RVT-H object writable.
	ret = rvth_dest->d_ptr->makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		int err;
		if (ret < 0) {
			err = -ret;
		} else {
			err = EROFS;
		}
		errno = err;
		return err;
	}

	// Reset the reader for the bank.
	if (entry_dest->reader) {
		delete entry_dest->reader;
		entry_dest->reader = nullptr;
	}
	// NOTE: Using the source LBA length, since we might be
	// importing a dual-layer Wii image.
	entry_dest->reader = Reader::open(rvth_dest->d_ptr->file,
		entry_dest->lba_start, entry_src->lba_len);
	if (!entry_dest->reader) {
		// Cannot create a reader...
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		return err;
	}

	if (entry_dest2) {
		// Clear the second bank entry.
		if (entry_dest2->reader) {
			delete entry_dest2->reader;
			entry_dest2->reader = nullptr;
		}
		entry_dest2->timestamp = -1;
		entry_dest2->type = RVTH_BankType_Wii_DL_Bank2;
		entry_dest2->region_code = 0xFF;
		entry_dest2->is_deleted = false;
		free(entry_dest2->ptbl);
		entry_dest2->ptbl = nullptr;

		// NOTE: We don't need to write the second bank table entry for,
		// DL images, since it should already be empty and/or deleted.
		// It has to be updated in memory for qrvthtool, though.
	}

	// Allocate the memory buffer.
	unique_ptr<uint8_t[]> buf(new uint8_t[BUF_SIZE]);

	// Copy the bank table information.
	entry_dest->lba_len	= entry_src->lba_len;
	entry_dest->type	= entry_src->type;
	entry_dest->region_code	= entry_src->region_code;
	entry_dest->is_deleted	= false;
	entry_dest->crypto_type	= entry_src->crypto_type;
	entry_dest->ios_version	= entry_src->ios_version;
	entry_dest->ticket	= entry_src->ticket;
	entry_dest->tmd		= entry_src->tmd;

	// Copy the disc header.
	memcpy(&entry_dest->discHeader, &entry_src->discHeader, sizeof(entry_dest->discHeader));

	// Timestamp.
	if (entry_src->timestamp >= 0) {
		entry_dest->timestamp = entry_src->timestamp;
	} else {
		entry_dest->timestamp = time(nullptr);
	}

	// NOTE: We're only writing up to the source image file size.
	// There's no point in wiping the rest of the bank.
	lba_copy_len = entry_src->lba_len;

	if (callback) {
		// Initialize the callback state.
		state.rvth = rvth_dest;
		state.rvth_gcm = this;
		state.bank_rvth = bank_dest;
		state.bank_gcm = bank_src;
		state.type = RVTH_PROGRESS_IMPORT;
		state.lba_processed = 0;
		state.lba_total = lba_copy_len;
	}

	// TODO: Optimize seeking? (Reader::write() seeks every time.)
	lba_buf_max = entry_dest->lba_len & ~(LBA_COUNT_BUF-1);
	for (lba_count = 0; lba_count < lba_buf_max; lba_count += LBA_COUNT_BUF) {
		if (callback) {
			bool bRet;
			state.lba_processed = lba_count;
			bRet = callback(&state, userdata);
			if (!bRet) {
				// Stop processing.
				errno = ECANCELED;
				return -ECANCELED;
			}
		}

		// TODO: Restore the disc header here if necessary?
		// GCMs being imported generally won't have the first
		// 16 KB zeroed out...

		// TODO: Error handling.
		entry_src->reader->read(buf.get(), lba_count, LBA_COUNT_BUF);
		entry_dest->reader->write(buf.get(), lba_count, LBA_COUNT_BUF);
		entry_dest->reader->flush();
	}

	// Process any remaining LBAs.
	if (lba_count < lba_copy_len) {
		const unsigned int lba_left = lba_copy_len - lba_count;
		entry_src->reader->read(buf.get(), lba_count, lba_left);
		entry_dest->reader->write(buf.get(), lba_count, lba_left);
		entry_dest->reader->flush();
	}

	if (callback) {
		bool bRet;
		state.lba_processed = lba_copy_len;
		bRet = callback(&state, userdata);
		if (!bRet) {
			// Stop processing.
			errno = ECANCELED;
			return -ECANCELED;
		}
	}

	// Flush the destination device.
	entry_dest->reader->flush();

	// Update the bank table.
	// TODO: Check for errors.
	rvth_dest->d_ptr->writeBankEntry(bank_dest);

	// Finished importing the disc image.
	return 0;
}

/**
 * Import a disc image into this RVT-H disk image.
 * Compatibility wrapper; this function creates an RvtH object for the
 * RVT-H disk image and then copyToHDD().
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Source GCM filename.
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @param ios_force	[in,opt] IOS version to force. (-1 to use the existing IOS)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::import(unsigned int bank, const TCHAR *filename,
	RvtH_Progress_Callback callback, void *userdata,
	int ios_force)
{
	if (!filename || filename[0] == 0) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= bankCount()) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Open the standalone disc image.
	int ret = 0;
	unique_ptr<RvtH> rvth_src(new RvtH(filename, &ret));
	if (!rvth_src->isOpen()) {
		// Error opening the standalone disc image.
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	} else if (rvth_src->isHDD() || rvth_src->bankCount() > 1) {
		// Not a standalone disc image.
		errno = EINVAL;
		return RVTH_ERROR_IS_HDD_IMAGE;
	} else if (rvth_src->bankCount() == 0) {
		// Unrecognized file format.
		// TODO: Distinguish between unrecognized and no banks.
		errno = EINVAL;
		return RVTH_ERROR_NO_BANKS;
	}

	// Copy the bank from the source GCM to the HDD.
	// TODO: HDD to HDD?
	// NOTE: `bank` parameter starts at 0, not 1.
	ret = rvth_src->copyToHDD(this, bank, 0, callback, userdata);
	if (ret == 0) {
		// Must convert to debug realsigned for use on RVT-H.
		const RvtH_BankEntry *const entry = this->bankEntry(bank);
		if (entry &&
			(entry->type == RVTH_BankType_Wii_SL ||
			 entry->type == RVTH_BankType_Wii_DL) &&
			(entry->crypto_type == RVL_CryptoType_Retail ||
			 entry->crypto_type == RVL_CryptoType_Korean ||
			 entry->crypto_type == RVL_CryptoType_vWii ||
		         entry->ticket.sig_status != RVL_SigStatus_OK ||
			 entry->tmd.sig_status != RVL_SigStatus_OK ||
			 (ios_force >= 3 && entry->ios_version != ios_force)))
		{
			// One of the following conditions:
			// - Encryption: Retail, Korean, or vWii
			// - Signature: Invalid
			// - IOS requested does not match the TMD IOS
			// Convert to Debug.
			ret = recryptWiiPartitions(bank, RVL_CryptoType_Debug, callback, userdata, ios_force);
		}
		else
		{
			// No recryption needed.
			// Write the identifier to indicate that this bank was imported.
			ret = d_ptr->recryptID(bank);
		}
	}
	return ret;
}
