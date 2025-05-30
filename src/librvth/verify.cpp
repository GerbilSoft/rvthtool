/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * verify.cpp: RVT-H verification functions.                               *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "rvth_p.hpp"
#include "rvth_error.h"

#include "ptbl.h"

// For LBA_TO_BYTES()
#include "nhcd_structs.h"

// Reader class
#include "reader/Reader.hpp"

// libwiicrypto
#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/wii_structs.h"
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wii_sector.h"
#include "libwiicrypto/title_key.h"

// Encryption and hashing
#include "aesw.h"
#include <nettle/sha1.h>

#include "byteswap.h"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>

// C++ includes
#include <array>
#include <memory>
using std::array;
using std::unique_ptr;

// Sector buffer. (1 LBA)
typedef union _sbuf1_t {
	uint8_t u8[LBA_SIZE];
	GCN_DiscHeader gcn;
	struct {
		RVL_VolumeGroupTable vgtbl;
		RVL_PartitionTableEntry ptbl[15];
	};
} sbuf1_t;

// Sector buffer. (2 LBAs)
typedef union _sbuf2_t {
	uint8_t u8[LBA_SIZE*2];
	GCN_DiscHeader gcn;
	struct {
		RVL_VolumeGroupTable vgtbl;
		RVL_PartitionTableEntry ptbl[31];
	};
} sbuf2_t;

/**
 * Is a block of data all zero bytes?
 * @param pData Data block
 * @param size Size of data block, in bytes
 * @return True if the data is all zero; false if not.
 */
static bool is_block_zero(const uint8_t *pData, size_t size)
{
	// Vectorize to uintptr_t.
	// TODO: Handle unaligned architectures?
	const uintptr_t *pData32 = reinterpret_cast<const uintptr_t*>(pData);
	for (; size > sizeof(uintptr_t)-1; size -= sizeof(uintptr_t), pData32++) {
		if (*pData32 != 0) {
			// Not zero.
			return false;
		}
	}
	if (unlikely(size > 0)) {
		pData = reinterpret_cast<const uint8_t*>(pData32);
		for (; size > 0; size--, pData++) {
			if (*pData != 0) {
				// Not zero.
				return false;
			}
		}
	}

	// It's all zero.
	return true;
}

/**
 * Verify partitions in a Wii disc image.
 *
 * NOTE: This function only supports encrypted Wii disc images,
 * either retail or debug encryption.
 *
 * This will check all five levels of hashes:
 * - H4: Hash of H3 table (stored in the TMD)
 * - H3: Hash of all H2 tables (H2 table = 2 MB group)
 * - H2: Hash of all H1 tables (H1 table = 256 KB subgroup)
 * - H1: Hash of all H0 tables (H0 table = 32 KB block)
 * - H0: Hash of a single KB   (H0 = 1 KB)
 *
 * NOTE: Assuming the TMD signature is valid, which means
 * the H4 hash is correct.
 *
 * @param bank		[in] Bank number (0-7)
 * @param errorCount	[out] Error counts for all 5 hash tables
 * @param callback	[in,opt] Progress callback
 * @param userdata	[in,opt] User data for progress callback
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::verifyWiiPartitions(unsigned int bank,
	WiiErrorCount_t *errorCount,
	RvtH_Verify_Progress_Callback callback,
	void *userdata)
{
	int ret = 0;	// errno or RvtH_Errors
	if (errorCount) {
		for (size_t i = 0; i < ARRAY_SIZE(errorCount->errs); i++) {
			errorCount->errs[i] = 0;
		}
	}

	// Make sure this is a Wii disc.
	RvtH_BankEntry *const entry = &d_ptr->entries[bank];
	switch (entry->type) {
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Verification is possible.
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_GCN:
			// Operation is not supported for GCN images.
			return RVTH_ERROR_NOT_WII_IMAGE;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	// Make sure it's encrypted.
	if (entry->crypto_type <= RVL_CryptoType_None ||
	    entry->crypto_type >= RVL_CryptoType_MAX)
	{
		// Not encrypted.
		return RVTH_ERROR_IS_UNENCRYPTED;
	}

	// Make sure the partition table is loaded.
	ret = rvth_ptbl_load(entry);
	if (ret != 0 || entry->pt_count == 0 || !entry->ptbl) {
		// Unable to load the partition table.
		errno = -ret;
		return ret;
	}

	// Callback state.
	RvtH_Verify_Progress_State state;

	if (callback) {
		// Initialize the callback state.
		state.rvth = this;
		state.bank = bank;

		state.pt_type = 0;	// will be updated later
		state.pt_current = 0;
		state.pt_total = entry->pt_count;

		state.type = RVTH_VERIFY_STATUS;
		state.hash_level = 0;
		state.sector = 0;
		state.kb = 0;
		state.err_type = RVTH_VERIFY_ERROR_UNKNOWN;
		state.is_zero = false;
	}

	struct sha1_ctx sha1;
	array<uint8_t, SHA1_DIGEST_SIZE> digest;
	unique_ptr<RVL_PartitionHeader> pt_hdr(new RVL_PartitionHeader);
	unique_ptr<Wii_Disc_H3_t> H3_tbl(new Wii_Disc_H3_t);
	// NOTE: Retaining the encrypted version in order to do zero checks.
	unique_ptr<Wii_Disc_Sector_t[]> gdata_enc(new Wii_Disc_Sector_t[64]);	// 2 MB, one group
	unique_ptr<Wii_Disc_Sector_t[]> gdata(new Wii_Disc_Sector_t[64]);	// 2 MB, one group

	// Initialize the AES context.
	errno = 0;
	AesCtx *const aesw = aesw_new();
	if (!aesw) {
		int ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	}

	// Zero IV for decrypting hashes.
	uint8_t zero_iv[16];
	memset(zero_iv, 0, sizeof(zero_iv));

	// Verify partitions.
	Reader *const reader = entry->reader;
	for (unsigned int pt_idx = 0; pt_idx < entry->pt_count; pt_idx++) {
		const pt_entry_t *const pte = &entry->ptbl[pt_idx];

		// Initial group count will be calculated based on data size.
		// NOTE: LBA length is in 512-byte (2^9) blocks. Groups are 2 MB (2^21).
		// To convert from 512-byte blocks to 2 MB blocks, shift
		// right by 12.
		// NOTE 2: Last group is usually incomplete, so we shouldn't check past
		// the last sector.
		unsigned int group_count = pte->lba_len >> 12;
		unsigned int last_group_sectors = 0;
		if (pte->lba_len & 0xFFF) {
			group_count++;
			last_group_sectors = (pte->lba_len & 0xFFF) / 64;
		}

		if (callback) {
			// Starting the next partition.
			state.pt_type = pte->type;
			state.pt_current = pt_idx;

			// NOTE: Determining number of groups by searching for the first
			// all-zero hash in the H3 table, not by using the data size.
			// TODO: Compare the two?
			state.group_cur = 0;
			state.group_total = group_count;

			state.type = RVTH_VERIFY_STATUS;
			callback(&state, userdata);
		}

		// Read the partition header.
		size_t lba_size = reader->read(pt_hdr.get(), pte->lba_start, BYTES_TO_LBA(sizeof(RVL_PartitionHeader)));
		if (lba_size != BYTES_TO_LBA(sizeof(RVL_PartitionHeader))) {
			// Read error.
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			aesw_free(aesw);
			return -err;
		}

		// Use the data length in the partition header to determine
		// the total number of groups and the last group sector count.
		// This is usually accurate except for unencrypted partitions,
		// in which case, this function won't work anyway!
		if (pt_hdr->data_size != 0) {
			const uint64_t data_size = static_cast<uint64_t>(be32_to_cpu(pt_hdr->data_size)) << 2;
			if (data_size > 9ULL*1024*1024*1024) {
				// Cannot be more than 9 GiB!
				// H3 table is limited to 9,830.4 MiB,
				// but dual-layer discs are limited to ~8 GiB.
				aesw_free(aesw);
				errno = EIO;
				return -EIO;
			}
			group_count = static_cast<uint32_t>(data_size / GROUP_SIZE_ENC);
			if (data_size % GROUP_SIZE_ENC != 0) {
				group_count++;
				last_group_sectors = static_cast<uint32_t>((data_size % GROUP_SIZE_ENC) / 32768);
			}
			if (callback) {
				state.group_total = group_count;
				state.type = RVTH_VERIFY_STATUS;
				callback(&state, userdata);
			}
		}

		// TMD must be located within the partition header.
		const unsigned int tmd_offset = be32_to_cpu(pt_hdr->tmd_offset) << 2;
		const unsigned int tmd_size = be32_to_cpu(pt_hdr->tmd_size);
		if (tmd_offset == 0 || tmd_offset > sizeof(RVL_PartitionHeader) ||
		    tmd_size < (sizeof(RVL_TMD_Header) + sizeof(RVL_Content_Entry)))
		{
			// TMD offset and/or size is invalid.
			// TODO: More specific error?
			aesw_free(aesw);
			errno = EIO;
			return -EIO;
		}
		const RVL_TMD_Header *const pTmd = reinterpret_cast<const RVL_TMD_Header*>(
			reinterpret_cast<const uint8_t*>(pt_hdr.get()) + tmd_offset);
		if (pTmd->nbr_cont != cpu_to_be16(1)) {
			// Disc partitions should only have one content in the TMD!
			// TODO: More specific error?
			aesw_free(aesw);
			errno = EIO;
			return -EIO;
		}
		const RVL_Content_Entry *const pContentEntry = reinterpret_cast<const RVL_Content_Entry*>(
			reinterpret_cast<const uint8_t*>(pt_hdr.get()) + tmd_offset + sizeof(RVL_TMD_Header));

		// Decrypt the title key.
		uint8_t title_key[16];
		uint8_t crypto_type;	// not used yet?
		ret = decrypt_title_key(&pt_hdr->ticket, title_key, &crypto_type);
		if (ret != 0) {
			// Error decrypting title key.
			// TODO: Indicate the error.
			return ret;
		}
		aesw_set_key(aesw, title_key, sizeof(title_key));

		// Get the H3 table offset. (usually 0x8000)
		// NOTE: It's shifted right by 2, so un-shift, then convert to LBA.
		// May cause overflow if it's too high, but it's usually 0x8000.
		const uint32_t h3_tbl_lba = BYTES_TO_LBA(be32_to_cpu(pt_hdr->h3_table_offset) << 2);
		if (h3_tbl_lba == 0) {
			// Invalid H3 table LBA.
			// TODO: Return a better error code.
			aesw_free(aesw);
			errno = EIO;
			return -EIO;
		}

		// Read the H3 table.
		lba_size = reader->read(H3_tbl.get(), pte->lba_start + h3_tbl_lba, BYTES_TO_LBA(sizeof(Wii_Disc_H3_t)));
		if (lba_size != BYTES_TO_LBA(sizeof(Wii_Disc_H3_t))) {
			// Read error.
			aesw_free(aesw);
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			return -err;
		}

		// Unlikely: Partition header's data size is 0.
		// If so, fall back to checking the H3 table.
		if (unlikely(pt_hdr->data_size != 0)) {
			// Find the first 00 hash. This will indicate the group count.
			// TODO: Partial final group?
			const uint8_t *H3_entry = H3_tbl->h3[0];
			for (group_count = 0; group_count < ARRAY_SIZE(H3_tbl->h3);
				group_count++, H3_entry += ARRAY_SIZE(H3_tbl->h3[0]))
			{
				if (H3_entry[0] != 0)
					continue;

				// Found an H3 entry that starts with 0.
				// Check the rest of the entry.
				if (is_block_zero(H3_entry, sizeof(H3_tbl->h3[0]))) {
					// Found an all-zero entry.
					break;
				}
			}
			if (callback) {
				state.group_total = group_count;
				callback(&state, userdata);
			}
		}

		// Verify the H4 hash. (H3 table)
		sha1_init(&sha1);
		sha1_update(&sha1, sizeof(Wii_Disc_H3_t), reinterpret_cast<const uint8_t*>(H3_tbl.get()));
		sha1_digest(&sha1, digest.size(), digest.data());
		if (memcmp(pContentEntry->sha1_hash, digest.data(), SHA1_DIGEST_SIZE) != 0) {
			state.is_zero = is_block_zero((const uint8_t*)H3_tbl.get(), 512);	// only check one LBA
			if (errorCount) {
				errorCount->h4++;
			}
			if (callback) {
				state.type = RVTH_VERIFY_ERROR_REPORT;
				state.hash_level = 4;
				state.sector = 0;	// irrelevant for H4
				state.err_type = RVTH_VERIFY_ERROR_BAD_HASH;
				callback(&state, userdata);
			}
		}

		// Process the 2 MB blocks.
		// FIXME: Check for an incomplete final block.
#define LBAS_PER_GROUP BYTES_TO_LBA(GROUP_SIZE_ENC)
		const uint8_t *H3_entry = H3_tbl->h3[0];
		uint32_t lba = pte->lba_start + BYTES_TO_LBA(static_cast<uint64_t>(be32_to_cpu(pt_hdr->data_offset)) << 2);
		for (unsigned int g = 0; g < group_count; g++, lba += LBAS_PER_GROUP) {
			const bool is_last_group = (g == (group_count - 1));

			unsigned int max_sector = 64;
			if (last_group_sectors != 0 && is_last_group) {
				max_sector = last_group_sectors;
			}

			// Update the status.
			if (callback) {
				state.group_cur = g;
				state.type = RVTH_VERIFY_STATUS;
				callback(&state, userdata);
			}

			if (unlikely(lba + LBAS_PER_GROUP > pte->lba_start + pte->lba_len)) {
				// Incomplete group. Attempting to read it will
				// result in an assertion. I'm not sure how this
				// would work on real hardware, but some SDK update
				// images have incomplete groups.
				if (!is_last_group) {
					// Should not happen if this isn't the last group!
					assert(!"Group is truncated, but it isn't the last group.");
					aesw_free(aesw);
					errno = EIO;
					return -EIO;
				}
				const uint32_t lba_remain = pte->lba_len - lba;
				lba_size = reader->read(gdata_enc.get(), lba, lba_remain);
				if (lba_size != lba_remain) {
					// Read error.
					aesw_free(aesw);
					errno = EIO;
					return -EIO;
				}

				const unsigned int tmp_max_sector = lba_remain / 64;
				if (tmp_max_sector < max_sector) {
					max_sector = tmp_max_sector;
				}
			} else {
				// Read a full group;
				lba_size = reader->read(gdata_enc.get(), lba, LBAS_PER_GROUP);
				if (lba_size != LBAS_PER_GROUP) {
					// Read error.
					aesw_free(aesw);
					errno = EIO;
					return -EIO;
				}
			}

			// Decrypt the blocks.
			// User data IV is stored within the encrypted H2 table,
			// so decrypt the user data first, *then* the hashes.
			memcpy(gdata.get(), gdata_enc.get(), GROUP_SIZE_ENC);
			for (unsigned int i = 0; i < max_sector; i++) {
				// Decrypt user data.
				aesw_set_iv(aesw, &gdata[i].hashes.H2[7][4], 16);
				aesw_decrypt(aesw, gdata[i].data, sizeof(gdata[i].data));

				// Decrypt hashes. (IV == 0)
				aesw_set_iv(aesw, zero_iv, sizeof(zero_iv));
				aesw_decrypt(aesw, (uint8_t*)&gdata[i].hashes, sizeof(gdata[i].hashes));
			}

			// Verify the H3 hash. (hash of H2 table in sector 0)
			sha1_init(&sha1);
			sha1_update(&sha1, sizeof(gdata[0].hashes.H2), gdata[0].hashes.H2[0]);
			sha1_digest(&sha1, digest.size(), digest.data());
			if (memcmp(H3_entry, digest.data(), digest.size()) != 0) {
				state.is_zero = is_block_zero((const uint8_t*)&gdata_enc[0], sizeof(gdata_enc[0]));
				if (errorCount) {
					errorCount->h3++;
				}
				if (callback) {
					state.type = RVTH_VERIFY_ERROR_REPORT;
					state.hash_level = 3;
					state.sector = 0;
					state.err_type = RVTH_VERIFY_ERROR_BAD_HASH;
					callback(&state, userdata);
				}
			}
			H3_entry += digest.size();

			// Make sure sectors 1-63 have the same H2 table as sector 0.
			for (unsigned int sector = 1; sector < max_sector; sector++) {
				if (memcmp(gdata[0].hashes.H2,
					   gdata[sector].hashes.H2,
				           sizeof(gdata[0].hashes.H2)) != 0)
				{
					state.is_zero = is_block_zero((const uint8_t*)&gdata_enc[sector], sizeof(gdata_enc[sector]));
					if (errorCount) {
						errorCount->h2++;
					}
					if (callback) {
						state.type = RVTH_VERIFY_ERROR_REPORT;
						state.hash_level = 2;
						state.sector = sector;
						state.err_type = RVTH_VERIFY_ERROR_TABLE_COPY;
						callback(&state, userdata);
					}
				}
			}

			// Verify the H2 hashes. (hash of H1 tables in each subgroup of 8 sectors)
			for (unsigned int sector = 0; sector < max_sector; sector += 8) {
				const unsigned int sg = sector / 8;
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(gdata[sector].hashes.H1), gdata[sector].hashes.H1[0]);
				sha1_digest(&sha1, digest.size(), digest.data());
				if (memcmp(gdata[0].hashes.H2[sg], digest.data(), digest.size()) != 0) {
					state.is_zero = is_block_zero((const uint8_t*)&gdata_enc[sector], sizeof(gdata_enc[sector]));
					if (errorCount) {
						errorCount->h2++;
					}
					if (callback) {
						state.type = RVTH_VERIFY_ERROR_REPORT;
						state.hash_level = 2;
						state.sector = sector;
						state.err_type = RVTH_VERIFY_ERROR_BAD_HASH;
						callback(&state, userdata);
					}
				}
			}

			// Make sure sectors in each subgroup have the same H1 table as
			// sectors 0, 8, 16, 24, 32, 40, 48, 56.
			for (unsigned int sector_start = 0; sector_start < max_sector; sector_start += 8) {
				unsigned int sector_end = sector_start + 8;
				if (sector_end > max_sector) {
					sector_end = max_sector;
				}
				for (unsigned int sector = sector_start; sector < sector_end; sector++) {
					if (memcmp(gdata[sector_start].hashes.H1,
					           gdata[sector].hashes.H1,
					           sizeof(gdata[0].hashes.H1)) != 0)
					{
						state.is_zero = is_block_zero((const uint8_t*)&gdata_enc[sector], sizeof(gdata_enc[sector]));
						if (errorCount) {
							errorCount->h1++;
						}
						if (callback) {
							state.type = RVTH_VERIFY_ERROR_REPORT;
							state.hash_level = 1;
							state.sector = sector;
							state.err_type = RVTH_VERIFY_ERROR_TABLE_COPY;
							callback(&state, userdata);
						}
					}
				}
			}

			// Verify the H1 hashes. (hash of H0 tables in each block of 31 KB)
			for (unsigned int sector = 0; sector < max_sector; sector++) {
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(gdata[sector].hashes.H0), gdata[sector].hashes.H0[0]);
				sha1_digest(&sha1, digest.size(), digest.data());
				if (memcmp(gdata[sector].hashes.H1[sector % 8], digest.data(), digest.size()) != 0) {
					state.is_zero = is_block_zero((const uint8_t*)&gdata_enc[sector], sizeof(gdata_enc[sector]));
					if (errorCount) {
						errorCount->h1++;
					}
					if (callback) {
						state.type = RVTH_VERIFY_ERROR_REPORT;
						state.hash_level = 1;
						state.sector = sector;
						state.err_type = RVTH_VERIFY_ERROR_BAD_HASH;
						callback(&state, userdata);
					}
				}
			}

			// H0 tables are unique per block.
			// Verify the H0 hashes. (Now we're actually checking the data!)
			for (unsigned int sector = 0; sector < max_sector; sector++) {
				const uint8_t *pData = gdata[sector].data;
				for (unsigned int kb = 0; kb < 31; kb++, pData += 1024) {
					sha1_init(&sha1);
					sha1_update(&sha1, 1024, pData);
					sha1_digest(&sha1, digest.size(), digest.data());
					if (memcmp(gdata[sector].hashes.H0[kb], digest.data(), digest.size()) != 0) {
						state.is_zero = is_block_zero(&gdata_enc[sector].data[kb * 1024], 1024);
						if (errorCount) {
							errorCount->h0++;
						}
						if (callback) {
							state.type = RVTH_VERIFY_ERROR_REPORT;
							state.hash_level = 0;
							state.sector = sector;
							state.kb = kb+1;
							state.err_type = RVTH_VERIFY_ERROR_BAD_HASH;
							callback(&state, userdata);
						}
					}
				}
			}
		}

		// Update the status.
		if (callback) {
			state.group_cur = group_count;
			state.type = RVTH_VERIFY_STATUS;
			callback(&state, userdata);
		}
	}

	// Finished verifying the disc.
	if (callback) {
		state.pt_current = entry->pt_count;
		callback(&state, userdata);
	}

	return ret;
}
