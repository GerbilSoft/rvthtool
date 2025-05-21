/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * extract_crypt.cpp: Extract and encrypt an unencrypted image.            *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth.hpp"
#include "disc_header.hpp"
#include "ptbl.h"
#include "rvth_error.h"

#include "byteswap.h"
#include "nhcd_structs.h"

// Reader class
#include "reader/Reader.hpp"

// libwiicrypto
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wii_sector.h"
#include "libwiicrypto/title_key.h"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

// Encryption
#include "aesw.h"
#include <nettle/sha1.h>

/**
 * Encrypt a group of Wii sectors.
 * @param aesw AES context. (Key must be set to the decrypted title key.)
 * @param pInBuf	[in] Input buffer.
 * @param inSize	[in] Size of in_buf. (Must have 3,968 LBAs, or 2,031,616 bytes.)
 * @param pOutBuf	[out] Output buffer.
 * @param outSize	[in] Size of out_buf. (Must have 4,096 LBAs, or 2,097,152 bytes.)
 * @param pH3		[in] Output buffer for the H3 hash.
 * @param H3_size;	[in] Size of pH3. (Must be SHA1_DIGEST_SIZE bytes.)
 * @return 0 on success; negative POSIX error code on error.
 */
static int rvth_encrypt_group(AesCtx *aesw, const uint8_t *pInBuf,
	size_t inSize, uint8_t *pOutBuf, size_t outSize,
	uint8_t *pH3, size_t H3_size)
{
	struct sha1_ctx sha1;
	unsigned int i, j;
	uint8_t iv[16];

	// Disc sector pointers.
	Wii_Disc_Sector_t *const sbuf = (Wii_Disc_Sector_t*)pOutBuf;
	Wii_Disc_Sector_t *sbuf_tmp;

	assert(aesw);
	assert(pInBuf);
	assert(inSize == GROUP_SIZE_DEC);
	assert(pOutBuf);
	assert(outSize == GROUP_SIZE_ENC);
	assert(pH3);
	assert(H3_size == SHA1_DIGEST_SIZE);

	if (!aesw || !pInBuf || inSize != GROUP_SIZE_DEC ||
	    !pOutBuf || outSize != GROUP_SIZE_ENC ||
	    !pH3 || H3_size != SHA1_DIGEST_SIZE)
	{
		// Invalid parameters.
		errno = EINVAL;
		return -EINVAL;
	}

	// Initialize the SHA-1 context.
	sha1_init(&sha1);

	// Copy the user data and calculate the H0 hashes.
	for (i = 0; i < 64; i++, pInBuf += SECTOR_SIZE_DEC) {
		// Copy user data.
		uint8_t *pH0 = sbuf[i].hashes.H0[0];
		uint8_t *pData = sbuf[i].data;
		memcpy(pData, pInBuf, SECTOR_SIZE_DEC);

		// Calculate the H0 hashes.
		for (j = 0; j < 31; j++, pData += 1024, pH0 += SHA1_DIGEST_SIZE) {
			sha1_update(&sha1, 1024, pData);
			sha1_digest(&sha1, SHA1_DIGEST_SIZE, pH0);
		}

		// Zero out the post-H0 padding.
		memset(sbuf[i].hashes.pad_H0, 0, sizeof(sbuf[i].hashes.pad_H0));
	}

	// Calculate the H1 hashes for each subgroup of 8 sectors.
	for (i = 0; i < 64; i += 8) {
		// First sector in the subgroup.
		Wii_Disc_Sector_t *const sbuf0 = &sbuf[i];
		uint8_t *pH1 = sbuf0->hashes.H1[0];

		// Hash the H0 tables and store the results
		// in the first sector's H1 table.
		for (j = i; j < i+8; j++, pH1 += SHA1_DIGEST_SIZE) {
			sha1_update(&sha1, sizeof(sbuf[j].hashes.H0), sbuf[j].hashes.H0[0]);
			sha1_digest(&sha1, SHA1_DIGEST_SIZE, pH1);
		}
		memset(sbuf0->hashes.pad_H1, 0, sizeof(sbuf0->hashes.pad_H1));

		// Copy the H1 hashes to each sector in the subgroup.
		for (j = i+1; j < i+8; j++) {
			memcpy(sbuf[j].hashes.H1, sbuf0->hashes.H1, sizeof(sbuf[j].hashes.H1));
			memset(sbuf[j].hashes.pad_H1, 0, sizeof(sbuf[j].hashes.pad_H1));
		}
	}

	// Calculate the H2 hashes for the subgroups.
	// NOTE: All sectors in this group have the same H2 hashes.
	sbuf_tmp = sbuf;
	for (i = 0; i < 8; i += 1, sbuf_tmp += 8) {
		sha1_update(&sha1, sizeof(sbuf_tmp->hashes.H1), sbuf_tmp->hashes.H1[0]);
		sha1_digest(&sha1, SHA1_DIGEST_SIZE, sbuf[0].hashes.H2[i]);
	}
	memset(sbuf[0].hashes.pad_H2, 0, sizeof(sbuf[0].hashes.pad_H2));

	// Copy the H2 hashes to all sectors and encrypt the hashes.
	sbuf_tmp = &sbuf[1];
	memset(iv, 0, sizeof(iv));
	for (i = 1; i < 64; i++, sbuf_tmp++) {
		memcpy(sbuf_tmp->hashes.H2, sbuf[0].hashes.H2, sizeof(sbuf[0].hashes.H2));
		memset(sbuf_tmp->hashes.pad_H2, 0, sizeof(sbuf_tmp->hashes.pad_H2));

		// TODO: Error checking.
		aesw_set_iv(aesw, iv, sizeof(iv));
		aesw_encrypt(aesw, (uint8_t*)&sbuf_tmp->hashes, sizeof(sbuf_tmp->hashes));
	}

	// Calculate the H3 hash.
	sha1_update(&sha1, sizeof(sbuf[0].hashes.H2), sbuf[0].hashes.H2[0]);
	sha1_digest(&sha1, SHA1_DIGEST_SIZE, pH3);

	// Encrypt sector 0's hashes.
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_encrypt(aesw, (uint8_t*)&sbuf[0].hashes, sizeof(sbuf[0].hashes));

	// Encrypt the user data.
	sbuf_tmp = sbuf;
	for (i = 0; i < 64; i++, sbuf_tmp++) {
		// User data IV is stored within the encrypted H2 table.
		aesw_set_iv(aesw, &sbuf[i].hashes.H2[7][4], 16);
		aesw_encrypt(aesw, sbuf_tmp->data, sizeof(sbuf_tmp->data));
	}

	// We're done here?
	return 0;
}

/**
 * Copy a bank from this RVT-H HDD or standalone disc image to a writable standalone disc image.
 *
 * This function copies an unencrypted Game Partition and encrypts it
 * using the existing title key. It does *not* change the encryption
 * method or signature, so recryption will be needed afterwards.
 *
 * @param rvth_dest	[out] Destination RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::copyToGcm_doCrypt(RvtH *rvth_dest, unsigned int bank_src,
	RvtH_Progress_Callback callback, void *userdata)
{
	uint32_t data_lba_src;	// Game partition, data offset LBA. (source, unencrypted)
	uint32_t data_lba_dest;	// Game partition, data offset LBA. (dest, encrypted)
	uint32_t lba_copy_len;	// Number of LBAs to copy. (game partition size)

	// Buffers.
	RVL_PartitionHeader pthdr;
	uint8_t *buf_dec = NULL;
	uint8_t *buf_enc = NULL;

	// H3 table.
	Wii_Disc_H3_t *H3_tbl = NULL;	// H3 hash table.
	uint8_t *pH3;			// Current H3 hash.
	RVL_Content_Entry *content;
	struct sha1_ctx sha1;

	// Current LBA counters.
	// Relative to the game partition.
	uint32_t data_offset;	// Partition data offset.
	uint32_t lba_count_dec;	// Decrypted (source)
	uint32_t lba_count_enc;	// Encrypted (destination)

	// Highest LBA that can be read from the source without
	// padding the buffer.
	uint32_t lba_max_dec;

	// Callback state.
	RvtH_Progress_State state;

	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	// Destination disc image.
	RvtH_BankEntry *entry_dest;

	// AES context.
	AesCtx *aesw = NULL;
	uint8_t titleKey[16];

	if (!rvth_dest) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank_src >= bankCount()) {
		errno = ERANGE;
		return -ERANGE;
	} else if (rvth_dest->isHDD() || rvth_dest->bankCount() != 1) {
		// Destination is not a standalone disc image.
		// Copying to HDDs will be handled differently.
		errno = EIO;
		return RVTH_ERROR_IS_HDD_IMAGE;
	}

	// Check if the source bank can be extracted.
	RvtH_BankEntry *const entry_src = &m_entries[bank_src];
	switch (entry_src->type) {
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Bank can be extracted.
			break;

		case RVTH_BankType_GCN:
			// No encryption for GameCube.
			errno = EIO;
			return RVTH_ERROR_NOT_WII_IMAGE;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			errno = EIO;
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			errno = ENOENT;
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			errno = EIO;
			return RVTH_ERROR_BANK_DL_2;
	}

	// Find the game partition.
	// TODO: Copy other partitions later?
	// TODO: Include disc headers in lba_copy_len?
	// TODO: Verify lba_copy_len.
	const pt_entry_t *const game_pte = rvth_ptbl_find_game(entry_src);
	if (!game_pte) {
		// Cannot find the game partition.
		err = EIO;
		ret = RVTH_ERROR_NO_GAME_PARTITION;
		goto end;
	}
	lba_copy_len = game_pte->lba_len;

	// TODO: Check the partition count.
	// If more than one partition, and the other partition
	// isn't an update partition, fail.

	// Process 64 sectors at a time.
	// TODO: Use unique_ptr<>?
	#define LBA_COUNT_DEC BYTES_TO_LBA(GROUP_SIZE_DEC)
	#define LBA_COUNT_ENC BYTES_TO_LBA(GROUP_SIZE_ENC)
	buf_dec = static_cast<uint8_t*>(malloc(GROUP_SIZE_DEC));
	buf_enc = static_cast<uint8_t*>(malloc(GROUP_SIZE_ENC));
	H3_tbl = static_cast<Wii_Disc_H3_t*>(calloc(1, sizeof(*H3_tbl)));	// zero initialized
	if (!buf_dec || !buf_enc || !H3_tbl) {
		// Error allocating memory.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		ret = -err;
		goto end;
	}

	// Initialize encryption.
	aesw = aesw_new();
	if (!aesw) {
		// Error initializing encryption.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto end;
	}

	// TODO: Set the expected file size.
	// Since we're writing mostly encrypted data, we aren't
	// going to make this a sparse file, but we should at least
	// tell the file system what the file's size will be.

	// Copy the bank table information.
	entry_dest = &rvth_dest->m_entries[0];
	entry_dest->type	= entry_src->type;
	entry_dest->region_code	= entry_src->region_code;
	entry_dest->is_deleted	= false;
	entry_dest->crypto_type	= entry_src->crypto_type;
	entry_dest->ios_version	= entry_src->ios_version;
	entry_dest->ticket	= entry_src->ticket;
	entry_dest->tmd		= entry_src->tmd;

	// Copy the disc header.
	memcpy(&entry_dest->discHeader, &entry_src->discHeader, sizeof(entry_dest->discHeader));

	// Timestamp.
	if (entry_src->timestamp >= 0) {
		entry_dest->timestamp = entry_src->timestamp;
	} else {
		entry_dest->timestamp = time(NULL);
	}

	// Copy the disc header.
	// TODO: Error handling.
	entry_src->reader->read(buf_dec, 0, 1);
	buf_dec[0x60] = 0;	// Hashes are enabled
	buf_dec[0x61] = 0;	// Disc is encrypted
	entry_dest->reader->write(buf_dec, 0, 1);

	// Create a volume group and partition table with a single entry.
	// TODO: Error handling.
	memset(buf_dec, 0, 512);
	{
		RVL_VolumeGroupTable *const vgtbl = (RVL_VolumeGroupTable*)&buf_dec[0];
		RVL_PartitionTableEntry *const pt = (RVL_PartitionTableEntry*)&buf_dec[sizeof(*vgtbl)];

		vgtbl->vg[0].count = cpu_to_be32(1);
		vgtbl->vg[0].addr = cpu_to_be32((uint32_t)((RVL_VolumeGroupTable_ADDRESS + sizeof(*vgtbl)) >> 2));
		pt->addr = cpu_to_be32((uint32_t)(LBA_TO_BYTES(game_pte->lba_start) >> 2));
		pt->type = cpu_to_be32(0);

		entry_dest->reader->write(buf_dec, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 1);
	}

	// Copy the region information.
	entry_src->reader->read(buf_dec, BYTES_TO_LBA(RVL_RegionSetting_ADDRESS), 1);
	entry_dest->reader->write(buf_dec, BYTES_TO_LBA(RVL_RegionSetting_ADDRESS), 1);

	// Copy the region information.
	// TODO: Error handling.
	entry_src->reader->read(buf_dec, BYTES_TO_LBA(RVL_RegionSetting_ADDRESS), 1);
	entry_dest->reader->write(buf_dec, BYTES_TO_LBA(RVL_RegionSetting_ADDRESS), 1);

	// Read the partition header.
	// This will be rewritten later, since we need to update the
	// content SHA-1 in the TMD.
	entry_src->reader->read(&pthdr, game_pte->lba_start, BYTES_TO_LBA(sizeof(pthdr)));

	// Data offset should be 0x8000 for unencrypted partitions.
	data_offset = be32_to_cpu(pthdr.data_offset) << 2;
	assert(data_offset == 0x8000);
	if (data_offset != 0x8000) {
		err = EIO;
		ret = RVTH_ERROR_PARTITION_HEADER_CORRUPTED;
		goto end;
	}

	// Calculate the data offset LBAs and adjust
	// lba_copy_len to skip the partition header.
	data_lba_src = game_pte->lba_start + BYTES_TO_LBA(data_offset);
	data_lba_dest = game_pte->lba_start + BYTES_TO_LBA(data_offset + sizeof(Wii_Disc_H3_t));
	lba_copy_len -= BYTES_TO_LBA(data_offset);

	if (callback) {
		// Initialize the callback state.
		// TODO: Fields for source vs. destination sizes?
		state.rvth = this;
		state.rvth_gcm = rvth_dest;
		state.bank_rvth = bank_src;
		state.bank_gcm = 0;
		state.type = RVTH_PROGRESS_EXTRACT;
		state.lba_processed = 0;
		state.lba_total = lba_copy_len;
	}

	// Decrypt the title key.
	ret = decrypt_title_key(&pthdr.ticket, titleKey, &entry_dest->crypto_type);
	if (ret != 0) {
		// Error decrypting the title key.
		err = EIO;
		goto end;
	}
	aesw_set_key(aesw, titleKey, sizeof(titleKey));

	// TODO: Optimize seeking? (Reader::write() seeks every time.)
	lba_max_dec = lba_copy_len - (lba_copy_len % LBA_COUNT_DEC);
	pH3 = H3_tbl->h3[0];
	for (lba_count_dec = 0, lba_count_enc = 0;
	     lba_count_dec < lba_max_dec;
	     lba_count_dec += LBA_COUNT_DEC, lba_count_enc += LBA_COUNT_ENC, pH3 += SHA1_DIGEST_SIZE)
	{
		if (callback) {
			bool bRet;
			state.lba_processed = lba_count_dec;
			bRet = callback(&state, userdata);
			if (!bRet) {
				// Stop processing.
				err = ECANCELED;
				ret = -ECANCELED;
				goto end;
			}
		}

		// TODO: Error handling.

		// Read 64 decrypted sectors.
		entry_src->reader->read(buf_dec, data_lba_src + lba_count_dec, LBA_COUNT_DEC);

		// Encrypt the sectors. (64*31k -> 64*32k)
		rvth_encrypt_group(aesw, buf_dec, GROUP_SIZE_DEC, buf_enc, GROUP_SIZE_ENC, pH3, SHA1_DIGEST_SIZE);

		// Write 64 encrypted sectors.
		entry_dest->reader->write(buf_enc, data_lba_dest + lba_count_enc, LBA_COUNT_ENC);
	}

	// If we have leftover, write a padded group.
	if (lba_count_dec < lba_copy_len) {
		const unsigned int lba_left = lba_copy_len - lba_count_dec;

		if (callback) {
			bool bRet;
			state.lba_processed = lba_count_dec;
			bRet = callback(&state, userdata);
			if (!bRet) {
				// Stop processing.
				err = ECANCELED;
				ret = -ECANCELED;
				goto end;
			}
		}

		// Read and pad the sectors.
		entry_src->reader->read(buf_dec, data_lba_src + lba_count_dec, lba_left);
		memset(&buf_dec[LBA_TO_BYTES(LBA_COUNT_DEC - lba_left)], 0, LBA_TO_BYTES(lba_left));

		// Encrypt the sectors. (64*31k -> 64*32k)
		rvth_encrypt_group(aesw, buf_dec, GROUP_SIZE_DEC, buf_enc, GROUP_SIZE_ENC, pH3, SHA1_DIGEST_SIZE);

		// Write 64 encrypted sectors.
		entry_dest->reader->write(buf_enc, data_lba_dest + lba_count_enc, LBA_COUNT_ENC);
	}

	/** Update the partition header. **/

	// H3 table offset. (0x8000 encrypted; not present unencrypted.)
	pthdr.h3_table_offset = cpu_to_be32(0x8000 >> 2);

	// Data offset. (0x20000 encrypted; 0x8000 unencrypted.)
	pthdr.data_offset = cpu_to_be32(
		be32_to_cpu(pthdr.data_offset) + (sizeof(*H3_tbl) >> 2));

	// Data size. (usually 0 in unencrypted images)
	pthdr.data_size = cpu_to_be32(LBA_TO_BYTES(lba_copy_len) >> 2);
	assert(pthdr.data_offset == cpu_to_be32(0x20000 >> 2));

	// H3 SHA-1 in the TMD.
	// FIXME: Figure out the correct content size.
	// - The Last Story, unencrypted: 4
	// - The Last Story, RVT-R: 0x3F8000
	content = (RVL_Content_Entry*)&pthdr.data[sizeof(RVL_TMD_Header)];
	content->size = cpu_to_be64(0x3F8000);
	sha1_init(&sha1);
	sha1_update(&sha1, sizeof(*H3_tbl), (const uint8_t*)H3_tbl);
	sha1_digest(&sha1, sizeof(content->sha1_hash), content->sha1_hash);

	// Write the partition header and H3 table.
	// TODO: Specific callback notice?
	entry_dest->reader->write(&pthdr,
		game_pte->lba_start, BYTES_TO_LBA(sizeof(pthdr)));
	entry_dest->reader->write(H3_tbl,
		game_pte->lba_start + BYTES_TO_LBA(sizeof(pthdr)),
		BYTES_TO_LBA(sizeof(*H3_tbl)));

	if (callback) {
		bool bRet;
		state.lba_processed = lba_copy_len;
		bRet = callback(&state, userdata);
		if (!bRet) {
			// Stop processing.
			err = ECANCELED;
			ret = -ECANCELED;
			goto end;
		}
	}

	// Finished extracting the disc image.
	entry_dest->reader->flush();

end:
	free(buf_dec);
	free(buf_enc);
	free(H3_tbl);
	aesw_free(aesw);
	if (err != 0) {
		errno = err;
	}
	return ret;
}
