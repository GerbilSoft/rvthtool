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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_H__
#define __RVTHTOOL_LIBRVTH_RVTH_H__

#include "common.h"
#include "ref_file.h"
#include "reader.h"
#include "cert_store.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8	/* Standard RVT-H HDD bank count. */
#define RVTH_BLOCK_SIZE 512

// Convert LBA values to bytes.
#define LBA_TO_BYTES(x) ((int64_t)(x) * RVTH_BLOCK_SIZE)
// Convert bytes to an LBA value.
// NOTE: Partial LBAs will be truncated!
#define BYTES_TO_LBA(x) ((uint32_t)(x / RVTH_BLOCK_SIZE))

typedef enum {
	RVTH_ERROR_SUCCESS		= 0,
	RVTH_ERROR_UNRECOGNIZED_FILE	= 1,	// Unrecognized file format.
	RVTH_ERROR_NHCD_TABLE_MAGIC	= 2,	// NHCD bank table has the wrong magic.
	RVTH_ERROR_NO_BANKS		= 3,	// No banks.
	RVTH_ERROR_BANK_UNKNOWN		= 4,	// Selected bank has an unknown status.
	RVTH_ERROR_BANK_EMPTY		= 5,	// Selected bank is empty.
	RVTH_ERROR_BANK_DL_2		= 6,	// Selected bank is the second bank of a DL image.
	RVTH_ERROR_NOT_A_DEVICE		= 7,	// Attempting to write to an RVT-H disk image.
	RVTH_ERROR_BANK_IS_DELETED	= 8,	// Attempting to delete a bank that's already deleted.
	RVTH_ERROR_BANK_NOT_DELETED	= 9,	// Attempting to undelete a bank that isn't deleted.
	RVTH_ERROR_NOT_HDD_IMAGE	= 10,	// Attempting to modify the bank table of a non-HDD image.
	RVTH_ERROR_NO_GAME_PARTITION	= 11,	// Game partition was not found in a Wii image.
	RVTH_ERROR_INVALID_BANK_COUNT	= 12,	// `bank_count` field is invalid.
	RVTH_ERROR_IS_HDD_IMAGE		= 13,	// Operation does not support HDD images.
	RVTH_ERROR_IS_RETAIL_CRYPTO	= 14,	// Cannot import a retail-encrypted Wii game.
	RVTH_ERROR_IMAGE_TOO_BIG	= 15,	// Source image does not fit in an RVT-H bank.
	RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED	= 16,	// Destination bank is not empty or deleted.
	RVTH_ERROR_NOT_WII_IMAGE	= 17,	// Wii-specific operation was requested on a non-Wii image.
	RVTH_ERROR_IS_UNENCRYPTED	= 18,	// Image is unencrypted.
	RVTH_ERROR_IS_ENCRYPTED		= 19,	// Image is encrypted.
	RVTH_ERROR_PARTITION_TABLE_CORRUPTED	= 20,	// Wii partition table is corrupted.
	RVTH_ERROR_PARTITION_HEADER_CORRUPTED	= 21,	// At least one Wii partition header is corrupted.
	RVTH_ERROR_ISSUER_UNKNOWN	= 22,	// Certificate has an unknown issuer.

	RVTH_ERROR_MAX
} RvtH_Errors;

// RVT-H struct.
struct _RvtH;
typedef struct _RvtH RvtH;

// Wii signature information.
typedef struct _RvtH_SigInfo {
	uint8_t sig_type;	// Signature type. (See RvtH_SigType_e.)
	uint8_t sig_status;	// Signature status. (See RvtH_SigStatus_e.)
} RvtH_SigInfo;

// Disc image and RVT-H bank entry definition.
typedef struct RvtH_BankEntry {
	Reader *reader;		// Disc image reader.
	// TODO: Function pointer table for reading CISO and WBFS.
	uint32_t lba_start;	// Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// Length, in 512-byte sectors.
	char id6[6];		// Game ID. (NOT NULL-terminated!)
	char game_title[65];	// Game title. (from GCN header)
	time_t timestamp;	// Timestamp. (no timezone information)
	uint8_t type;		// Bank type. (See RvtH_BankType_e.)
	uint8_t disc_number;	// Disc number.
	uint8_t revision;	// Revision.
	uint8_t region_code;	// Region code. (See GCN_Region_Code.)
	bool is_deleted;	// If true, this entry was deleted.

	// Wii-specific
	uint8_t crypto_type;	// Encryption type. (See RvtH_CryptoType_e.)
	RvtH_SigInfo ticket;	// Ticket encryption/signature.
	RvtH_SigInfo tmd;	// TMD encryption/signature.
} RvtH_BankEntry;

// RVT-H bank types.
typedef enum {
	RVTH_BankType_Empty		= 0,	// Magic is 0.
	RVTH_BankType_Unknown		= 1,	// Unknown magic.
	RVTH_BankType_GCN		= 2,
	RVTH_BankType_Wii_SL		= 3,
	RVTH_BankType_Wii_DL		= 4,
	RVTH_BankType_Wii_DL_Bank2	= 5,	// Bank 2 for DL images.

	RVTH_BankType_MAX
} RvtH_BankType_e;

// Encryption types.
typedef enum {
	RVTH_CryptoType_Unknown	= 0,	// Unknown encryption.
	RVTH_CryptoType_None	= 1,	// No encryption.
	RVTH_CryptoType_Debug	= 2,	// RVT-R encryption.
	RVTH_CryptoType_Retail	= 3,	// Retail encryption.
	RVTH_CryptoType_Korean	= 4,	// Korean retail encryption.

	RVTH_CryptoType_MAX
} RvtH_CryptoType_e;

// Signature types.
typedef enum {
	RVTH_SigType_Unknown	= 0,	// Unknown signature.
	RVTH_SigType_Debug	= 1,	// Debug signature.
	RVTH_SigType_Retail	= 2,	// Retail signature.

	RVTH_SigType_MAX
} RvtH_SigType_e;

// Signature status.
typedef enum {
	RVTH_SigStatus_Unknown	= 0,	// Unknown signature status.
	RVTH_SigStatus_OK	= 1,	// Valid
	RVTH_SigStatus_Invalid	= 2,	// Invalid
	RVTH_SigStatus_Fake	= 3,	// Fakesigned

	RVTH_SigStatus_MAX
} RVTH_SigStatus_e;

/** Functions. **/

/**
 * Get a string description of an error number.
 * - Negative: POSIX error. (strerror())
 * - Positive; RVT-H error. (RvtH_Errors)
 * @param err Error number.
 * @return String description.
 */
const char *rvth_error(int err);

/**
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const TCHAR *filename, int *pErr);

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth);

/**
 * Get the number of banks in an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 * @return Number of banks.
 */
unsigned int rvth_get_BankCount(const RvtH *rvth);

/**
 * Is this RVT-H object an HDD image or a standalone disc image?
 * @param rvth RVT-H object.
 * @return True if the RVT-H object is an HDD image; false if it's a standalone disc image.
 */
bool rvth_is_hdd(const RvtH *rvth);

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr);

/** rvth_write.c **/

// Progress callback type.
typedef enum {
	RVTH_PROGRESS_UNKNOWN	= 0,
	RVTH_PROGRESS_EXTRACT,		// Extract image
	RVTH_PROGRESS_IMPORT,		// Import image
	RVTH_PROGRESS_RECRYPT,		// Recrypt image
} RvtH_Progress_Type;

// Progress callback status.
typedef struct _RvtH_Progress_State {
	RvtH_Progress_Type type;

	// RvtH objects.
	const RvtH *rvth;	// Primary RvtH.
	const RvtH *rvth_gcm;	// GCM being extracted or imported, if not NULL.

	// Bank numbers.
	unsigned int bank_rvth;	// Bank number in `rvth`.
	unsigned int bank_gcm;	// Bank number in `rvth_gcm`. (UINT_MAX if none)

	// Progress.
	// If RVTH_PROGRESS_RECRYPT and lba_total == 0,
	// we're only changing the ticket and TMD.
	// Otherwise, we're encrypting/decrypting.
	uint32_t lba_processed;
	uint32_t lba_total;
} RvtH_Progress_State;

/**
 * RVT-H progress callback.
 * @param data Callback data.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Progress_Callback)(const RvtH_Progress_State *state);

/**
 * Delete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_delete(RvtH *rvth, unsigned int bank);

/**
 * Undelete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_undelete(RvtH *rvth, unsigned int bank);

/** rvth_extract.c **/

/**
 * Create a writable disc image object.
 * @param filename	[in] Filename.
 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH on success; NULL on error. (check errno)
 */
RvtH *rvth_create_gcm(const TCHAR *filename, uint32_t lba_len, int *pErr);

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to a writable standalone disc image.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_gcm(RvtH *rvth_dest, const RvtH *rvth_src, unsigned int bank_src, RvtH_Progress_Callback callback);

/**
 * Extract a disc image from the RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_create_gcm() and rvth_copy().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback);

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to an RVT-H system.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param bank_dest	[out] Destination bank number. (0-7)
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_hdd(RvtH *rvth_dest, unsigned int bank_dest, const RvtH *rvth_src,
	unsigned int bank_src, RvtH_Progress_Callback callback);

/**
 * Import a disc image into an RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_open() and rvth_copy_to_hdd().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Source GCM filename.
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_import(RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_H__ */
