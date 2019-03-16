/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_error.h: RVT-H error codes.                                        *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#ifndef __LIBRVTH_RVTH_ERROR_H__
#define __LIBRVTH_RVTH_ERROR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RVTH_ERROR_SUCCESS			= 0,
	RVTH_ERROR_UNRECOGNIZED_FILE		= 1,	// Unrecognized file format.
	RVTH_ERROR_NHCD_TABLE_MAGIC		= 2,	// NHCD bank table has the wrong magic.
	RVTH_ERROR_NO_BANKS			= 3,	// No banks.
	RVTH_ERROR_BANK_UNKNOWN			= 4,	// Selected bank has an unknown status.
	RVTH_ERROR_BANK_EMPTY			= 5,	// Selected bank is empty.
	RVTH_ERROR_BANK_DL_2			= 6,	// Selected bank is the second bank of a DL image.
	RVTH_ERROR_NOT_A_DEVICE			= 7,	// Attempting to write to an RVT-H disk image.
	RVTH_ERROR_BANK_IS_DELETED		= 8,	// Attempting to delete a bank that's already deleted.
	RVTH_ERROR_BANK_NOT_DELETED		= 9,	// Attempting to undelete a bank that isn't deleted.
	RVTH_ERROR_NOT_HDD_IMAGE		= 10,	// Attempting to modify the bank table of a non-HDD image.
	RVTH_ERROR_NO_GAME_PARTITION		= 11,	// Game partition was not found in a Wii image.
	RVTH_ERROR_INVALID_BANK_COUNT		= 12,	// `bank_count` field is invalid.
	RVTH_ERROR_IS_HDD_IMAGE			= 13,	// Operation cannot be performed on devices or HDD images.
	RVTH_ERROR_IMAGE_TOO_BIG		= 14,	// Source image does not fit in an RVT-H bank.
	RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED	= 15,	// Destination bank is not empty or deleted.
	RVTH_ERROR_NOT_WII_IMAGE		= 16,	// Wii-specific operation was requested on a non-Wii image.
	RVTH_ERROR_IS_UNENCRYPTED		= 17,	// Image is unencrypted.
	RVTH_ERROR_IS_ENCRYPTED			= 18,	// Image is encrypted.
	RVTH_ERROR_PARTITION_TABLE_CORRUPTED	= 19,	// Wii partition table is corrupted.
	RVTH_ERROR_PARTITION_HEADER_CORRUPTED	= 20,	// At least one Wii partition header is corrupted.
	RVTH_ERROR_ISSUER_UNKNOWN		= 21,	// Certificate has an unknown issuer.

	// 'import' command: Dual-Layer errors.
	RVTH_ERROR_IMPORT_DL_EXT_NO_BANK1	= 22,	// Extended Bank Table: Cannot use Bank 1 for a Dual-Layer image.
	RVTH_ERROR_IMPORT_DL_LAST_BANK		= 23,	// Cannot use the last bank for a Dual-Layer image.
	RVTH_ERROR_BANK2DL_NOT_EMPTY_OR_DELETED	= 24,	// The second bank for the Dual-Layer image is not empty or deleted.
	RVTH_ERROR_IMPORT_DL_NOT_CONTIGUOUS	= 25,	// The two banks are not contiguous.

	// NDEV option.
	RVTH_ERROR_NDEV_GCN_NOT_SUPPORTED	= 26,	// NDEV headers for GCN are currently unsupported.

	RVTH_ERROR_MAX
} RvtH_Errors;

/**
 * Get a string description of an error number.
 * - Negative: POSIX error. (strerror())
 * - Positive; RVT-H error. (RvtH_Errors)
 * @param err Error number.
 * @return String description.
 */
const char *rvth_error(int err);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRVTH_RVTH_ERROR_H__ */
