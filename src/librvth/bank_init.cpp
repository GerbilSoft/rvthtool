/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * bank_init.cpp: RvtH_BankEntry initialization functions.                 *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"

#include "disc_header.h"
#include "nhcd_structs.h"
#include "ptbl.h"
#include "rvth_time.h"
#include "reader/Reader.hpp"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

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
	lba_size = entry->reader->read(sector_buf.u8, lba_region, 1);
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
	lba_size = entry->reader->read(&header, game_pte->lba_start, BYTES_TO_LBA(sizeof(header)));
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
 * Check the address limit for a DOL header.
 * @param dol DOL header.
 * @param limit Address limit.
 * @return True if the DOL does not exceed the address limit; false if it does.
 */
static bool dol_check_address_limit(const DOL_Header *dol, uint32_t limit)
{
	unsigned int i;
	uint32_t end;

	for (i = 0; i < ARRAY_SIZE(dol->text); i++) {
		if (dol->text[i] != cpu_to_be32(0)) {
			end = be32_to_cpu(dol->text[i]) + be32_to_cpu(dol->textLen[i]);
			if ((end < 0x81100000 || end > 0x81130000) && end > limit) {
				// Out of range.
				return false;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(dol->data); i++) {
		if (dol->data[i] != cpu_to_be32(0)) {
			end = be32_to_cpu(dol->data[i]) + be32_to_cpu(dol->dataLen[i]);
			if ((end < 0x81100000 || end > 0x81130000) && end > limit) {
				// Out of range.
				return false;
			}
		}
	}

	end = be32_to_cpu(dol->bss) + be32_to_cpu(dol->bssLen);
	if ((end < 0x81100000 || end > 0x81130000) && end > limit) {
		// Out of range.
		return false;
	}

	// In range.
	return true;
}

/**
 * Set the aplerr field in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_AppLoader(RvtH_BankEntry *entry)
{
	uint32_t lba_size;
	uint32_t lba_start = 0;
	uint8_t shift = 0;
	bool is_wii = false;
	bool fst_after_dol = false;
	unsigned int i;

	// Sector buffer.
	uint8_t sector_buf[RVTH_BLOCK_SIZE*2];

	// Physical memory size is 24 MB.
	static const uint32_t physMemSize = 24*1024*1024;

	// BI2 fields.
	uint32_t debugMonSize;
	uint32_t simMemSize;

#pragma pack(1)
	// Boot block and boot info. (starts at 0x420)
	struct PACKED {
		GCN_Boot_Block bb2;
		GCN_Boot_Info bi2;
	} boot;
#pragma pack()

	// main.dol
	int64_t dolOffset;
	DOL_Header dol;

	assert(entry->reader != NULL);

	// Reference: https://www.gc-forever.com/wiki/index.php?title=Apploader
	entry->aplerr = APLERR_UNKNOWN;
	memset(entry->aplerr_val, 0, sizeof(entry->aplerr_val));
	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty bank.
			return RVTH_ERROR_BANK_EMPTY;
		case RVTH_BankType_Unknown:
		default:
			// Unknown bank.
			return RVTH_ERROR_BANK_UNKNOWN;
		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;

		case RVTH_BankType_GCN:
			// Shift value is 0.
			lba_start = 0;
			shift = 0;
			is_wii = false;
			break;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL: {
			// Find the game partition.
			// TODO: Error checking.
			const pt_entry_t *game_pte = rvth_ptbl_find_game(entry);
			if (!game_pte) {
				// No game partition...
				return RVTH_ERROR_NO_GAME_PARTITION;
			}
			// NOTE: This is the partition header.
			// Need to read the header to find the data offset.
			lba_start = game_pte->lba_start;
			shift = 2;
			is_wii = true;
			break;
		}
	}

	// TODO: Need to manually decrypt the Wii sectors.
	if (entry->crypto_type != RVTH_CryptoType_None) {
		return RVTH_ERROR_IS_ENCRYPTED;
	}

	if (is_wii) {
		// Read the partition header to determine the data offset.
		// 0x2B8: Data offset >> 2 (LBA 1)
		uint64_t data_offset;
		lba_size = entry->reader->read(sector_buf, lba_start + 1, 1);
		if (lba_size != 1) {
			// Error reading the boot block and boot info.
			return -EIO;
		}

		// TODO: Optimize this?
		data_offset = (sector_buf[0x0B8] << 24) |
			      (sector_buf[0x0B9] << 16) |
			      (sector_buf[0x0BA] <<  8) |
			       sector_buf[0x0BB];
		data_offset <<= shift;
		lba_start += BYTES_TO_LBA(data_offset);
	}

	// Read the boot block and boot info.
	// Start address: 0x420 (LBA 2)
	lba_size = entry->reader->read(sector_buf, lba_start + 2, 1);
	if (lba_size != 1) {
		// Error reading the boot block and boot info.
		return -EIO;
	}
	memcpy(&boot, &sector_buf[0x020], sizeof(boot));

	// BI2 fields.
	debugMonSize = be32_to_cpu(boot.bi2.debugMonSize);
	simMemSize = be32_to_cpu(boot.bi2.simMemSize);

	// Is the FST before or after main.dol?
	fst_after_dol = (be32_to_cpu(boot.bb2.bootFilePosition) < be32_to_cpu(boot.bb2.FSTPosition));

	if (!fst_after_dol) {
		// FST is before main.dol.
		if (be32_to_cpu(boot.bb2.FSTLength) > be32_to_cpu(boot.bb2.FSTMaxLength)) {
			// FSTLength > FSTMaxLength
			entry->aplerr = APLERR_FSTLENGTH;
			entry->aplerr_val[0] = be32_to_cpu(boot.bb2.FSTLength) << shift;
			entry->aplerr_val[1] = be32_to_cpu(boot.bb2.FSTMaxLength) << shift;
			return 0;
		}
	}

	// Validate the rest of the boot info.
	if (debugMonSize % 32 != 0) {
		// Debug Monitor Size is not a multiple of 32.
		entry->aplerr = APLERR_DEBUGMONSIZE_UNALIGNED;
		entry->aplerr_val[0] = debugMonSize;
		return 0;
	} else if (simMemSize % 32 != 0) {
		// Simulated Memory Size is not a multiple of 32.
		entry->aplerr = APLERR_SIMMEMSIZE_UNALIGNED;
		entry->aplerr_val[0] = simMemSize;
		return 0;
	} else if ((simMemSize < physMemSize) && (debugMonSize >= (physMemSize - simMemSize))) {
		// (PhysMemSize - SimMemSize) must be > DebugMonSize
		entry->aplerr = APLERR_PHYSMEMSIZE_MINUS_SIMMEMSIZE_NOT_GT_DEBUGMONSIZE;
		entry->aplerr_val[0] = physMemSize;
		entry->aplerr_val[1] = simMemSize;
		entry->aplerr_val[2] = debugMonSize;
		return 0;
	} else if (simMemSize > physMemSize) {
		// Simulated Memory Size must be <= Physical Memory Size
		entry->aplerr = APLERR_SIMMEMSIZE_NOT_LE_PHYSMEMSIZE;
		entry->aplerr_val[0] = physMemSize;
		entry->aplerr_val[1] = simMemSize;
		return 0;
	} else if (be32_to_cpu(boot.bb2.FSTAddress) > 0x81700000) {
		// Illegal FST address. (must be < 0x81700000)
		entry->aplerr = APLERR_ILLEGAL_FST_ADDRESS;
		entry->aplerr_val[0] = be32_to_cpu(boot.bb2.FSTAddress);
		return 0;
	}

	// Load the DOL header.
	dolOffset = (int64_t)be32_to_cpu(boot.bb2.bootFilePosition) << shift;
	lba_size = entry->reader->read(sector_buf, lba_start + BYTES_TO_LBA(dolOffset), 2);
	if (lba_size != 2) {
		// Error reading the DOL header.
		return -EIO;
	}

	memcpy(&dol, &sector_buf[dolOffset % RVTH_BLOCK_SIZE], sizeof(dol));

	if (boot.bi2.dolLimit != cpu_to_be32(0)) {
		// Calculate the total size of all sections.
		// FIXME: ALIGN() macros aren't working...
		const uint32_t dolLimit = be32_to_cpu(boot.bi2.dolLimit);
		uint32_t dolSize = 0;
		for (i = 0; i < ARRAY_SIZE(dol.text); i++) {
			if (dol.textData[i] != cpu_to_be32(0)) {
				dolSize += be32_to_cpu(dol.textLen[i]);
				dolSize = ((dolSize + 31U) & ~31U);
			}
		}
		for (i = 0; i < ARRAY_SIZE(dol.data); i++) {
			if (dol.dataData[i] != cpu_to_be32(0)) {
				dolSize += be32_to_cpu(dol.dataLen[i]);
				dolSize = ((dolSize + 31U) & ~31U);
			}
		}
		if (dolSize > dolLimit) {
			// DOL exceeds size limit.
			entry->aplerr = APLERR_DOL_EXCEEDS_SIZE_LIMIT;
			entry->aplerr_val[0] = dolSize;
			entry->aplerr_val[1] = dolLimit;
			return 0;
		}
	}

	// Check the address limit.
	if (is_wii) {
		if (!dol_check_address_limit(&dol, 0x80900000)) {
			// Retail Wii address limit exceeded.
			// TODO: Does this vary by apploader?
			// "The Last Story" uses 0x80900000.
			entry->aplerr = APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED;
			entry->aplerr_val[0] = 0x80900000;
			return 0;
		} else if (!dol_check_address_limit(&dol, 0x81200000)) {
			// Debug Wii address limit exceeded.
			// TODO: Verify this. (using the gc-forever value)
			entry->aplerr = APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED;
			entry->aplerr_val[0] = 0x81200000;
			return 0;
		}
	} else {
		if (!dol_check_address_limit(&dol, 0x80700000)) {
			// Retail Wii address limit exceeded.
			entry->aplerr = APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED;
			entry->aplerr_val[0] = 0x80700000;
			return 0;
		} else if (!dol_check_address_limit(&dol, 0x81200000)) {
			// Debug Wii address limit exceeded.
			entry->aplerr = APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED;
			entry->aplerr_val[0] = 0x81200000;
			return 0;
		}
	}

	// "Load" text segments.
	for (i = 0; i < ARRAY_SIZE(dol.textData); i++) {
		if (dol.textData[i] != cpu_to_be32(0)) {
			const uint32_t addr = be32_to_cpu(dol.text[i]);
			const uint32_t end = addr + be32_to_cpu(dol.textLen[i]);
			if (end > 0x81200000) {
				// Text segment is too big.
				entry->aplerr = APLERR_DOL_TEXTSEG2BIG;
				entry->aplerr_val[0] = addr;
				entry->aplerr_val[1] = end;
				return 0;
			}
		}
	}

	// "Load" data segments.
	for (i = 0; i < ARRAY_SIZE(dol.dataData); i++) {
		if (dol.dataData[i] != cpu_to_be32(0)) {
			const uint32_t addr = be32_to_cpu(dol.data[i]);
			const uint32_t end = addr + be32_to_cpu(dol.dataLen[i]);
			if (end > 0x81200000) {
				// Data segment is too big.
				entry->aplerr = APLERR_DOL_DATASEG2BIG;
				entry->aplerr_val[0] = addr;
				entry->aplerr_val[1] = end;
				return 0;
			}
		}
	}

	if (fst_after_dol) {
		// FST is after main.dol.
		if (be32_to_cpu(boot.bb2.FSTLength) > be32_to_cpu(boot.bb2.FSTMaxLength)) {
			// FSTLength > FSTMaxLength
			entry->aplerr = APLERR_FSTLENGTH;
			entry->aplerr_val[0] = be32_to_cpu(boot.bb2.FSTLength) << shift;
			entry->aplerr_val[1] = be32_to_cpu(boot.bb2.FSTMaxLength) << shift;
			return 0;
		}
	}

	// We're done here.
	entry->aplerr = APLERR_OK;
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
	// TODO: Error handling.
	entry->reader = Reader::open(f_img, lba_start, reader_lba_len);

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
	// Initialize the AppLoader error status.
	rvth_init_BankEntry_AppLoader(entry);

	// We're done here.
	return 0;
}
