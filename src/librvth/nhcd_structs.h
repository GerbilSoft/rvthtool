/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * nhcd_structs.h: RVT-H data structures.                                  *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// NOTE: This file defines the on-disk RVT-H structs.
// These structs are prefixed with "NHCD_", named after
// the magic number.

// rvth.h contains "RVTH_" high-level structs, which should
// be used by the application.

#pragma once

#include <stdint.h>
#include "libwiicrypto/common.h"

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
	uint32_t bank_count;	// [0x008] Bank count.
	uint32_t x00C;		// [0x00C] 0x00000000
	uint32_t x010;		// [0x010] 0x002FF000
	uint8_t unk[492];	// [0x014] Unknown
} NHCD_BankTable_Header;
ASSERT_STRUCT(NHCD_BankTable_Header, 512);

// NHCD header status.
// TOOD: Maybe move "MBR" and "GPT" to a separate value and/or use a bitfield.
typedef enum {
	NHCD_STATUS_UNKNOWN	= 0,	// Unknown
	NHCD_STATUS_OK		= 1,	// NHCD is present
	NHCD_STATUS_MISSING	= 2,	// NHCD is missing
	NHCD_STATUS_HAS_MBR	= 3,	// NHCD is missing; MBR is present
	NHCD_STATUS_HAS_GPT	= 4,	// NHCD is missing; EFI GPT is present
} NHCD_Status_e;

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
ASSERT_STRUCT(NHCD_BankEntry, 512);

/**
 * RVT-H bank types.
 */
typedef enum {
	NHCD_BankType_Empty  = 0x00000000,
	NHCD_BankType_GCN    = 0x4743314C,	// "GC1L"
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
ASSERT_STRUCT(NHCD_BankTable, 512*9);

/** LBA values. **/

// Bank table address.
#define NHCD_BANKTABLE_ADDRESS_LBA		0x300000U
// Maximum bank size.
#define NHCD_BANK_SIZE_LBA			0x8C4A00U

// Bank 1 size for extended bank tables.
#define NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA	NHCD_BANKTABLE_ADDRESS_LBA
#define NHCD_EXTBANKTABLE_BANK_1_OFFSET_LBA	(NHCD_BANKTABLE_ADDRESS_LBA - NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA)

/**
 * Get the default bank starting address.
 *
 * For `bank_count <= 8`, this always starts after the
 * bank table header and 8 bank entries. (0x300009U)
 *
 * For `bank_count > 8`, the same is true for banks 2 and higher.
 * Bank 1 is relocated to LBA 0 and will only support GCN images.
 * TODO: Test changes - was relocated to LBA 0x40000, but now 0.
 *
 * @param bank Bank number. (0-7)
 * @param bank_count Bank count.
 * @return Bank starting address, in LBAs.
 */
#define NHCD_BANK_START_LBA(bank, bank_count) \
	((bank) > 0 \
		? (NHCD_BANKTABLE_ADDRESS_LBA + 9U + (NHCD_BANK_SIZE_LBA * (bank))) \
		: (((bank_count) <= 8) \
			? (NHCD_BANKTABLE_ADDRESS_LBA + 9U) \
			: NHCD_EXTBANKTABLE_BANK_1_OFFSET_LBA \
		) \
	)

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
#define NHCD_BANK_GCN_SIZE_RETAIL_LBA		0x2B82C0U
#define NHCD_BANK_GCN_SIZE_NR_LBA		0x2B82C0U	/* VERIFY */

// Block size.
#define NHCD_BLOCK_SIZE		512

// SDK header size.
#define SDK_HEADER_SIZE_BYTES	32768
#define SDK_HEADER_SIZE_LBA	(SDK_HEADER_SIZE_BYTES / NHCD_BLOCK_SIZE)

/** LBA macros **/

// LBA size.
#define LBA_SIZE 512

// Convert LBA values to bytes.
#define LBA_TO_BYTES(x) ((off64_t)(x) * LBA_SIZE)
// Convert bytes to an LBA value.
// NOTE: Partial LBAs will be truncated!
#define BYTES_TO_LBA(x) ((uint32_t)((x) / LBA_SIZE))

#pragma pack()

#ifdef __cplusplus
}
#endif
