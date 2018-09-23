/***************************************************************************
 * RVT-H Tool (libwiicrypto)                                               *
 * gcn_structs.h: Nintendo GameCube and Wii data structures.               *
 *                                                                         *
 * Copyright (c) 2016-2018 by David Korth.                                 *
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

#ifndef __RVTHTOOL_LIBWIICRYPTO_GCN_STRUCTS_H__
#define __RVTHTOOL_LIBWIICRYPTO_GCN_STRUCTS_H__

#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

/**
 * GameCube/Wii disc image header.
 * This matches the disc image format exactly.
 *
 * NOTE: Strings are NOT null-terminated!
 */
#define GCN_MAGIC 0xC2339F3D
#define WII_MAGIC 0x5D1C9EA3
typedef struct PACKED _GCN_DiscHeader {
	union {
		char id6[6];	// [0x000] Game code. (ID6)
		struct {
			char id4[4];		// [0x000] Game code. (ID4)
			char company[2];	// [0x004] Company code.
		};
	};

	uint8_t disc_number;		// [0x006] Disc number.
	uint8_t revision;		// [0x007] Revision.
	uint8_t audio_streaming;	// [0x008] Audio streaming flag.
	uint8_t stream_buffer_size;	// [0x009] Streaming buffer size.

	uint8_t reserved1[14];		// [0x00A]

	uint32_t magic_wii;		// [0x018] Wii magic. (0x5D1C9EA3)
	uint32_t magic_gcn;		// [0x01C] GameCube magic. (0xC2339F3D)

	char game_title[64];		// [0x020] Game title.

	// Wii: Disc encryption status.
	// Normally 0 on retail and RVT-R (indicating the disc is encrypted).
	uint8_t hash_verify;		// [0x060] If non-zero, disable hash verification.
	uint8_t disc_noCrypt;		// [0x061] If non-zero, disable disc encryption.
	uint8_t reserved2[6];		// [0x062] Reserved.
} GCN_DiscHeader;
ASSERT_STRUCT(GCN_DiscHeader, 0x68);

/**
 * GameCube region codes.
 * Used in bi2.bin (GameCube) and RVL_RegionSetting.
 */
typedef enum {
	GCN_REGION_JAPAN = 0,		// Japan / Taiwan
	GCN_REGION_USA = 1,		// USA
	GCN_REGION_PAL = 2,		// Europe / Australia
	GCN_REGION_FREE = 3,		// Region-Free
	GCN_REGION_SOUTH_KOREA = 4,	// South Korea
} GCN_Region_Code;

/**
 * DVD Boot Block.
 * References:
 * - http://wiibrew.org/wiki/Wii_Disc#Decrypted
 * - http://hitmen.c02.at/files/yagcd/yagcd/chap13.html
 * - http://www.gc-forever.com/wiki/index.php?title=Apploader
 *
 * All fields are big-endian.
 */
#define GCN_Boot_Block_ADDRESS 0x420
typedef struct PACKED _GCN_Boot_Block {
	uint32_t bootFilePosition;	// NOTE: 34-bit RSH2 on Wii.
	uint32_t FSTPosition;		// NOTE: 34-bit RSH2 on Wii.
	uint32_t FSTLength;		// FST size. (NOTE: 34-bit RSH2 on Wii.)
	uint32_t FSTMaxLength;		// Size of biggest additional FST. (NOTE: 34-bit RSH2 on Wii.)

	uint32_t FSTAddress;	// FST address in RAM.
	uint32_t userPosition;	// Data area start. (Might be wrong; use FST.)
	uint32_t userLength;	// Data area length. (Might be wrong; use FST.)
	uint32_t reserved;
} GCN_Boot_Block;
ASSERT_STRUCT(GCN_Boot_Block, 32);

/**
 * DVD Boot Info. (bi2.bin)
 * Reference: http://www.gc-forever.com/wiki/index.php?title=Apploader
 *
 * All fields are big-endian.
 */
#define GCN_Boot_Info_ADDRESS 0x440
typedef struct PACKED _GCN_Boot_Info {
	uint32_t debugMonSize;		// Debug monitor size. [FIXME: Listed as signed?]
	uint32_t simMemSize;		// Simulated memory size. (bytes) [FIXME: Listed as signed?]
	uint32_t argOffset;		// Command line arguments.
	uint32_t debugFlag;		// Debug flag. (set to 3 if using CodeWarrior on GDEV)
	uint32_t TRKLocation;		// Target resident kernel location.
	uint32_t TRKSize;		// Size of TRK. [FIXME: Listed as signed?]
	uint32_t region_code;		// Region code. (See GCN_Region_Code.)
	uint32_t reserved1[3];
	uint32_t dolLimit;		// Maximum total size of DOL text/data sections. (0 == unlimited)
	uint32_t reserved2;
} GCN_Boot_Info;
ASSERT_STRUCT(GCN_Boot_Info, 48);

/**
 * DOL header.
 * Reference: http://wiibrew.org/wiki/DOL
 *
 * All fields are big-endian.
 */
typedef struct PACKED _DOL_Header {
	uint32_t textData[7];	// File offsets to Text sections.
	uint32_t dataData[11];	// File offsets to Data sections.
	uint32_t text[7];	// Load addresses for Text sections.
	uint32_t data[11];	// Load addresses for Data sections.
	uint32_t textLen[7];	// Section sizes for Text sections.
	uint32_t dataLen[11];	// Section sizes for Data sections.
	uint32_t bss;		// BSS address.
	uint32_t bssLen;	// BSS size.
	uint32_t entry;		// Entry point.
	uint8_t padding[28];	// Padding.
} DOL_Header;
ASSERT_STRUCT(DOL_Header, 256);

/**
 * AppLoader errors.
 *
 * Reference: https://www.gc-forever.com/wiki/index.php?title=Apploader
 */
typedef enum {
	// Unknown.
	APLERR_UNKNOWN			= 0,
	// No errors.
	APLERR_OK			= 1,

	// FSTLength > FSTMaxLength
	APLERR_FSTLENGTH,

	// Debug Monitor Size is not a multiple of 32.
	APLERR_DEBUGMONSIZE_UNALIGNED,

	// Simulated Memory Size is not a multiple of 32.
	APLERR_SIMMEMSIZE_UNALIGNED,

	// (PhysMemSize - SimMemSize) must be > DebugMonSize
	APLERR_PHYSMEMSIZE_MINUS_SIMMEMSIZE_NOT_GT_DEBUGMONSIZE,

	// Simulated Memory Size must be <= Physical Memory Size
	APLERR_SIMMEMSIZE_NOT_LE_PHYSMEMSIZE,

	// Illegal FST address. (must be < 0x81700000)
	APLERR_ILLEGAL_FST_ADDRESS,

	// DOL exceeds size limit.
	APLERR_DOL_EXCEEDS_SIZE_LIMIT,

	// DOL exceeds retail address limit.
	APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED,

	// DOL exceeds debug address limit.
	APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED,

	// Text segment is too big.
	APLERR_DOL_TEXTSEG2BIG,

	// Data segment is too big.
	APLERR_DOL_DATASEG2BIG,
} AppLoader_Error_e;

/** Wii-specific structs. **/

// 34-bit value stored in uint32_t.
// The value must be lshifted by 2.
typedef uint32_t uint34_rshift2_t;

/**
 * Wii volume group table.
 * Contains information about the (maximum of) four volume groups.
 * References:
 * - http://wiibrew.org/wiki/Wii_Disc#Partitions_information
 * - http://blog.delroth.net/2011/06/reading-wii-discs-with-python/
 *
 * All fields are big-endian.
 */
#define RVL_VolumeGroupTable_ADDRESS 0x40000
typedef struct PACKED _RVL_VolumeGroupTable {
	struct {
		uint32_t count;		// Number of partitions in this volume group.
		uint34_rshift2_t addr;	// Start address of this table, rshifted by 2.
	} vg[4];
} RVL_VolumeGroupTable;

/**
 * Wii partition table entry.
 * Contains information about an individual partition.
 * Reference: http://wiibrew.org/wiki/Wii_Disc#Partition_table_entry
 *
 * All fields are big-endian.
 */
typedef struct PACKED _RVL_PartitionTableEntry {
	uint34_rshift2_t addr;	// Start address of this partition, rshifted by 2.
	uint32_t type;		// Type of partition. (0 == Game, 1 == Update, 2 == Channel Installer, other = title ID)
} RVL_PartitionTableEntry;

// Wii ticket constants.
#define RVL_SIGNATURE_TYPE_RSA2048 0x10001
#define RVL_COMMON_KEY_INDEX_DEFAULT 0
#define RVL_COMMON_KEY_INDEX_KOREAN 1

/**
 * Time limit structs for Wii ticket.
 * Reference: http://wiibrew.org/wiki/Ticket
 */
typedef struct PACKED _RVL_TimeLimit {
	uint32_t enable;	// 1 == enable; 0 == disable
	uint32_t seconds;	// Time limit, in seconds.
} RVL_TimeLimit;
ASSERT_STRUCT(RVL_TimeLimit, 8);

/**
 * Title ID struct/union.
 * TODO: Verify operation on big-endian systems.
 */
typedef union PACKED _RVL_TitleID_t {
	uint64_t id;
	struct {
		uint32_t hi;
		uint32_t lo;
	};
	uint8_t u8[8];
} RVL_TitleID_t;
ASSERT_STRUCT(RVL_TitleID_t, 8);

/**
 * Wii ticket.
 * Reference: http://wiibrew.org/wiki/Ticket
 */
typedef struct PACKED _RVL_Ticket {
	uint32_t signature_type;	// [0x000] Always 0x10001 for RSA-2048.
	uint8_t signature[0x100];	// [0x004] Signature.
	uint8_t padding_sig[0x3C];	// [0x104] Padding. (always 0)

	// The following fields are all covered by the above signature.
	char issuer[0x40];		// [0x140] Signature issuer.
	uint8_t ecdh_data[0x3C];	// [0x180] ECDH data.
	uint8_t padding1[0x03];		// [0x1BC] Padding.
	uint8_t enc_title_key[0x10];	// [0x1BF] Encrypted title key.
	uint8_t unknown1;		// [0x1CF] Unknown.
	uint8_t ticket_id[0x08];	// [0x1D0] Ticket ID. (IV for title key decryption for console-specific titles.)
	uint8_t console_id[4];		// [0x1D8] Console ID.
	RVL_TitleID_t title_id;		// [0x1DC] Title ID. (IV used for AES-CBC encryption.)
	uint8_t unknown2[2];		// [0x1E4] Unknown, mostly 0xFFFF.
	uint8_t ticket_version[2];	// [0x1E6] Ticket version.
	uint32_t permitted_titles_mask;	// [0x1E8] Permitted titles mask.
	uint32_t permit_mask;		// [0x1EC] Permit mask.
	uint8_t title_export;		// [0x1F0] Title Export allowed using PRNG key. (1 == yes, 0 == no)
	uint8_t common_key_index;	// [0x1F1] Common Key index. (0 == default, 1 == Korean)
	uint8_t unknown3[0x30];		// [0x1F2] Unknown. (VC related?)
	uint8_t content_access_perm[0x40];	// [0x222] Content access permissions. (1 bit per content)
	uint8_t padding2[2];		// [0x262] Padding. (always 0)
	RVL_TimeLimit time_limits[8];	// [0x264] Time limits.
} RVL_Ticket;
ASSERT_STRUCT(RVL_Ticket, 0x2A4);

/**
 * Wii TMD header.
 * Reference: http://wiibrew.org/wiki/Title_metadata
 */
typedef struct PACKED _RVL_TMD_Header {
	uint32_t signature_type;	// [0x000] Always 0x10001 for RSA-2048.
	uint8_t signature[0x100];	// [0x004] Signature.
	uint8_t padding_sig[0x3C];	// [0x104] Padding. (always 0)

	// The following fields are all covered by the above signature.
	char issuer[0x40];		// [0x140] Signature issuer.
	uint8_t version;		// [0x180] Version.
	uint8_t ca_crl_version;		// [0x181] CA CRL version.
	uint8_t signer_crl_version;	// [0x182] Signer CRL version.
	uint8_t padding1;		// [0x183]
	RVL_TitleID_t sys_version;	// [0x184] System version. (IOS title ID)
	RVL_TitleID_t title_id;		// [0x18C] Title ID.
	uint32_t title_type;		// [0x194] Title type.
	uint16_t group_id;		// [0x198] Group ID.
	uint8_t reserved[62];		// [0x19A]
	uint32_t access_rights;		// [0x1D8] Access rights for e.g. DVD Video and AHBPROT.
	uint16_t title_version;		// [0x1DC] Title version.
	uint16_t nbr_cont;		// [0x1DE] Number of contents.
	uint16_t boot_index;		// [0x1E0] Boot index.
	uint8_t padding2[2];		// [0x1E2]

	// Following this header is a variable-length content table.
} RVL_TMD_Header;
ASSERT_STRUCT(RVL_TMD_Header, 0x1E4);

/**
 * Wii content entry. (Stored after the TMD.)
 * Reference: http://wiibrew.org/wiki/Title_metadata
 */
typedef struct PACKED _RVL_Content_Entry {
	uint32_t content_id;		// [0x000] Content ID
	uint16_t index;			// [0x004] Index
	uint16_t type;			// [0x006] Type
	uint64_t size;			// [0x008] Size
	uint8_t sha1_hash[20];		// [0x010] SHA-1 hash of the H3 table
} RVL_Content_Entry;
ASSERT_STRUCT(RVL_Content_Entry, 0x24);

/**
 * Wii partition header.
 * Reference: http://wiibrew.org/wiki/Wii_Disc#Partition
 */
typedef union PACKED _RVL_PartitionHeader {
	struct {
		RVL_Ticket ticket;			// [0x000]
		uint32_t tmd_size;			// [0x2A4] TMD size.
		uint34_rshift2_t tmd_offset;		// [0x2A8] TMD offset, rshifted by 2.
		uint32_t cert_chain_size;		// [0x2AC] Certificate chain size.
		uint34_rshift2_t cert_chain_offset;	// [0x2B0] Certificate chain offset, rshifted by 2.
		uint34_rshift2_t h3_table_offset;	// [0x2B4] H3 table offset, rshifted by 2. (Size is always 0x18000.)
		uint34_rshift2_t data_offset;		// [0x2B8] Data offset, rshifted by 2.
		uint34_rshift2_t data_size;		// [0x2BC] Data size, rshifted by 2.

		// The rest of the header contains the
		// certificate chain and the TMD.
		uint8_t data[0x7D40];			// [0x2C0] Certificate chain and TMD.
	};
	uint8_t u8[0x8000];
} RVL_PartitionHeader;
ASSERT_STRUCT(RVL_PartitionHeader, 0x8000);

/**
 * Country indexes in RVL_RegionSetting.ratings[].
 */
typedef enum {
	RVL_RATING_JAPAN = 0,		// CERO
	RVL_RATING_USA = 1,		// ESRB
	RVL_RATING_GERMANY = 3,		// USK
	RVL_RATING_PEGI = 4,		// PEGI
	RVL_RATING_FINLAND = 5,		// MEKU?
	RVL_RATING_PORTUGAL = 6,	// Modified PEGI
	RVL_RATING_BRITAIN = 7,		// BBFC
	RVL_RATING_AUSTRALIA = 8,	// AGCB
	RVL_RATING_SOUTH_KOREA = 9,	// GRB
} RVL_RegionSetting_RatingCountry;

/**
 * Region setting and age ratings.
 * Reference: http://wiibrew.org/wiki/Wii_Disc#Region_setting
 */
#define RVL_RegionSetting_ADDRESS 0x4E000
typedef struct PACKED _RVL_RegionSetting {
	uint32_t region_code;	// Region code. (See GCN_Region_Code.)
	uint8_t reserved[12];
	uint8_t ratings[0x10];	// Country-specific age ratings.
} RVL_RegionSetting;

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBWIICRYPTO_GCN_STRUCTS_H__ */
