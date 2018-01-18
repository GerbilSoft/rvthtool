/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * nhcd_structs.h: RVT-H data structures.                                  *
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

// NOTE: This file defines the on-disk RVT-H structs.
// These structs are prefixed with "NHCD_", named after
// the magic number.

// rvth.h contains "RVTH_" high-level structs, which should
// be used by the application.

// FIXME: ASSERT_STRUCT() doesn't work in C99.

#ifndef __RVTHTOOL_LIBRVTH_NHCD_STRUCTS_H__
#define __RVTHTOOL_LIBRVTH_NHCD_STRUCTS_H__

#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

#define NHCD_BANK_COUNT 8

/**
 * RVT-H bank table header.
 * All fields are in big-endian.
 */
#define NHCD_BANKTABLE_MAGIC 0x4E484344	/* "NHCD" */
typedef struct PACKED _NHCD_BankTable_Header {
	uint32_t magic;		// [0x000] "NHCD"
	uint32_t x004;		// [0x004] 0x00000001
	uint32_t x008;		// [0x008] 0x00000008
	uint32_t x00C;		// [0x00C] 0x00000000
	uint32_t x010;		// [0x010] 0x002FF000
	uint8_t unk[492];	// [0x014] Unknown
} NHCD_BankTable_Header;
//ASSERT_STRUCT(NHCD_BankTable_Header, 512);

/**
 * RVT-H bank entry.
 * All fields are in big-endian.
 */
typedef struct PACKED _NHCD_BankEntry {
	uint32_t type;		// [0x000] Type. See NHCD_BankType_e.
	char all_zero[14];	// [0x004] All ASCII zeroes. ('0')
	char timestamp[14];	// [0x012] Full timestamp, in ASCII. ('20180112222720')
	uint32_t lba_start;	// [0x020] Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// [0x024] Length, in 512-byte sectors.
	uint8_t unk[472];	// [0x028] Unknown
} NHCD_BankEntry;
//ASSERT_STRUCT(NHCD_BankEntry, 512);

/**
 * RVT-H bank types.
 */
typedef enum {
	NHCD_BankType_GCN = 0x4743314C,		// "GC1L"
	NHCD_BankType_Wii_SL = 0x4E4E314C,	// "NN1L"
	NHCD_BankType_Wii_DL = 0x4E4E324C,	// "NN2L"
} NHCD_BankType_e;

/**
 * RVT-H bank table.
 */
typedef struct _NHCD_BankTable {
	NHCD_BankTable_Header header;
	NHCD_BankEntry entries[NHCD_BANK_COUNT];
} NHCD_BankTable;
//ASSERT_STRUCT(NHCD_BankTable, 512*9);

/** Byte values. **/

/** LBA values. **/

// Bank table address.
#define NHCD_BANKTABLE_ADDRESS_LBA		0x300000U
// Bank 1 starting address.
#define NHCD_BANK_1_START_LBA			0x300009U
// Maximum bank size.
#define NHCD_BANK_SIZE_LBA			0x8C4A00U

// Wii SL disc image size.
// NOTE: The RVT-R size matches the DVD-R specification.
#define NHCD_BANK_WII_SL_SIZE_RETAIL_LBA	0x8C1200U
#define NHCD_BANK_WII_SL_SIZE_RVTR_LBA		0x8C4A00U
#define NHCD_BANK_WII_SL_SIZE_NOCRYPTO_LBA	0x800000U

// Wii DL disc image size.
// NOTE: The RVT-R size matches the DVD-R specification.
#define NHCD_BANK_WII_DL_SIZE_RETAIL_LBA	0xFDA700U
#define NHCD_BANK_WII_DL_SIZE_RVTR_LBA		0xFE9F00U	/* VERIFY */
#define NHCD_BANK_WII_DL_SIZE_NOCRYPTO_LBA	0xEE0000U

// GameCube disc image size.
// TODO: NR size?
#define NHCD_BANK_GCN_DL_SIZE_RETAIL_LBA	0x2B82C0U
#define NHCD_BANK_GCN_DL_SIZE_NR_LBA		0x2B82C0U	/* VERIFY */

// Block size.
#define NHCD_BLOCK_SIZE		512

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_NHCD_STRUCTS_H__ */
