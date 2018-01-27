/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_recrypt.c: RVT-H "recryption" functions.                           *
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

#include "rvth_recrypt.h"
#include "rvth.h"
#include "rvth_p.h"

#include "reader.h"
#include "gcn_structs.h"
#include "cert.h"
#include "aesw.h"

#include "common.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

static inline uint32_t toNext64(uint32_t n)
{
	return (n + 63U) & ~63U;
}

/**
 * Re-encrypt a ticket's title key.
 * This will also change the issuer if necessary.
 *
 * NOTE: This function will NOT fakesign the ticket.
 * Call cert_fakesign_tmd() afterwards.
 * TODO: Real signing for debug.
 *
 * @param ticket Ticket.
 * @param toKey New key.
 * @return 0 on success; non-zero on error.
 */
static int rvth_recrypt_ticket(RVL_Ticket *ticket, RVL_AES_Keys_e toKey)
{
	// Common keys.
	RVL_AES_Keys_e fromKey;
	const uint8_t *key_from, *key_to;
	const char *issuer;
	uint8_t iv[16];	// based on Title ID

	// TODO: Error checking.
	// TODO: Pass in an aesw context for less overhead.
	AesCtx *aesw;

	// TODO: Determine the 'from' key by checking the
	// original issuer and key index.
	fromKey = RVL_KEY_DEBUG;
	if (fromKey == toKey) {
		// No recryption needed.
		return 0;
	}

	// Key data and 'To' issuer.
	key_from = RVL_AES_Keys[fromKey];
	key_to = RVL_AES_Keys[toKey];
	if (likely(toKey != RVL_KEY_DEBUG)) {
		issuer = RVL_Cert_Issuers[RVL_CERT_ISSUER_RETAIL_TICKET];
	} else {
		issuer = RVL_Cert_Issuers[RVL_CERT_ISSUER_DEBUG_TICKET];
	}

	// Initialize the AES context.
	errno = 0;
	aesw = aesw_new();
	if (!aesw) {
		int ret = -errno;
		if (ret == 0) {
			ret = -EIO;
		}
		return ret;
	}

	// IV is the 64-bit title ID, followed by zeroes.
	memcpy(iv, &ticket->title_id, 8);
	memset(&iv[8], 0, 8);

	// Decrypt the key with the original common key.
	aesw_set_key(aesw, key_from, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_decrypt(aesw, ticket->enc_title_key, sizeof(ticket->enc_title_key));

	// Encrypt the key with the new common key.
	aesw_set_key(aesw, key_to, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));
	aesw_encrypt(aesw, ticket->enc_title_key, sizeof(ticket->enc_title_key));

	// Update the issuer.
	strncpy(ticket->issuer, issuer, sizeof(ticket->issuer));

	// We're done here
	aesw_free(aesw);
	return 0;
}

/**
 * Re-encrypt partitions in a Wii disc image.
 *
 * This operation will *wipe* the update partition, since installing
 * retail updates on a debug system and vice-versa can result in a brick.
 *
 * NOTE: This function only supports converting from one encryption to
 * another. It does not support converting unencrypted to encrypted or
 * vice-versa.
 *
 * NOTE 2: Any partitions that are already encrypted with the specified key
 * will be left as-is.
 *
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param toKey	[in] Key to use for re-encryption.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_recrypt_partitions(RvtH *rvth, unsigned int bank, RVL_AES_Keys_e toKey)
{
	RvtH_BankEntry *entry;
	Reader *reader;
	uint32_t lba_size;
	unsigned int i, j;

	int ret = 0;	// errno or RvtH_Errors
	int err = 0;	// errno setting

	// Certificates.
	const RVL_Cert_RSA2048 *cert_ticket;
	const RVL_Cert_RSA4096_RSA2048 *cert_CA;
	const RVL_Cert_RSA2048 *cert_TMD;
	const char *issuer_TMD;

	// Sector buffer.
	// TODO: Dynamic allocation?
	union {
		uint8_t u8[RVTH_BLOCK_SIZE*2];
		GCN_DiscHeader gcn;
		struct {
			RVL_VolumeGroupTable vgtbl;
			RVL_PartitionTableEntry ptbl[31];
		};
	} sector_buf;

	// Partitions to re-encrypt.
	// NOTE: This only contains the LBA starting address.
	// The actual partition length is in the partition header.
	// TODO: Unencrypted partitions will need special handling.
	uint32_t pt_lba[31];
	unsigned int pt_count = 0;

	// Partitions to zero out. (update partitions)
	// NOTE: This only contains the LBA starting address.
	// The actual partition length is in the partition header.
	// TODO: Unencrypted partitions will need special handling.
	uint32_t upd_lba[31];
	unsigned int upd_count = 0;

	if (!rvth || toKey < RVL_KEY_RETAIL || toKey >= RVL_KEY_MAX) {
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= rvth->bank_count) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Check the bank type.
	entry = &rvth->entries[bank];
	switch (entry->type) {
		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Re-encryption is possible.
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

	// Is the disc encrypted?
	if (entry->crypto_type <= RVTH_CryptoType_None) {
		// Not encrypted. Cannot process it.
		return RVTH_ERROR_IS_UNENCRYPTED;
	}

	// NOTE: We're not checking for encryption/signature type,
	// since we're doing that for each partition individually.

	// Make the RVT-H object writable.
	ret = rvth_make_writable(rvth);
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		if (ret < 0) {
			err = -ret;
		} else {
			err = EROFS;
		}
		goto end;
	}

	// Get the volume group and partition tables.
	reader = entry->reader;
	errno = 0;
	lba_size = reader_read(reader, &sector_buf.u8, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 2);
	if (lba_size != 2) {
		// Read error.
		err = errno;
		if (err == 0) {
			errno = EIO;
		}
		ret = -err;
		goto end;
	}

	// All volume group tables must be within the two sectors.
	for (i = 0; i < ARRAY_SIZE(sector_buf.vgtbl.vg); i++) {
		int64_t addr;
		RVL_PartitionTableEntry *pte_start, *pte;

		uint32_t count = be32_to_cpu(sector_buf.vgtbl.vg[i].count);
		if (count == 0)
			continue;

		addr = (int64_t)be32_to_cpu(sector_buf.vgtbl.vg[i].addr) << 2;
		addr -= RVL_VolumeGroupTable_ADDRESS;
		if (addr + (count * sizeof(RVL_PartitionTableEntry)) > sizeof(sector_buf.u8)) {
			// Out of bounds.
			err = EIO;
			ret = RVTH_ERROR_PARTITION_TABLE_CORRUPTED;
			goto end;
		}

		// Process the partition table.
		pte_start = (RVL_PartitionTableEntry*)&sector_buf.u8[addr];
		pte = pte_start;
		for (j = 0; j < count; j++, pte++) {
			uint32_t lba_start = (uint32_t)((int64_t)be32_to_cpu(pte->addr) / (RVTH_BLOCK_SIZE/4));

			if (be32_to_cpu(pte->type) != 1) {
				// Standard partition.
				pt_lba[pt_count++] = lba_start;
			} else {
				// Update partition.
				upd_lba[upd_count++] = lba_start;
				// Shift the rest of the partitions over.
				memmove(pte, pte+1, (count-j) * sizeof(*pte));
				pte_start[count-1].addr = 0;
				pte_start[count-1].type = 0;
				// Decrement the counters to compensate for the shift.
				count--; j--; pte--;
			}
		}

		// Update the partition count.
		sector_buf.vgtbl.vg[i].count = cpu_to_be32(count);
	}

	// Write the updated volume group and partition tables.
	errno = 0;
	lba_size = reader_write(reader, &sector_buf.u8, BYTES_TO_LBA(RVL_VolumeGroupTable_ADDRESS), 2);
	if (lba_size != 2) {
		// Read error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		ret = -err;
		goto end;
	}

	// TODO: Zero out any update partitions.

	// Get the certificates.
	if (toKey != RVL_KEY_DEBUG) {
		// Retail certificates.
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_TICKET);
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_TMD);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_RETAIL_TMD];
	} else {
		// Debug certificates.
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_TICKET);
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_TMD);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_DEBUG_TMD];
	}

	// Process the other partitions.
	for (i = 0; i < pt_count; i++) {
		RVL_PartitionHeader hdr_orig;	// Original header
		RVL_PartitionHeader hdr_new;	// Rebuilt header
		uint32_t data_pos;		// Current position in hdr_new.u8[].
		uint32_t tmd_size, tmd_offset_orig;
		RVL_TMD_Header *tmdHeader;
		uint32_t cert_chain_size_new;
		uint8_t *p_cert_chain;

		//uint32_t tmd_size, tmd_offset;

		// Read the partition header.
		errno = 0;
		lba_size = reader_read(reader, &hdr_orig, pt_lba[i], BYTES_TO_LBA(sizeof(hdr_orig.u8)));
		if (lba_size != BYTES_TO_LBA(sizeof(hdr_orig))) {
			// Read error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			ret = -err;
			goto end;
		}

		// TODO: Check if the partition is already encrypted with the target keys.
		// If it is, skip it.
		memset(&hdr_new, 0, sizeof(hdr_new));

		// Copy in the ticket.
		memcpy(&hdr_new.ticket, &hdr_orig.ticket, sizeof(hdr_new.ticket));
		// Recrypt the key. (This also updates the issuer.)
		rvth_recrypt_ticket(&hdr_new.ticket, toKey);
		// Fakesign the ticket.
		// TODO: Error checking.
		cert_fakesign_ticket(&hdr_new.ticket);

		// Starting position.
		data_pos = toNext64(offsetof(RVL_PartitionHeader, data));

		// Copy in the TMD.
		tmd_size = be32_to_cpu(hdr_orig.tmd_size);
		tmd_offset_orig = be32_to_cpu(hdr_orig.tmd_offset) << 2;
		if (data_pos + tmd_size > sizeof(hdr_new)) {
			// Invalid...
			err = EIO;
			ret = RVTH_ERROR_PARTITION_HEADER_CORRUPTED;
			goto end;
		}
		memcpy(&hdr_new.u8[data_pos], &hdr_orig.u8[tmd_offset_orig], tmd_size);

		// Change the issuer.
		tmdHeader = (RVL_TMD_Header*)&hdr_new.u8[data_pos];
		strncpy(tmdHeader->issuer, issuer_TMD, sizeof(tmdHeader->issuer));
		// Fakesign the TMD.
		// TODO: Error checking.
		cert_fakesign_tmd(&hdr_new.u8[data_pos], tmd_size);

		// TMD parameters.
		hdr_new.tmd_size = hdr_orig.tmd_size;
		hdr_new.tmd_offset = cpu_to_be32(data_pos >> 2);
		data_pos += toNext64(tmd_size);

		// Write the new certificate chain.
		// NOTE: RVT-H images usually have a development certificate,
		// which makes the debug cert chain 0xC40 bytes. The retail
		// cert chain is 0xA00 bytes.
		cert_chain_size_new = sizeof(*cert_ticket) + sizeof(*cert_CA) + sizeof(*cert_TMD);
		if (data_pos + cert_chain_size_new > sizeof(hdr_new)) {
			// Invalid...
			err = EIO;
			ret = RVTH_ERROR_PARTITION_HEADER_CORRUPTED;
			goto end;
		}

		// Write the new certificate chain.
		// Cert chain order for retail is Ticket, CA, TMD.
		// TODO: Verify for debug! (and write the dev cert?)
		p_cert_chain = &hdr_new.u8[data_pos];
		memcpy(p_cert_chain, cert_ticket, sizeof(*cert_ticket));
		p_cert_chain += sizeof(*cert_ticket);
		memcpy(p_cert_chain, cert_CA, sizeof(*cert_CA));
		p_cert_chain += sizeof(*cert_CA);
		memcpy(p_cert_chain, cert_TMD, sizeof(*cert_TMD));

		hdr_new.cert_chain_size = cpu_to_be32(cert_chain_size_new);
		hdr_new.cert_chain_offset = cpu_to_be32(data_pos >> 2);

		// H3 table offset.
		// Copied as-is, since we're not changing it.
		hdr_new.h3_table_offset = hdr_orig.h3_table_offset;

		// Data offset and size.
		// TODO: If data size is 0, calculate it.
		hdr_new.data_offset = hdr_orig.data_offset;
		hdr_new.data_size = hdr_orig.data_size;

		// TODO: Special identifier.

		// Write the new partition header.
		errno = 0;
		lba_size = reader_write(reader, &hdr_new, pt_lba[i], BYTES_TO_LBA(sizeof(hdr_orig.u8)));
		if (lba_size != BYTES_TO_LBA(sizeof(hdr_new))) {
			// Write error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			ret = -err;
			goto end;
		}
	}

	// TODO: Update the bank entry.

	// https://github.com/mirror/wiimms-iso-tools/blob/master/src/libwbfs/file-formats.h

	// Fakesign notes:
	// - wiimm's iso tools has a slightly incorrect ticket layout.
	//   It uses the u32 at 0x24C (content_access_perm[0x2A]).
	//   We can probably use the final u8 values in this field.
	//   This would also be a unique identifier for what created
	//   the fake signature.
	// - For TMD, it uses the u32 at 0x19A. ("reserved")
	// Finished processing the disc image.
	reader_flush(reader);

end:
	if (err != 0) {
		errno = err;
	}
	return ret;
}
