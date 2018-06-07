/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * bank_init.c: RvtH_BankEntry initialization functions.                   *
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

#include "bank_init.h"

#include "byteswap.h"
#include "cert.h"
#include "disc_header.h"
#include "nhcd_structs.h"
#include "ptbl.h"
#include "rvth_time.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <string.h>

/**
 * Set the region field in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_region(RvtH_BankEntry *entry)
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
 * The reader and discHeader fields must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_crypto(RvtH_BankEntry *entry)
{
	const pt_entry_t *game_pte;	// Game partition entry.
	uint32_t lba_size;
	uint32_t tmd_size;
	int ret;

	// Partition header.
	RVL_PartitionHeader header;
	const RVL_TMD_Header *tmdHeader;

	assert(entry->reader != NULL);

	// Clear the ticket/TMD crypto information initially.
	entry->ios_version = 0;
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
	if (entry->discHeader.hash_verify != 0 && entry->discHeader.disc_noCrypt != 0) {
		// Unencrypted.
		// TODO: I haven't seen any images where only one of these
		// is zero or non-zero, but that may show up...
		entry->crypto_type = RVTH_CryptoType_None;
	}

	// Find the game partition.
	// TODO: Error checking.
	game_pte = rvth_ptbl_find_game(entry);
	if (!game_pte) {
		// No game partition...
		return RVTH_ERROR_NO_GAME_PARTITION;
	}

	// Found the game partition.
	// Read the partition header.
	lba_size = reader_read(entry->reader, &header, game_pte->lba_start, BYTES_TO_LBA(sizeof(header)));
	if (lba_size != BYTES_TO_LBA(sizeof(header))) {
		// Error reading the partition header.
		return -EIO;
	}

	// Check the ticket signature issuer.
	switch (cert_get_issuer_from_name(header.ticket.issuer)) {
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
			entry->ticket.sig_status = ((ret & SIG_FAIL_HASH_FAKE)
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
	tmdHeader = (const RVL_TMD_Header*)header.data;
	switch (cert_get_issuer_from_name(tmdHeader->issuer)) {
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
	if (tmd_size <= sizeof(header.data)) {
		// TMD is not too big. We can validate the signature.
		ret = cert_verify(header.data, tmd_size);
		if (ret != 0) {
			// Signature verification error.
			if (ret > 0 && ((ret & SIG_ERROR_MASK) == SIG_ERROR_INVALID)) {
				// Invalid signature.
				entry->tmd.sig_status = ((ret & SIG_FAIL_HASH_FAKE)
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

	// Get the required IOS version.
	if (be32_to_cpu(tmdHeader->sys_version.hi) == 1) {
		uint32_t ios_tid_lo = be32_to_cpu(tmdHeader->sys_version.lo);
		if (ios_tid_lo < 256) {
			entry->ios_version = (uint8_t)ios_tid_lo;
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
int rvth_init_BankEntry(RvtH_BankEntry *entry, RefFile *f_img,
	uint8_t type, uint32_t lba_start, uint32_t lba_len,
	const char *nhcd_timestamp)
{
	uint32_t reader_lba_len;
	bool isDeleted;

	int ret;	// errno or RvtH_Errors

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
	ret = rvth_disc_header_get(f_img, lba_start, &entry->discHeader, &isDeleted);
	if (ret < 0) {
		// Error...
		// TODO: Mark the bank as invalid?
		memset(&entry->discHeader, 0, sizeof(entry->discHeader));
		return ret;
	}

	// If the original bank type is empty, or the disc header is zeroed,
	// this bank is deleted.
	entry->is_deleted = (isDeleted | (type == RVTH_BankType_Empty && ret >= RVTH_BankType_GCN));
	if (entry->is_deleted) {
		// Bank type was determined by rvth_disc_header_get().
		type = (uint8_t)ret;
		entry->type = type;
	}

	// Determine the maximum LBA length for the Reader:
	// - GCN or Wii SL: Full bank size.
	// - Wii DL: Dual-layer bank size.
	// - First bank in extended bank table: Smaller bank size.
	if (lba_start < NHCD_BANKTABLE_ADDRESS_LBA) {
		// Bank starts before the bank table.
		// This is a relocated Bank 1 on a device with
		// an extended bank table, so it can only support
		// GCN disc images.
		reader_lba_len = NHCD_EXTBANKTABLE_BANK_1_SIZE_LBA;
	} else {
		// Use the default LBA length based on bank type.
		switch (type) {
			default:
			case RVTH_BankType_Empty:
			case RVTH_BankType_Unknown:
			case RVTH_BankType_GCN:
			case RVTH_BankType_Wii_SL:
			case RVTH_BankType_Wii_DL_Bank2:
				// Full bank.
				reader_lba_len = NHCD_BANK_WII_SL_SIZE_RVTR_LBA;
				break;

			case RVTH_BankType_Wii_DL:
				// Dual-layer bank.
				reader_lba_len = NHCD_BANK_WII_DL_SIZE_RVTR_LBA;
				break;
		}
	}

	if (lba_len == 0) {
		// Empty bank. Assume the length matches the bank,
		// except for GameCube.
		lba_len = (type == RVTH_BankType_GCN)
			? NHCD_BANK_GCN_SIZE_NR_LBA
			: reader_lba_len;
	}

	// Set the bank entry's LBA length.
	entry->lba_len = lba_len;

	// Initialize the disc image reader.
	entry->reader = reader_open(f_img, lba_start, reader_lba_len);

	if (type == RVTH_BankType_Empty) {
		// We're done here.
		return 0;
	}

	// Parse the timestamp.
	if (nhcd_timestamp) {
		entry->timestamp = rvth_timestamp_parse(nhcd_timestamp);
	}

	// TODO: Error handling.
	// Initialize the region code.
	rvth_init_BankEntry_region(entry);
	// Initialize the encryption status.
	rvth_init_BankEntry_crypto(entry);

	// We're done here.
	return 0;
}
