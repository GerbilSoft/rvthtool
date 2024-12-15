/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * wiiu_structs.h: Nintendo Wii U data structures.                         *
 *                                                                         *
 * Copyright (c) 2016-2024 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// NOTE: This file has Wii U-specific structs only.
// For structs shared with Wii, see wii_structs.h.
// For structs shared with GameCube, see gcn_structs.h.

#pragma once

#include <stdint.h>
#include "common.h"

#include "wii_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Wii U ticket constants.
#define WUP_SIGNATURE_TYPE_RSA2048_SHA256 0x10004

/**
 * Wii U ticket.
 * References:
 * - https://wiibrew.org/wiki/Ticket
 * - https://wiiubrew.org/wiki/Ticket
 * - https://www.3dbrew.org/wiki/Ticket
 * - https://github.com/Maschell/JNUSLib/blob/3eb299d3a150107f5c4d474650fe6d05d91bf64c/src/de/mas/wiiu/jnus/entities/Ticket.java
 */
#pragma pack(1)
typedef struct PACKED _WUP_Ticket {
	uint32_t signature_type;	// [0x000] Always 0x10004 for RSA-2048 with SHA-256.
	uint8_t signature[0x100];	// [0x004] Signature.
	uint8_t padding_sig[0x3C];	// [0x104] Padding. (always 0)

	// The following fields are all covered by the above signature.
	char issuer[0x40];		// [0x140] Signature issuer.
	uint8_t ecdh_data[0x3C];	// [0x180] ECDH data.
	uint8_t version;		// [0x1BC] Ticket version. (Wii U: v1)
	uint8_t ca_crl_version;		// [0x1BD] CA CRL version. (0)
	uint8_t signer_crl_version;	// [0x1BE] Signer CRL version. (0)
	uint8_t enc_title_key[0x10];	// [0x1BF] Encrypted title key.
	uint8_t unknown1;		// [0x1CF] Unknown.
	uint64_t ticket_id;		// [0x1D0] Ticket ID. (IV for title key decryption for console-specific titles.)
	uint32_t console_id;		// [0x1D8] Console ID.
	RVL_TitleID_t title_id;		// [0x1DC] Title ID. (IV used for AES-CBC encryption.)

	// FIXME: Figure out the rest of the Wii U ticket format.
	uint8_t unknown_remaining[0x16C];
} WUP_Ticket;
ASSERT_STRUCT(WUP_Ticket, 0x350);
#pragma pack()

/**
 * Wii U content info.
 * This is located in the Wii U TMD header.
 *
 * Reference: https://github.com/Maschell/JNUSLib/blob/3eb299d3a150107f5c4d474650fe6d05d91bf64c/src/de/mas/wiiu/jnus/entities/TMD.java
 */
typedef struct _WUP_ContentInfo {
	uint16_t indexOffset;	// [0x000] Offset into the content table.
	uint16_t commandCount;	// [0x002] Number of contents to process.
	uint8_t sha256[32];	// [0x004] SHA-256 of the content entry.
} WUP_ContentInfo;
ASSERT_STRUCT(WUP_ContentInfo, 0x24);

/**
 * Wii U TMD header.
 * References:
 * - https://wiibrew.org/wiki/Title_metadata
 * - https://wiiubrew.org/wiki/Title_metadata
 * - https://github.com/Maschell/JNUSLib/blob/3eb299d3a150107f5c4d474650fe6d05d91bf64c/src/de/mas/wiiu/jnus/entities/TMD.java
 */
#pragma pack(1)
typedef struct PACKED _WUP_TMD_Header {
	RVL_TMD_Header rvl;		// [0x000] TMD header. (Identical to Wii format, with version set to 1.)
	uint8_t contentInfo_sha256[32];	// [0x1E4] SHA-256 of the ContentInfo table.

	// Following this header is the content info table,
	// which is followed by WUP_Content_Entry structs.
	// This may be followed by the CA and CP certs.
} WUP_TMD_Header;
ASSERT_STRUCT(WUP_TMD_Header, 0x204);
#pragma pack()

/**
 * Wii U content info table.
 * References:
 * - https://github.com/Maschell/JNUSLib/blob/3eb299d3a150107f5c4d474650fe6d05d91bf64c/src/de/mas/wiiu/jnus/entities/TMD.java
 */
#define WUP_CONTENTINFO_ENTRIES 0x40
typedef struct _WUP_TMD_ContentInfoTable {
	// NOTE: This table is protected by an SHA-256 hash
	// in the TMD header.
	WUP_ContentInfo info[WUP_CONTENTINFO_ENTRIES];

	// Following this table is the content table,
	// which has WUP_Content_Entry structs.
} WUP_TMD_ContentInfoTable;
ASSERT_STRUCT(WUP_TMD_ContentInfoTable, 0x900);

/**
 * Wii U content entry. (Stored after the TMD content info table.)
 * Reference: https://github.com/Maschell/JNUSLib/blob/3eb299d3a150107f5c4d474650fe6d05d91bf64c/src/de/mas/wiiu/jnus/entities/content/Content.java
 */
#pragma pack(1)
typedef struct PACKED _WUP_Content_Entry {
	uint32_t content_id;		// [0x000] Content ID
	uint16_t index;			// [0x004] Index
	uint16_t type;			// [0x006] Type (see RVL_Content_Type_e)
	uint64_t size;			// [0x008] Size
	uint8_t sha1_hash[20];		// [0x010] SHA-1 hash of the content (installed) or H3 table (disc).
	uint8_t unused[12];		// [0x024] Unused. (Maybe it was going to be used for SHA-256?)
} WUP_Content_Entry;
ASSERT_STRUCT(WUP_Content_Entry, 0x30);
#pragma pack()

#ifdef __cplusplus
}
#endif
