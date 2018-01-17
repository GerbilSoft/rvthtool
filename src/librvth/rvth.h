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

#include "ref_file.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// TODO: Move to another file.
#if defined(_MSC_VER) && _MSC_VER >= 1800
# include <stdbool.h>
#else
typedef unsigned char bool;
#define true 1
#define false 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8
#define RVTH_BLOCK_SIZE 512

typedef enum {
	RVTH_ERROR_SUCCESS		= 0,
	RVTH_ERROR_NHCD_TABLE_MAGIC	= 1,	// NHCD bank table has the wrong magic.
	RVTH_ERROR_BANK_EMPTY		= 2,	// Selected bank is empty.
} RvtH_Errors;

// RVT-H struct.
struct _RvtH;
typedef struct _RvtH RvtH;

// Disc image and RVT-H bank entry definition.
typedef struct RvtH_BankEntry {
	RefFile *f_img;		// Disc image file.
	// TODO: Function pointer table for reading CISO and WBFS.
	uint32_t lba_start;	// Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// Length, in 512-byte sectors.
	char id6[6];		// Game ID. (NOT NULL-terminated!)
	char game_title[65];	// Game title. (from GCN header)
	time_t timestamp;	// Timestamp. (no timezone information)
	uint8_t type;		// Bank type. (See RvtH_BankType_e.)
	uint8_t crypto_type;	// Encryption type. (See RvtH_CryptoType_e.)
	uint8_t sig_type;	// Signature type. (See RvtH_SigType_e.)
	bool is_deleted;	// If true, this entry was deleted.
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
} RvtH_CryptoType_e;

// Signature types.
typedef enum {
	RVTH_SigType_Unknown	= 0,	// Unknown signature.
	RVTH_SigType_Debug	= 1,	// Debug signature.
	RVTH_SigType_Retail	= 2,	// Retail signature.
} RvtH_SigType_e;

/** Functions. **/

/**
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const char *filename, int *pErr);

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth);

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range or empty.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr);

/**
 * RVT-H progress callback.
 * @param lba_processed LBAs processed.
 * @param lba_total LBAs total.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Progress_Callback)(uint32_t lba_processed, uint32_t lba_total);

/**
 * Extract a disc image from the RVT-H disk image.
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param callback	[in,opt] Progress callback.
 * @return 0 on success; negative POSIX error code on error.
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const char *filename, RvtH_Progress_Callback callback);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_H__ */
