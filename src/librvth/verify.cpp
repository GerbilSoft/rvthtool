/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * verify.cpp: RVT-H verification functions.                               *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
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

#include "rvth.hpp"
#include "ptbl.h"
#include "rvth_error.h"

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
#include <memory>
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
 * @param bank		[in] Bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::verifyWiiPartitions(unsigned int bank,
	RvtH_Verify_Progress_Callback callback,
	void *userdata)
{
	int ret = 0;	// errno or RvtH_Errors

	// Make sure this is a Wii disc.
	RvtH_BankEntry *const entry = &m_entries[bank];
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
	}

	struct sha1_ctx sha1;
	uint8_t digest[SHA1_DIGEST_SIZE];
	unique_ptr<RVL_PartitionHeader> pt_hdr(new RVL_PartitionHeader);
	unique_ptr<Wii_Disc_H3_t> H3_tbl(new Wii_Disc_H3_t);
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
	for (unsigned int pt_idx = 0; pt_idx < entry->pt_count; pt_idx++)
	{
		const pt_entry_t *const pte = &entry->ptbl[pt_idx];

		// Initial group count will be calculated based on data size.
		// NOTE: LBA length is in 512-byte (2^9) blocks. Groups are 2 MB (2^21).
		// To convert from 512-byte blocks to 2 MB blocks, shift
		// right by 12.
		// NOTE 2: Last group might be incomplete. This won't be counted,
		// since it's not an actual group.
		// (FIXME: Maybe it needs to be counted?)
		unsigned int group_count = pte->lba_len >> 12;
		printf("\ninitial group count: %u\n", group_count);

		if (callback) {
			// Starting the next partition.
			state.pt_type = pte->type;
			state.pt_current = 0;

			// NOTE: Determining number of groups by searching for the first
			// all-zero hash in the H3 table, not by using the data size.
			// TODO: Compare the two?
			state.group_cur = 0;
			state.group_total = group_count;

			// Zero out the progress values.
			state.h4_errs = 0;
			state.h3_errs = 0;
			state.h2_errs = 0;
			state.h1_errs = 0;
			state.h0_errs = 0;

			callback(&state, userdata);
		}

		// Read the partition header.
		size_t lba_size = reader->read(pt_hdr.get(), pte->lba_start, BYTES_TO_LBA(sizeof(RVL_PartitionHeader)));
		if (lba_size != BYTES_TO_LBA(sizeof(RVL_PartitionHeader))) {
			// Read error.
			aesw_free(aesw);
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			return -err;
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

		// Find the first 00 hash. This will indicate the group count.
		const uint8_t *H3_entry = H3_tbl->h3[0];
		for (group_count = 0; group_count < ARRAY_SIZE(H3_tbl->h3);
		     group_count++, H3_entry += ARRAY_SIZE(H3_tbl->h3[0]))
		{
			if (H3_entry[0] != 0)
				continue;

			// Found an H3 entry that starts with 0.
			// Check the rest of the entry.
			bool isAllZero = true;
			for (unsigned int i = 1; i < ARRAY_SIZE(H3_tbl->h3[0]); i++) {
				if (H3_entry[i] != 0) {
					// Not all zero.
					isAllZero = false;
					break;
				}
			}

			if (isAllZero) {
				// Found an all-zero entry.
				printf("ALL-ZERO FOUND; total groups is %u\n", group_count);
				break;
			}
		}
		state.group_total = group_count;
		callback(&state, userdata);

		// Verify the H4 hash. (H3 table)
		sha1_init(&sha1);
		sha1_update(&sha1, sizeof(Wii_Disc_H3_t), reinterpret_cast<const uint8_t*>(H3_tbl.get()));
		sha1_digest(&sha1, sizeof(digest), digest);
		if (memcmp(pContentEntry->sha1_hash, digest, SHA1_DIGEST_SIZE) != 0) {
			printf("\nH4 has FAIL!\n");
			// TODO: Callback to print an error.
			state.h4_errs++;
		} else {
			printf("\nH4 hash is OK\n");
		}
		callback(&state, userdata);

		// Process the 2 MB blocks.
		// FIXME: Check for an incomplete final block.
#define LBAS_PER_GROUP BYTES_TO_LBA(GROUP_SIZE_ENC)
		H3_entry = H3_tbl->h3[0];
		uint32_t lba = pte->lba_start + h3_tbl_lba + BYTES_TO_LBA(sizeof(Wii_Disc_H3_t));
		for (unsigned int g = 0; g < group_count; g++, lba += LBAS_PER_GROUP) {
			// Update the status.
			if (callback) {
				state.group_cur = g;
				callback(&state, userdata);
			}

			if (unlikely(lba + LBAS_PER_GROUP > pte->lba_len)) {
				// Incomplete group. Attempting to read it will
				// result in an assertion. I'm not sure how this
				// would work on real hardware, but some SDK update
				// images have incomplete groups.
				const uint32_t lba_remain = pte->lba_len - lba;
				lba_size = reader->read(gdata.get(), lba, lba_remain);
				if (lba_size != lba_remain) {
					// Read error.
					aesw_free(aesw);
					errno = EIO;
					return -EIO;
				}

				// Zero out the rest of the buffer.
				memset((uint8_t*)gdata.get() + LBA_TO_BYTES(lba_remain), 0,
				       GROUP_SIZE_ENC - LBA_TO_BYTES(lba_remain));
			} else {
				// Read a full group;
				lba_size = reader->read(gdata.get(), lba, LBAS_PER_GROUP);
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
			for (unsigned int i = 0; i < 64; i++) {
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
			sha1_digest(&sha1, sizeof(digest), digest);
			if (memcmp(H3_entry, digest, SHA1_DIGEST_SIZE) != 0) {
				printf("\nGroup %u: H3 hash (of H2 table) FAIL!", g);
				// TODO: Callback to print an error.
				state.h3_errs++;
			} else {
				printf("\nGroup %u: H3 hash (of H2 table) is OK", g);
			}
			H3_entry += sizeof(digest);

			// Make sure sectors 1-63 have the same H2 table as sector 0.
			for (unsigned int sector = 1; sector < 64; sector++) {
				if (memcmp(gdata[0].hashes.H2,
					   gdata[sector].hashes.H2,
				           sizeof(gdata[0].hashes.H2)) != 0)
				{
					// TODO: Error counter?
					printf("\nGroup %u: Sector %u H2 table doesn't match sector 0!\n", g, sector);
				}
			}

			// Verify the H2 hashes. (hash of H1 tables in each subgroup of 8 sectors)
			for (unsigned int sg = 0; sg < 8; sg++) {
				const unsigned int sector = sg * 8;
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(gdata[sector].hashes.H1), gdata[sector].hashes.H1[0]);
				sha1_digest(&sha1, sizeof(digest), digest);
				if (memcmp(gdata[0].hashes.H2[sg], digest, sizeof(digest)) != 0) {
					printf("\nGroup %u, sub %u: H2 hash (of H1 table) FAIL!", g, sg);
					// TODO: Callback to print an error.
					state.h2_errs++;
				} else {
					printf("\nGroup %u, sub %u: H2 hash (of H1 table) is OK", g, sg);
				}
			}

			// Make sure sectors in each subgroup have the same H1 table as
			// sectors 0, 8, 16, 24, 32, 40, 48, 56.
			for (unsigned int sg = 0; sg < 8; sg++) {
				const unsigned int sector_start = sg * 8;
				const unsigned int sector_end = sector_start + 8;
				for (unsigned int sector = sector_start; sector < sector_end; sector++) {
					if (memcmp(gdata[sector_start].hashes.H1,
					           gdata[sector].hashes.H1,
					           sizeof(gdata[0].hashes.H1)) != 0)
					{
						// TODO: Error counter?
						printf("\nGroup %u, Subgroup %u: Sector %u H1 table doesn't match sector %u!\n", g, sg, sector, sector_start);
					}
				}
			}

			// Verify the H1 hashes. (hash of H0 tables in each block of 31 KB)
			for (unsigned int sector = 0; sector < 64; sector++) {
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(gdata[sector].hashes.H0), gdata[sector].hashes.H0[0]);
				sha1_digest(&sha1, sizeof(digest), digest);
				if (memcmp(gdata[sector].hashes.H1[sector % 8], digest, sizeof(digest)) != 0) {
					printf("\nGroup %u, sub %u, sector %u: H1 hash (of H0 table) FAIL!", g, sector / 8, sector);
					// TODO: Callback to print an error.
					state.h1_errs++;
				} else {
					printf("\nGroup %u, sub %u, sector %u: H1 hash (of H0 table) is OK", g, sector / 8, sector);
				}
			}

			// H0 tables are unique per block.
			// Verify the H0 hashes. (Now we're actually checking the data!)
			for (unsigned int sector = 0; sector < 64; sector++) {
				const uint8_t *pData = gdata[sector].data;
				for (unsigned int kb = 0; kb < 31; kb++, pData += 1024) {
					sha1_init(&sha1);
					sha1_update(&sha1, 1024, pData);
					sha1_digest(&sha1, sizeof(digest), digest);
					if (memcmp(gdata[sector].hashes.H0[kb], digest, sizeof(digest)) != 0) {
						printf("\nGroup %u, sub %u, sector %u: H0 hash (of kilobyte %u) FAIL!", g, sector / 8, sector, kb);
						// TODO: Callback to print an error.
						state.h0_errs++;
					} else {
						printf("\nGroup %u, sub %u, sector %u: H0 hash (of kilobyte %u) is OK", g, sector / 8, sector, kb);
					}
				}
			}
		}
	}

	// Finished verifying the disc.
	if (callback) {
		state.pt_current = entry->pt_count;
		callback(&state, userdata);
	}

	return ret;
}
