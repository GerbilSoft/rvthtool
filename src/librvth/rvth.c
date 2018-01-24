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

#include "rvth.h"
#include "rvth_p.h"

#include "byteswap.h"
#include "nhcd_structs.h"
#include "gcn_structs.h"
#include "cert_store.h"
#include "cert.h"

// Disc image readers.
#include "reader_plain.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// NDDEMO header.
// Used in early GameCube tech demos.
// Note the lack of a GameCube magic number.
static const uint8_t nddemo_header[64] = {
	0x30, 0x30, 0x00, 0x45, 0x30, 0x31, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x4E, 0x44, 0x44, 0x45, 0x4D, 0x4F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/**
 * Trim a game title.
 * @param title Game title.
 * @param size Size of game title.
 */
static void trim_title(char *title, int size)
{
	size--;
	for (; size >= 0; size--) {
		if (title[size] != ' ')
			break;
		title[size] = 0;
	}
}

/**
 * Parse an RVT-H timestamp.
 * @param nhcd_timestamp Pointer to NHCD timestamp string.
 * @return Timestamp, or -1 if invalid.
 */
static time_t rvth_parse_timestamp(const char *nhcd_timestamp)
{
	// Date format: "YYYYMMDD"
	// Time format: "HHMMSS"
	unsigned int ymd = 0;
	unsigned int hms = 0;
	unsigned int i;
	struct tm ymdtime;

	// Convert the date to an unsigned integer.
	for (i = 0; i < 8; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		ymd *= 10;
		ymd += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be higher than 19000101.
	// - Must be lower than 99991231.
	if (unlikely(ymd < 19000101 || ymd > 99991231)) {
		// Invalid date.
		return -1;
	}

	// Convert the time to an unsigned integer.
	for (i = 8; i < 14; i++) {
		if (unlikely(!isdigit(nhcd_timestamp[i]))) {
			// Invalid digit.
			return -1;
		}
		hms *= 10;
		hms += (nhcd_timestamp[i] & 0xF);
	}

	// Sanity checks:
	// - Must be lower than 235959.
	if (unlikely(hms > 235959)) {
		// Invalid time.
		return -1;
	}

	// Convert to Unix time.
	// NOTE: struct tm has some oddities:
	// - tm_year: year - 1900
	// - tm_mon: 0 == January
	ymdtime.tm_year = (ymd / 10000) - 1900;
	ymdtime.tm_mon  = ((ymd / 100) % 100) - 1;
	ymdtime.tm_mday = ymd % 100;

	ymdtime.tm_hour = (hms / 10000);
	ymdtime.tm_min  = (hms / 100) % 100;
	ymdtime.tm_sec  = hms % 100;

	// tm_wday and tm_yday are output variables.
	ymdtime.tm_wday = 0;
	ymdtime.tm_yday = 0;
	ymdtime.tm_isdst = 0;

	// If conversion fails, this will return -1.
	return timegm(&ymdtime);
}

/**
 * Find the game partition in a Wii disc image.
 * @param reader	[in] Reader*
 * @return Game partition LBA (relative to start of reader), or 0 on error.
 */
uint32_t rvth_find_GamePartition(Reader *reader)
{
	// Assuming this is a valid Wii disc image.
	int64_t addr;
	unsigned int ptcount, i;
	uint32_t lba_size;

	// Volume group and partition table.
	// NOTE: Only reading the first partition table,
	// which we're assuming is stored directly after
	// the volume group table.
	// FIXME: Handle other cases when using direct device access on Windows.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		struct {
			RVL_VolumeGroupTable vgtbl;
			RVL_PartitionTableEntry ptbl[16];
		};
	} pt;

	// Get the volume group table.
	lba_size = reader_read(reader, &pt, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 1);
	if (lba_size != 1) {
		// Read error.
		return 0;
	}

	// Game partition is always in volume group 0.
	addr = ((int64_t)be32_to_cpu(pt.vgtbl.vg[0].addr) << 2);
	if (addr != (RVL_VolumeGroupTable_ADDRESS + sizeof(pt.vgtbl))) {
		// Partition table offset isn't supported right now.
		return 0;
	}

	ptcount = be32_to_cpu(pt.vgtbl.vg[0].count);
	for (i = 0; i < ptcount; i++) {
		if (be32_to_cpu(pt.ptbl[i].type == 0)) {
			// Found the game partition.
			return (be32_to_cpu(pt.ptbl[i].addr) / (RVTH_BLOCK_SIZE/4));
		}
	}

	// No game partition found...
	return 0;
}

/**
 * Initialize the GCN Disc Header fields in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @param discHeader	[in] GCN_DiscHeader
 */
static void rvth_init_BankEntry_gcn(RvtH_BankEntry *entry, GCN_DiscHeader *discHeader)
{
	// Copy the game ID.
	memcpy(entry->id6, discHeader->id6, 6);

	// Disc number and revision.
	entry->disc_number = discHeader->disc_number;
	entry->revision    = discHeader->revision;

	// Read the disc title.
	// TODO: Convert from Shift-JIS to UTF-8?
	memcpy(entry->game_title, discHeader->game_title, 64);
	// Remove excess spaces.
	trim_title(entry->game_title, 64);
}

/**
 * Set the region field in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_init_BankEntry_region(RvtH_BankEntry *entry)
{
	uint32_t lba_size;
	uint32_t lba_region;
	bool is_wii = false;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		struct {
			uint8_t bi2_pad[0x40];
			GCN_Boot_Info bi2;
		};
		RVL_RegionSetting rvl_region;
	} sector_buf;

	assert(entry->reader != NULL);

	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty bank.
			entry->region_code = 0xFF;
			return RVTH_ERROR_BANK_EMPTY;
		case RVTH_BankType_Unknown:
		default:
			// Unknown bank.
			entry->region_code = 0xFF;
			return RVTH_ERROR_BANK_UNKNOWN;
		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			entry->region_code = 0xFF;
			return RVTH_ERROR_BANK_DL_2;

		case RVTH_BankType_GCN:
			// GameCube image. Read bi2.bin.
			lba_region = BYTES_TO_LBA(GCN_Boot_Info_ADDRESS);
			is_wii = false;
			break;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Wii disc image. Read the region settings.
			lba_region = BYTES_TO_LBA(RVL_RegionSetting_ADDRESS);
			is_wii = true;
			break;
	}

	// Read the LBA containing the region code.
	lba_size = reader_read(entry->reader, sector_buf.u8, lba_region, 1);
	if (lba_size != 1) {
		// Error reading the region code.
		return -EIO;
	}

	// FIXME: region_code is a 32-bit value,
	// but only the first few bits are used...
	entry->region_code = (is_wii
		? (uint8_t)be32_to_cpu(sector_buf.rvl_region.region_code)
		: (uint8_t)be32_to_cpu(sector_buf.bi2.region_code)
		);
	return 0;
}

/**
 * Set the crypto_type and sig_type fields in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @param discHeader	[in] GCN_DiscHeader
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
static int rvth_init_BankEntry_crypto(RvtH_BankEntry *entry, const GCN_DiscHeader *discHeader)
{
	uint32_t lba_game;	// LBA of the Game Partition.
	uint32_t lba_size;
	uint32_t tmd_size;
	RVL_Cert_Issuer issuer;
	int ret;

	// Partition header.
	RVL_PartitionHeader header;
	const RVL_TMD_Header *tmd;

	assert(entry->reader != NULL);

	// Clear the ticket/TMD crypto information initially.
	memset(&entry->ticket, 0, sizeof(entry->ticket));
	memset(&entry->tmd, 0, sizeof(entry->tmd));

	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty bank.
			return RVTH_ERROR_BANK_EMPTY;
		case RVTH_BankType_Unknown:
		default:
			// Unknown bank.
			return RVTH_ERROR_BANK_UNKNOWN;
		case RVTH_BankType_GCN:
			// GameCube image. No encryption.
			// TODO: Technically no signature either...
			entry->crypto_type = RVTH_CryptoType_None;
			return 0;
		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Wii disc image.
			break;
	}

	// Check for an unencrypted RVT-H image.
	if (discHeader->hash_verify != 0 && discHeader->disc_noCrypt != 0) {
		// Unencrypted.
		// TODO: I haven't seen any images where only one of these
		// is zero or non-zero, but that may show up...
		entry->crypto_type = RVTH_CryptoType_None;
	}

	// Find the game partition.
	// TODO: Error checking.
	lba_game = rvth_find_GamePartition(entry->reader);
	if (lba_game == 0) {
		// No game partition...
		return RVTH_ERROR_NO_GAME_PARTITION;
	}

	// Found the game partition.
	// Read the partition header.
	lba_size = reader_read(entry->reader, &header, lba_game, BYTES_TO_LBA(sizeof(header)));
	if (lba_size != BYTES_TO_LBA(sizeof(header))) {
		// Error reading the partition header.
		return -EIO;
	}

	// Check the ticket signature issuer.
	issuer = cert_get_issuer_from_name(header.ticket.signature_issuer);
	switch (issuer) {
		case RVL_CERT_ISSUER_RETAIL_TICKET:
			// Retail certificate.
			entry->ticket.sig_type = RVTH_SigType_Retail;
			break;
		case RVL_CERT_ISSUER_DEBUG_TICKET:
			// Debug certificate.
			entry->ticket.sig_type = RVTH_SigType_Debug;
			break;
		default:
			// Unknown issuer, or not valid for ticket.
			break;
	}

	// Validate the signature.
	ret = cert_verify((const uint8_t*)&header.ticket, sizeof(header.ticket));
	if (ret != 0) {
		// Signature verification error.
		if (ret > 0 && ((ret & SIG_ERROR_MASK) == SIG_ERROR_INVALID)) {
			// Invalid signature.
			entry->ticket.sig_status = (ret & SIG_FAIL_HASH_FAKE
				? RVTH_SigStatus_Fake
				: RVTH_SigStatus_Invalid);
		} else {
			// Other error.
			entry->ticket.sig_status = RVTH_SigStatus_Unknown;
		}
	} else {
		// Signature is valid.
		entry->ticket.sig_status = RVTH_SigStatus_OK;
	}

	// Check the TMD signature issuer.
	// TODO: Verify header.tmd_offset?
	tmd = (const RVL_TMD_Header*)header.tmd;
	issuer = cert_get_issuer_from_name(tmd->signature_issuer);
	switch (issuer) {
		case RVL_CERT_ISSUER_RETAIL_TMD:
			// Retail certificate.
			entry->tmd.sig_type = RVTH_SigType_Retail;
			break;
		case RVL_CERT_ISSUER_DEBUG_TMD:
			// Debug certificate.
			entry->tmd.sig_type = RVTH_SigType_Debug;
			break;
		default:
			// Unknown issuer, or not valid for TMD.
			break;
	}

	// Check the TMD size.
	tmd_size = be32_to_cpu(header.tmd_size);
	if (tmd_size <= sizeof(header.tmd)) {
		// TMD is not too big. We can validate the signature.
		ret = cert_verify(header.tmd, tmd_size);
		if (ret != 0) {
			// Signature verification error.
			if (ret > 0 && ((ret & SIG_ERROR_MASK) == SIG_ERROR_INVALID)) {
				// Invalid signature.
				entry->tmd.sig_status = (ret & SIG_FAIL_HASH_FAKE
					? RVTH_SigStatus_Fake
					: RVTH_SigStatus_Invalid);
			} else {
				// Other error.
				entry->tmd.sig_status = RVTH_SigStatus_Unknown;
			}
		} else {
			// Signature is valid.
			entry->tmd.sig_status = RVTH_SigStatus_OK;
		}
	}

	// If encrypted, check the crypto type.
	// NOTE: Retail + unencrypted is usually an error.
	// FIXME: Common key index seems to have wacky values in homebrew WADs.
	// - USB Loader GX UNEO Forwarder v5.1: 0x06
	// - Nintendont forwarder: 0xB7
	if (entry->crypto_type != RVTH_CryptoType_None) {
		switch (entry->ticket.sig_type) {
			case RVTH_SigType_Retail:
				// Retail may be either Common Key or Korean Key.
				switch (header.ticket.common_key_index) {
					case 0:
						entry->crypto_type = RVTH_CryptoType_Retail;
						break;
					case 1:
						entry->crypto_type = RVTH_CryptoType_Korean;
						break;
					default:
						entry->crypto_type = RVTH_CryptoType_Unknown;
						break;
				}
				break;

			case RVTH_SigType_Debug:
				// There's only one debug key.
				if (header.ticket.common_key_index == 0) {
					entry->crypto_type = RVTH_CryptoType_Debug;
				} else {
					entry->crypto_type = RVTH_CryptoType_Unknown;
				}
				break;

			default:
				// Should not happen...
				entry->crypto_type = RVTH_CryptoType_Unknown;
				break;
		}
	}

	return 0;
}

/**
 * Initialize an RVT-H bank entry from an opened HDD image.
 * @param entry			[out] RvtH_BankEntry
 * @param f_img			[in] RefFile*
 * @param type			[in] Bank type. (See RvtH_BankType_e.)
 * @param lba_start		[in] Starting LBA.
 * @param lba_len		[in] Length, in LBAs.
 * @param nhcd_timestamp	[in] Timestamp string pointer from the bank table.
 * @return 0 on success; negative POSIX error code on error.
 */
static int rvth_init_BankEntry(RvtH_BankEntry *entry, RefFile *f_img,
	uint8_t type, uint32_t lba_start, uint32_t lba_len,
	const char *nhcd_timestamp)
{
	int ret;
	size_t size;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
	} sector_buf;

	// Initialize the standard properties.
	memset(entry, 0, sizeof(*entry));
	entry->lba_start = lba_start;
	entry->lba_len = lba_len;
	entry->type = type;
	entry->timestamp = -1;	// Default to "no timestamp".

	assert(entry->type < RVTH_BankType_MAX);
	if (type == RVTH_BankType_Unknown ||
	    type >= RVTH_BankType_MAX)
	{
		// Unknown entry type.
		// Don't bother reading anything else.
		return 0;
	}

	// Read the GCN disc header.
	// TODO: For non-deleted banks, verify the magic number?
	errno = 0;
	ret = ref_seeko(f_img, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}
	size = ref_read(sector_buf.u8, 1, sizeof(sector_buf.u8), f_img);
	if (size != sizeof(sector_buf.u8)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		// TODO: Mark the bank as invalid?
		return -err;
	}

	// If the entry type is Empty, check for GCN/Wii magic.
	if (type == RVTH_BankType_Empty) {
		if (sector_buf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
			// Wii disc image.
			// TODO: Detect SL vs. DL.
			// TODO: Actual disc image size?
			type = RVTH_BankType_Wii_SL;
			entry->is_deleted = true;
		} else if (sector_buf.gcn.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
			// GameCube disc image.
			// TODO: Actual disc image size?
			type = RVTH_BankType_GCN;
			entry->is_deleted = true;
		} else {
			// Check for GameCube NDDEMO.
			if (!memcmp(&sector_buf.gcn, nddemo_header, sizeof(nddemo_header))) {
				// NDDEMO header found.
				type = RVTH_BankType_GCN;
				entry->is_deleted = true;
			} else {
				// Probably actually empty...
			}
		}
		entry->type = type;
	}

	if (lba_len == 0) {
		if (lba_start < NHCD_BANKTABLE_ADDRESS_LBA) {
			// Bank starts before the bank table.
			// This is a relocated Bank 1 on a device with
			// an extended bank table, so it can only support
			// GCN disc images.
			lba_len = NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA;
		} else {
			// Use the default LBA length based on bank type.
			switch (type) {
				default:
				case RVTH_BankType_Empty:
				case RVTH_BankType_Unknown:
				case RVTH_BankType_Wii_SL:
				case RVTH_BankType_Wii_DL_Bank2:
					// Full bank.
					lba_len = NHCD_BANK_WII_SL_SIZE_RVTR_LBA;
					break;

				case RVTH_BankType_Wii_DL:
					// Dual-layer bank.
					lba_len = NHCD_BANK_WII_DL_SIZE_RVTR_LBA;
					break;

				case RVTH_BankType_GCN:
					// GameCube disc image.
					lba_len = NHCD_BANK_GCN_SIZE_NR_LBA;
					break;
			}
		}
		entry->lba_len = lba_len;
	}

	if (type == RVTH_BankType_Empty) {
		// We're done here.
		return 0;
	}

	// Initialize the disc image reader.
	// NOTE: Always the plain reader for RVT-H HDD images.
	entry->reader = reader_plain_open(f_img, lba_start, lba_len);

	// Parse the timestamp.
	if (nhcd_timestamp) {
		entry->timestamp = rvth_parse_timestamp(nhcd_timestamp);
	}

	// Initialize fields from the disc header.
	rvth_init_BankEntry_gcn(entry, &sector_buf.gcn);

	// TODO: Error handling.
	// Initialize the region code.
	rvth_init_BankEntry_region(entry);
	// Initialize the encryption status.
	rvth_init_BankEntry_crypto(entry, &sector_buf.gcn);

	// We're done here.
	return 0;
}

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
	static const char *const errtbl[RVTH_ERROR_MAX] = {
		// tr: RVTH_ERROR_SUCCESS
		"Success",
		// tr: RVTH_ERROR_UNRECOGNIZED_FILE
		"Unrecognized file format",
		// tr: RVTH_ERROR_NHCD_TABLE_MAGIC
		"Bank table magic is incorrect",
		// tr: RVTH_ERROR_NO_BANKS
		"No banks found",
		// tr: RVTH_ERROR_BANK_UNKNOWN
		"Bank status is unknown",
		// tr: RVTH_ERROR_BANK_EMPTY
		"Bank is empty",
		// tr: RVTH_ERROR_BANK_DL_2
		"Bank is second bank of a dual-layer image",
		// tr: RVTH_ERROR_NOT_A_DEVICE
		"Operation can only be performed on a device, not an image file",
		// tr: RVTH_ERROR_BANK_IS_DELETED
		"Bank is deleted",
		// tr: RVTH_ERROR_BANK_NOT_DELETED
		"Bank is not deleted",
		// tr: RVTH_ERROR_NOT_HDD_IMAGE
		"RVT-H object is not an HDD image",
		// tr: RVTH_ERROR_NO_GAME_PARTITION
		"Wii game partition not found",
		// tr: RVTH_ERROR_INVALID_BANK_COUNT
		"RVT-H bank count field is invalid",
		// tr: RVTH_ERROR_IS_HDD_IMAGE
		"Operation cannot be performed on devices or HDD images",
		// tr: RVTH_ERROR_IS_RETAIL_CRYPTO
		"Cannot import a retail-encrypted Wii game",
		// tr: RVTH_ERROR_IMAGE_TOO_BIG
		"Source image does not fit in an RVT-H bank",
		// tr: RVTH_ERROR_BANK_NOT_EMPTY_OR_DELETED
		"Destination bank is not empty or deleted",
	};

	if (err < 0) {
		return strerror(-err);
	} else if (err >= ARRAY_SIZE(errtbl)) {
		return "(unknown)";
	}

	return errtbl[err];
}

/**
 * Open a Wii or GameCube disc image.
 * @param f_img	[in] RefFile*
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
static RvtH *rvth_open_gcm(RefFile *f_img, int *pErr)
{
	RvtH *rvth;
	RvtH_BankEntry *entry;
	int ret;
	size_t size;

	int64_t len;
	uint8_t type;

	// Sector buffer.
	union {
		uint8_t u8[RVTH_BLOCK_SIZE];
		GCN_DiscHeader gcn;
	} sector_buf;

	// TODO: Detect CISO and WBFS.

	// Get the file length.
	// FIXME: This is obtained in rvth_open().
	// Pass it as a parameter?
	ret = ref_seeko(f_img, 0, SEEK_END);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}
	len = ref_tello(f_img);

	// Rewind back to the beginning of the file.
	ret = ref_seeko(f_img, 0, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}

	// Check the disc header.
	errno = 0;
	size = ref_read(sector_buf.u8, 1, sizeof(sector_buf.u8), f_img);
	if (size != sizeof(sector_buf.u8)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Check for GameCube and/or Wii magic numbers.
	if (sector_buf.gcn.magic_wii == cpu_to_be32(WII_MAGIC)) {
		// Wii disc image.
		if (len > LBA_TO_BYTES(NHCD_BANK_WII_SL_SIZE_RVTR_LBA)) {
			// Dual-layer Wii disc image.
			// TODO: Check for retail vs. RVT-R?
			type = RVTH_BankType_Wii_DL;
		} else {
			// Single-layer Wii disc image.
			type = RVTH_BankType_Wii_SL;
		}
	} else if (sector_buf.gcn.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
		// GameCube disc image.
		type = RVTH_BankType_GCN;
	} else {
		// Check for GameCube NDDEMO.
		if (!memcmp(&sector_buf.gcn, nddemo_header, sizeof(nddemo_header))) {
			// NDDEMO header found.
			type = RVTH_BankType_GCN;
		} else {
			// Not supported.
			errno = EIO;
			if (pErr) {
				*pErr = -EIO;
			}
			return NULL;
		}
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Allocate memory for a single RvtH_BankEntry object.
	rvth->bank_count = 1;
	rvth->is_hdd = false;
	rvth->entries = calloc(1, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	// Initialize the bank entry.
	// NOTE: Not using rvth_init_BankEntry() here.
	rvth->f_img = f_img;
	entry = rvth->entries;
	entry->lba_start = 0;
	entry->lba_len = BYTES_TO_LBA(len);
	entry->type = type;
	entry->is_deleted = false;

	// Initialize the disc image reader.
	// TODO: Handle CISO and WBFS for standalone disc images.
	entry->reader = reader_plain_open(f_img, entry->lba_start, entry->lba_len);

	// Timestamp.
	// TODO: Get the timestamp from the file.
	entry->timestamp = -1;

	// Initialize fields from the disc header.
	rvth_init_BankEntry_gcn(entry, &sector_buf.gcn);

	// TODO: Error handling.
	// Initialize the region code.
	rvth_init_BankEntry_region(entry);
	// Initialize the encryption status.
	rvth_init_BankEntry_crypto(entry, &sector_buf.gcn);

	// Disc image loaded.
	return rvth;
}

/**
 * Open an RVT-H disk image.
 * @param f_img	[in] RefFile*
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
static RvtH *rvth_open_hdd(RefFile *f_img, int *pErr)
{
	NHCD_BankTable_Header nhcd_header;
	RvtH *rvth;
	RvtH_BankEntry *rvth_entry;
	int ret;
	unsigned int i;
	int64_t addr;
	size_t size;

	// Check the bank table header.
	errno = 0;
	ret = ref_seeko(f_img, LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}
	errno = 0;
	size = ref_read(&nhcd_header, 1, sizeof(nhcd_header), f_img);
	if (size != sizeof(nhcd_header)) {
		// Short read.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Check the magic number.
	if (nhcd_header.magic != be32_to_cpu(NHCD_BANKTABLE_MAGIC)) {
		// Incorrect magic number.
		if (pErr) {
			*pErr = RVTH_ERROR_NHCD_TABLE_MAGIC;
		}
		errno = EIO;
		return NULL;
	}

	// Allocate memory for the RvtH object
	errno = 0;
	rvth = malloc(sizeof(RvtH));
	if (!rvth) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	}

	// Get the bank count.
	rvth->bank_count = be32_to_cpu(nhcd_header.bank_count);
	if (rvth->bank_count < 8 || rvth->bank_count > 32) {
		// Bank count is either too small or too large.
		// RVT-H systems are set to 8 banks at the factory,
		// but we're supporting up to 32 in case the user
		// has modified it.
		// TODO: More extensive "extra bank" testing.
		free(rvth);
		errno = EIO;
		if (pErr) {
			*pErr = RVTH_ERROR_INVALID_BANK_COUNT;
		}
		return NULL;
	}

	// Allocate memory for the 8 RvtH_BankEntry objects.
	rvth->is_hdd = true;
	rvth->entries = calloc(rvth->bank_count, sizeof(RvtH_BankEntry));
	if (!rvth->entries) {
		// Error allocating memory.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(rvth);
		errno = err;
		if (pErr) {
			*pErr = -err;
		}
		return NULL;
	};

	rvth->f_img = ref_dup(f_img);
	rvth_entry = rvth->entries;
	addr = (uint32_t)(LBA_TO_BYTES(NHCD_BANKTABLE_ADDRESS_LBA) + NHCD_BLOCK_SIZE);
	for (i = 0; i < rvth->bank_count; i++, rvth_entry++, addr += 512) {
		NHCD_BankEntry nhcd_entry;
		uint32_t lba_start = 0, lba_len = 0;
		uint8_t type = RVTH_BankType_Unknown;

		if (i > 0 && (rvth_entry-1)->type == RVTH_BankType_Wii_DL) {
			// Second bank for a dual-layer Wii image.
			rvth_entry->type = RVTH_BankType_Wii_DL_Bank2;
			rvth_entry->timestamp = -1;
			continue;
		}

		errno = 0;
		ret = ref_seeko(f_img, addr, SEEK_SET);
		if (ret != 0) {
			// Seek error.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		errno = 0;
		size = ref_read(&nhcd_entry, 1, sizeof(nhcd_entry), f_img);
		if (size != sizeof(nhcd_entry)) {
			// Short read.
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			rvth_close(rvth);
			errno = err;
			return NULL;
		}

		// Check the type.
		switch (be32_to_cpu(nhcd_entry.type)) {
			default:
				// Unknown bank type...
				type = RVTH_BankType_Unknown;
				break;
			case NHCD_BankType_Empty:
				// "Empty" bank. May have a deleted image.
				type = RVTH_BankType_Empty;
				break;
			case NHCD_BankType_GCN:
				// GameCube
				type = RVTH_BankType_GCN;
				break;
			case NHCD_BankType_Wii_SL:
				// Wii (single-layer)
				type = RVTH_BankType_Wii_SL;
				break;
			case NHCD_BankType_Wii_DL:
				// Wii (dual-layer)
				// TODO: Cannot start in Bank 8.
				type = RVTH_BankType_Wii_DL;
				break;
		}

		// For valid types, use the listed LBAs if they're non-zero.
		if (type >= RVTH_BankType_GCN) {
			lba_start = be32_to_cpu(nhcd_entry.lba_start);
			lba_len = be32_to_cpu(nhcd_entry.lba_len);
		}

		if (lba_start == 0 || lba_len == 0) {
			// Invalid LBAs. Use the default starting offset.
			// Bank size will be determined by rvth_init_BankEntry().
			lba_start = NHCD_BANK_START_LBA(i, rvth->bank_count);
			lba_len = 0;
		}

		// Initialize the bank entry.
		rvth_init_BankEntry(rvth_entry, f_img, type,
			lba_start, lba_len, nhcd_entry.timestamp);
	}

	// RVT-H image loaded.
	return rvth;
}

/**
 * Open an RVT-H disk image, GameCube disc image, or Wii disc image.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a supported image; NULL on error. (check errno)
 */
RvtH *rvth_open(const TCHAR *filename, int *pErr)
{
	RefFile *f_img;
	RvtH *rvth = NULL;
	int ret = 0;
	int64_t len;

	// Open the disk image.
	f_img = ref_open(filename);
	if (!f_img) {
		// Could not open the file.
		if (pErr) {
			*pErr = -errno;
		}
		return NULL;
	}

	// Determine if this is an HDD image or a disc image.
	ret = ref_seeko(f_img, 0, SEEK_END);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		ret = -errno;
		goto end;
	}
	len = ref_tello(f_img);
	if (len == 0) {
		// File is empty.
		errno = EIO;
	} else if (len <= 2*LBA_TO_BYTES(NHCD_BANK_SIZE_LBA)) {
		// Two banks or less.
		// This is most likely a standalone disc image.
		rvth = rvth_open_gcm(f_img, pErr);
	} else {
		// More than two banks.
		// This is most likely an RVT-H HDD image.
		rvth = rvth_open_hdd(f_img, pErr);
	}

end:
	if (pErr) {
		*pErr = -errno;
	}
	// If the RvtH object was opened, it will have
	// called ref_dup() to increment the reference count.
	ref_close(f_img);
	return rvth;
}

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth)
{
	unsigned int i;

	if (!rvth)
		return;

	// Close all bank entry files.
	// RefFile has a reference count, so we have to clear the count.
	for (i = 0; i < rvth->bank_count; i++) {
		if (rvth->entries[i].reader) {
			reader_close(rvth->entries[i].reader);
		}
	}

	// Free the bank entries array.
	free(rvth->entries);

	// Clear the main reference.
	if (rvth->f_img) {
		ref_close(rvth->f_img);
	}

	free(rvth);
}

/**
 * Is this RVT-H object an HDD image or a standalone disc image?
 * @param rvth RVT-H object.
 * @return True if the RVT-H object is an HDD image; false if it's a standalone disc image.
 */
bool rvth_is_hdd(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return false;
	}
	return rvth->is_hdd;
}

/**
 * Get the number of banks in an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 * @return Number of banks.
 */
unsigned int rvth_get_BankCount(const RvtH *rvth)
{
	if (!rvth) {
		errno = EINVAL;
		return 0;
	}

	return rvth->bank_count;
}

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry, or NULL if out of range.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr)
{
	if (!rvth) {
		errno = EINVAL;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	} else if (bank >= rvth->bank_count) {
		errno = ERANGE;
		if (pErr) {
			*pErr = -EINVAL;
		}
		return NULL;
	}

	return &rvth->entries[bank];
}
