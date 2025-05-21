/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.cpp: RVT-H image handler.                                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_p.hpp"
#include "rvth_error.h"

#include "nhcd_structs.h"
#include "disc_header.hpp"
#include "ptbl.h"
#include "bank_init.h"
#include "reader/Reader.hpp"

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"

#include "time_r.h"

// C includes.
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Open an RVT-H disk image, GameCube disc image, or Wii disc image.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
RvtH::RvtH(const TCHAR *filename, int *pErr)
	: d_ptr(new RvtHPrivate(this))
{
	// Open the disk image.
	RefFile *const f_img = new RefFile(filename);
	if (!f_img->isOpen()) {
		// Could not open the file.
		if (pErr) {
			*pErr = -f_img->lastError();
		}
		f_img->unref();
		return;
	}

	// Determine if this is an HDD image or a disc image.
	errno = 0;
	int64_t len = f_img->size();
	if (len <= 0) {
		// File is empty and/or an I/O error occurred.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
	} else if (len <= 2*LBA_TO_BYTES(NHCD_BANK_SIZE_LBA)) {
		// Two banks or less.
		// This is most likely a standalone disc image.
		errno = 0;
		int err = d_ptr->openGcm(f_img);
		if (pErr) {
			*pErr = err;
		}
	} else {
		// More than two banks.
		// This is most likely an RVT-H HDD image.
		errno = 0;
		int err = d_ptr->openHDD(f_img);
		if (pErr) {
			*pErr = err;
		}
	}

	// If the RvtH object was opened, it will have
	// called f_img->ref() to increment the reference count.
	f_img->unref();
}

RvtH::~RvtH()
{
	// Close all bank entry files.
	// RefFile has a reference count, so we have to clear the count.
	for (unsigned int i = 0; i < bankCount(); i++) {
		delete d_ptr->entries[i].reader;
		free(d_ptr->entries[i].ptbl);
	}

	// Clear the main file reference.
	if (d_ptr->file) {
		d_ptr->file->unref();
	}
}

/**
 * Is the file open?
 * @return True if open; false if not.
 */
bool RvtH::isOpen(void) const
{
	return (d_ptr->file != nullptr);
}

/**
 * Get the number of banks in the RVT-H disk image.
 * @return Number of banks
 */
unsigned int RvtH::bankCount(void) const
{
	return d_ptr->bankCount();
}

/**
 * Is this RVT-H object an RVT-H Reader / HDD image, or a standalone disc image?
 * @param rvth RVT-H object
 * @return True if the RVT-H object is an RVT-H Reader / HDD image; false if it's a standalone disc image.
 */
bool RvtH::isHDD(void) const
{
	return d_ptr->isHDD();
}

/**
 * Get the RVT-H image type.
 * @param rvth RVT-H object
 * @return RVT-H image type
 */
RvtH_ImageType_e RvtH::imageType(void) const
{
	return d_ptr->imageType;
}

/**
 * Get the NHCD table status.
 *
 * For optical disc images, this is always NHCD_STATUS_MISSING.
 * For HDD images, this should be NHCD_STATUS_OK unless the
 * NHCD table was wiped or a PC-partitioned disk was selected.
 *
 * @return NHCD table status
 */
NHCD_Status_e RvtH::nhcd_status(void) const
{
	return d_ptr->nhcdStatus;
}

/**
 * Get the NHCD Table Header.
 * @return NHCD Bank Table Header
 */
NHCD_BankTable_Header *RvtH::nhcd_header(void) const
{
	return d_ptr->nhcdHeader.get();
}

/**
 * Get a bank table entry.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range.
 */
const RvtH_BankEntry *RvtH::bankEntry(unsigned int bank, int *pErr) const
{
	if (bank >= bankCount()) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -ERANGE;
		}
		return nullptr;
	}

	return &d_ptr->entries[bank];
}
