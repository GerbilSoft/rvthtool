/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_error.c: RVT-H error codes.                                        *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth_error.h"
#include "libwiicrypto/common.h"

// C includes
#include <assert.h>
#include <string.h>

#ifndef QT_TRANSLATE_NOOP
#  define QT_TRANSLATE_NOOP(context, sourceText) (sourceText)
#endif /* QT_TRANSLATE_NOOP */

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
		QT_TRANSLATE_NOOP("RvtH|Error", "Success"),
		// tr: RVTH_ERROR_UNRECOGNIZED_FILE
		QT_TRANSLATE_NOOP("RvtH|Error", "Unrecognized file format"),
		// tr: RVTH_ERROR_NHCD_TABLE_MAGIC
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank table magic is incorrect"),
		// tr: RVTH_ERROR_NO_BANKS
		QT_TRANSLATE_NOOP("RvtH|Error", "No banks found"),
		// tr: RVTH_ERROR_BANK_UNKNOWN
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank status is unknown"),
		// tr: RVTH_ERROR_BANK_EMPTY
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank is empty"),
		// tr: RVTH_ERROR_BANK_DL_2
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank is second bank of a dual-layer image"),
		// tr: RVTH_ERROR_NOT_A_DEVICE
		QT_TRANSLATE_NOOP("RvtH|Error", "Operation can only be performed on a device, not an image file"),
		// tr: RVTH_ERROR_BANK_IS_DELETED
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank is deleted"),
		// tr: RVTH_ERROR_BANK_NOT_DELETED
		QT_TRANSLATE_NOOP("RvtH|Error", "Bank is not deleted"),
		// tr: RVTH_ERROR_NOT_HDD_IMAGE
		QT_TRANSLATE_NOOP("RvtH|Error", "RVT-H object is not an HDD image"),
		// tr: RVTH_ERROR_NO_GAME_PARTITION
		QT_TRANSLATE_NOOP("RvtH|Error", "Wii game partition not found"),
		// tr: RVTH_ERROR_INVALID_BANK_COUNT
		QT_TRANSLATE_NOOP("RvtH|Error", "RVT-H bank count field is invalid"),
		// tr: RVTH_ERROR_IS_HDD_IMAGE
		QT_TRANSLATE_NOOP("RvtH|Error", "Operation cannot be performed on devices or HDD images"),
		// tr: RVTH_ERROR_IMAGE_TOO_BIG
		QT_TRANSLATE_NOOP("RvtH|Error", "Source image does not fit in an RVT-H bank"),
		// tr: RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED
		QT_TRANSLATE_NOOP("RvtH|Error", "Destination bank is not empty or deleted"),
		// tr: RVTH_ERROR_NOT_WII_IMAGE
		QT_TRANSLATE_NOOP("RvtH|Error", "Wii-specific operation was requested on a non-Wii image"),
		// tr: RVTH_ERROR_IS_UNENCRYPTED
		QT_TRANSLATE_NOOP("RvtH|Error", "Image is unencrypted"),
		// tr: RVTH_ERROR_IS_ENCRYPTED
		QT_TRANSLATE_NOOP("RvtH|Error", "Image is encrypted"),
		// tr: RVTH_ERROR_PARTITION_TABLE_CORRUPTED
		QT_TRANSLATE_NOOP("RvtH|Error", "Wii partition table is corrupted"),
		// tr: RVTH_ERROR_PARTITION_HEADER_CORRUPTED
		QT_TRANSLATE_NOOP("RvtH|Error", "At least one Wii partition header is corrupted"),
		// tr: RVTH_ERROR_ISSUER_UNKNOWN
		QT_TRANSLATE_NOOP("RvtH|Error", "Certificate has an unknown issuer"),

		// 'import' command: Dual-Layer errors

		// tr: RVTH_ERROR_IMPORT_DL_EXT_NO_BANK1
		QT_TRANSLATE_NOOP("RvtH|Error", "Extended Bank Table: Cannot use Bank 1 for a Dual-Layer image."),
		// tr: RVTH_ERROR_IMPORT_DL_LAST_BANK
		QT_TRANSLATE_NOOP("RvtH|Error", "Cannot use the last bank for a Dual-Layer image"),
		// tr: RVTH_ERROR_BANK2DL_NOT_EMPTY_OR_DELETED
		QT_TRANSLATE_NOOP("RvtH|Error", "The second bank for the Dual-Layer image is not empty or deleted"),
		// tr: RVTH_ERROR_IMPORT_DL_NOT_CONTIGUOUS
		QT_TRANSLATE_NOOP("RvtH|Error", "The two banks are not contiguous"),

		// NDEV option

		// tr: RVTH_ERROR_NDEV_GCN_NOT_SUPPORTED
		QT_TRANSLATE_NOOP("RvtH|Error", "NDEV headers for GCN are currently not supported."),
	};
	static_assert(ARRAY_SIZE(errtbl) == RVTH_ERROR_MAX, "Missing error descriptions!");

	if (err < 0) {
		return strerror(-err);
	} else if (err >= ARRAY_SIZE(errtbl)) {
		return "(unknown)";
	}

	return errtbl[err];
}
