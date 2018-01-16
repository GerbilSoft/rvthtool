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

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8
#define RVTH_BLOCK_SIZE 512

// RVT-H struct.
struct _RvtH;
typedef struct _RvtH RvtH;

// RVT-H bank entry.
typedef struct RvtH_BankEntry {
	uint32_t lba_start;	// Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// Length, in 512-byte sectors.
	time_t timestamp;	// Timestamp. (no timezone information)
	uint8_t type;		// Bank type. (See RvtH_BankType_e.)
	char id6[6];		// Game ID. (NOT NULL-terminated!)
	char game_title[65];	// Game title. (from GCN header)
	uint8_t crypto_type;	// Encryption type. (See RvtH_CryptoType_e.)
	bool is_deleted;	// If true, this entry was deleted.
} RvtH_BankEntry;

// RVT-H bank types.
typedef enum {
	RVTH_BankType_Empty	= 0,	// Magic is 0.
	RVTH_BankType_Unknown	= 1,	// Unknown magic.
	RVTH_BankType_GCN	= 2,
	RVTH_BankType_Wii_SL	= 3,
	RVTH_BankType_Wii_DL	= 4,

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

/** Functions. **/

/**
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename Filename.
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const char *filename);

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth);

/**
 * Get a bank table entry.
 * @param rvth RVT-H disk image.
 * @param bank Bank number. (0-7)
 * @return Bank table entry, or NULL if out of range or empty.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_H__ */
