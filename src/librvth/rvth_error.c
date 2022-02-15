/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_error.c: RVT-H error codes.                                        *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth_error.h"
#include "libwiicrypto/common.h"

// C includes.
#include <assert.h>
#include <string.h>

/**
 * Get a string description of an error number.
 * - Negative: POSIX error. (strerror())
 * - Positive; RVT-H error. (RvtH_Errors)
 * @param err Error number.
 * @return String description.
 */
const char *rvth_error(int err)
{
	// TODO: Update functions to only return POSIX error codes
	// for system-level issues. For anything weird encountered
	// within an RVT-H HDD or GCN/Wii disc image, an
	// RvtH_Errors code should be retured instead.
	static const char *const errtbl[] = {
		// tr: RVTH_ERROR_SUCCESS
		"Success",
		// tr: RVTH_ERROR_UNRECOGNIZED_FILE
		"Unrecognized file format",
		// tr: RVTH_ERROR_NHCD_TABLE_MAGIC
		"Bank table magic is incorrect",
		// tr: RVTH_ERROR_NO_BANKS
		"No banks found",
		// tr: RVTH_ERROR_BANK_UNKNOWN
		"Bank status is unknown",
		// tr: RVTH_ERROR_BANK_EMPTY
		"Bank is empty",
		// tr: RVTH_ERROR_BANK_DL_2
		"Bank is second bank of a dual-layer image",
		// tr: RVTH_ERROR_NOT_A_DEVICE
		"Operation can only be performed on a device, not an image file",
		// tr: RVTH_ERROR_BANK_IS_DELETED
		"Bank is deleted",
		// tr: RVTH_ERROR_BANK_NOT_DELETED
		"Bank is not deleted",
		// tr: RVTH_ERROR_NOT_HDD_IMAGE
		"RVT-H object is not an HDD image",
		// tr: RVTH_ERROR_NO_GAME_PARTITION
		"Wii game partition not found",
		// tr: RVTH_ERROR_INVALID_BANK_COUNT
		"RVT-H bank count field is invalid",
		// tr: RVTH_ERROR_IS_HDD_IMAGE
		"Operation cannot be performed on devices or HDD images",
		// tr: RVTH_ERROR_IMAGE_TOO_BIG
		"Source image does not fit in an RVT-H bank",
		// tr: RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED
		"Destination bank is not empty or deleted",
		// tr: RVTH_ERROR_NOT_WII_IMAGE
		"Wii-specific operation was requested on a non-Wii image",
		// tr: RVTH_ERROR_IS_UNENCRYPTED
		"Image is unencrypted",
		// tr: RVTH_ERROR_IS_ENCRYPTED
		"Image is encrypted",
		// tr: RVTH_ERROR_PARTITION_TABLE_CORRUPTED
		"Wii partition table is corrupted",
		// tr: RVTH_ERROR_PARTITION_HEADER_CORRUPTED
		"At least one Wii partition header is corrupted",
		// tr: RVTH_ERROR_ISSUER_UNKNOWN
		"Certificate has an unknown issuer",

		// 'import' command: Dual-Layer errors.

		// tr: RVTH_ERROR_IMPORT_DL_EXT_NO_BANK1
		"Extended Bank Table: Cannot use Bank 1 for a Dual-Layer image.",
		// tr: RVTH_ERROR_IMPORT_DL_LAST_BANK
		"Cannot use the last bank for a Dual-Layer image",
		// tr: RVTH_ERROR_BANK2DL_NOT_EMPTY_OR_DELETED
		"The second bank for the Dual-Layer image is not empty or deleted",
		// tr: RVTH_ERROR_IMPORT_DL_NOT_CONTIGUOUS
		"The two banks are not contiguous",

		// NDEV option.

		// tr: RVTH_ERROR_NDEV_GCN_NOT_SUPPORTED
		"NDEV headers for GCN are currently unsupported.",
	};
	static_assert(ARRAY_SIZE(errtbl) == RVTH_ERROR_MAX, "Missing error descriptions!");

	if (err < 0) {
		return strerror(-err);
	} else if (err >= ARRAY_SIZE(errtbl)) {
		return "(unknown)";
	}

	return errtbl[err];
}
