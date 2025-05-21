/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * wii_sector.cpp: Wii sector layout structures.                           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include "common.h"

// Defined in nettle/sha1.h, but let's not include it here.
#define RVL_SHA1_DIGEST_SIZE 20

// Sector: 32 KB [H0]
// Subgroup: 8 sectors == 256 KB [H1]
// Group: 8 subgroups == 2 MB [H2]

#define SECTOR_SIZE_DEC		(31*1024)
#define SECTOR_SIZE_ENC		(32*1024)
#define SUBGROUP_SIZE_DEC	(8*SECTOR_SIZE_DEC)
#define SUBGROUP_SIZE_ENC	(8*SECTOR_SIZE_ENC)
#define GROUP_SIZE_DEC		(8*SUBGROUP_SIZE_DEC)
#define GROUP_SIZE_ENC		(8*SUBGROUP_SIZE_ENC)

// H3 table: SHA-1 hashes of each group's H2 tables.
// Up to 4,915 groups can be hashed. (9,830 MB of encrypted data)
// Unused hash entries are all zero.
// The SHA-1 hash of the H3 table is stored in the TMD content table.
typedef struct _Wii_Disc_H3_t {
	uint8_t h3[4915][RVL_SHA1_DIGEST_SIZE];
	uint8_t pad[4];
} Wii_Disc_H3_t;
ASSERT_STRUCT(Wii_Disc_H3_t, 0x18000);

// Encrypted Wii disc sector: Hash data.
// The hash data is encrypted using AES-128-CBC.
// - Key: Decrypted title key.
// - IV: All zero.
typedef struct _Wii_Disc_Hashes_t {
	// H0 hashes.
	// One SHA-1 hash for each kilobyte of user data.
	uint8_t H0[31][RVL_SHA1_DIGEST_SIZE];

	// Padding. (0x00)
	uint8_t pad_H0[20];

	// H1 hashes.
	// Each hash is over the H0 table for each sector
	// in an 8-sector subgroup.
	uint8_t H1[8][RVL_SHA1_DIGEST_SIZE];

	// Padding. (0x00)
	uint8_t pad_H1[32];

	// H2 hashes.
	// Each hash is over the H1 table for each subgroup
	// in an 8-subgroup group.
	// NOTE: The last 16 bytes of h2[7], when encrypted,
	// is the user data CBC IV.
	uint8_t H2[8][RVL_SHA1_DIGEST_SIZE];

	// Padding. (0x00)
	uint8_t pad_H2[32];
} Wii_Disc_Hashes_t;
ASSERT_STRUCT(Wii_Disc_Hashes_t, 1024);

// Encrypted Wii disc sector.
typedef struct _Wii_Disc_Sector_t {
	// Hash table.
	Wii_Disc_Hashes_t hashes;

	// User data.
	// This section is encrypted using AES-128-CBC:
	// - Key: Decrypted title key.
	// - IV: *Encrypted* bytes 0x3D0-0x3DF of the hash table,
	//        aka the last 16 bytes of hashes.h2[7].
	uint8_t data[31*1024];
} Wii_Disc_Sector_t;
ASSERT_STRUCT(Wii_Disc_Sector_t, 32*1024);
