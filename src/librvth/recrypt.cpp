/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * recrypt.cpp: RVT-H "recryption" functions.                              *
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
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/rsaw.h"
#include "libwiicrypto/priv_key_store.h"
#include "libwiicrypto/sig_tools.h"

#include "byteswap.h"

// C includes
#include <stdlib.h>
#include "time_r.h"

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>

// C++ includes
#include <array>
using std::array;

// Sector buffer (1 LBA)
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

// ID
static constexpr uint32_t id_exp = 0x00010001;
static const array<uint8_t, 256> id_pub = {{
	0xB5,0xBC,0x70,0x4C,0x75,0x3D,0xCF,0x02,0x67,0x04,0x1A,0xAB,0xC3,0xC8,0x20,0xD6,
	0x51,0xE8,0xE2,0xCC,0x6A,0x08,0xCF,0x70,0xEE,0xCF,0x45,0x20,0x27,0xCC,0x81,0x77,
	0x98,0xBB,0x22,0x82,0x61,0xA4,0x1B,0x52,0x19,0xC0,0x3F,0x50,0xAF,0xCE,0x6E,0xAB,
	0x22,0xF8,0xC2,0x23,0xC0,0xCF,0x18,0x82,0x72,0xDD,0xFC,0xF9,0xB9,0x7C,0x73,0x1E,
	0xBF,0xAB,0xDF,0x49,0x1F,0xCC,0x73,0x53,0xDF,0xB9,0x01,0xDA,0x13,0x5C,0x11,0x9E,
	0xA0,0x1E,0x7B,0xFA,0x61,0x2F,0x50,0xB1,0xDA,0x98,0x8F,0xB5,0x29,0x60,0x30,0x44,
	0x80,0x01,0x20,0xE1,0x03,0x24,0xFB,0xBA,0xDC,0x07,0xA0,0xBB,0x57,0x6F,0x37,0x38,
	0xD2,0xD2,0x44,0x81,0x5C,0xE5,0xF4,0xF6,0xDC,0x68,0x58,0x19,0x3D,0x8B,0xD8,0xEC,
	0x5D,0x8F,0x46,0x11,0x46,0x0E,0x2C,0xDA,0x00,0x47,0x0B,0xD7,0x24,0x70,0x7E,0x5B,
	0x6E,0xEF,0x7B,0xF0,0x3C,0x5A,0x55,0xD4,0x42,0xA2,0x03,0x88,0x0C,0x2C,0xB2,0xEB,
	0x98,0x96,0x15,0xAD,0xEE,0x99,0xAD,0x9D,0x1B,0xD6,0x16,0xF8,0x70,0x55,0xF1,0x43,
	0x12,0x5B,0x2B,0x51,0x1C,0x09,0x05,0xBC,0xD3,0xEA,0xD9,0x35,0xEA,0x20,0x54,0x1D,
	0x86,0xF2,0xC1,0xD1,0x60,0xEE,0x66,0x39,0xA2,0x75,0xCB,0x65,0xEC,0x53,0x24,0x5C,
	0x8F,0x06,0x25,0xD9,0xC1,0x88,0x03,0xEC,0xC3,0x0A,0xC2,0x72,0x49,0x4C,0x45,0xEF,
	0xAB,0x2F,0x66,0xA1,0x3C,0xDC,0x28,0x39,0xFD,0x64,0x33,0xDF,0x72,0x43,0xD9,0x65,
	0x2B,0xDF,0x94,0x14,0x0A,0x7B,0xE0,0xBA,0x40,0x29,0xC5,0x23,0x30,0x2C,0x14,0xC1
}};

/**
 * @param id ID buffer.
 * @param size Size of `id`. (Must be >= 256 bytes.)
 * @param gcn GameCube disc header.
 * @param extra Extra string.
 */
static int rvth_create_id(uint8_t *id, size_t size,
	const GCN_DiscHeader *gcn, const char *extra)
{
	// Leaving 16 bytes for the PKCS#1 header.
	static const array<uint8_t, 10> id_hdr = {{0x1B,0x1F,0x1D,0x01,0x1D,0x06,0x06,0x05,0x53,0x49}};
	uint8_t buf[256-16];

	char ts[64];
	struct tm tmbuf_utc;
	struct tm tmbuf_local;
	int tzoffset;
	const char *tzsign;
	char tzval[16];
	time_t now = time(NULL);

	assert(size >= 256);
	if (size < 256) {
		return -EINVAL;
	}

	// Clear the buffer.
	memset(buf, 0xFF, sizeof(buf));

	gmtime_r(&now, &tmbuf_utc);
	localtime_r(&now, &tmbuf_local);

	// Timezone offset.
	tzoffset = ((tmbuf_local.tm_hour * 60) + (tmbuf_local.tm_min)) -
		   ((tmbuf_utc.tm_hour * 60) + (tmbuf_utc.tm_min));
	tzsign = (tzoffset < 0 ? "-" : "");
	tzoffset = abs(tzoffset);
	snprintf(tzval, sizeof(tzval), "%s%02d%02d", tzsign,
		tzoffset / 60, tzoffset % 60);

	// ID, extra data and timestamp.
	memcpy(buf, id_hdr.data(), id_hdr.size());
	for (size_t i = 0; i < id_hdr.size(); i++) {
		buf[i] ^= 0x69;
	}
	strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", &tmbuf_local);
	if (extra) {
		snprintf((char*)&buf[sizeof(id_hdr)], /*0x40*/ sizeof(buf)-id_hdr.size(),
			 "%s, %s %s", extra, ts, tzval);
	} else {
		snprintf((char*)&buf[sizeof(id_hdr)], /*0x40*/ sizeof(buf)-id_hdr.size(),
			 "%s %s", ts, tzval);
	}

	// Disc header.
	memcpy(&buf[0x40], gcn, 0x68);

	// Encrypt it!
	return rsaw_encrypt(id, size, id_pub.data(), id_pub.size(), id_exp, buf, sizeof(buf));
}

int RvtHPrivate::recryptID(unsigned int bank)
{
	int err = 0;	// errno

	// Sector buffer.
	unsigned int lba_size;
	sbuf1_t sbuf;
	GCN_DiscHeader gcn;

	if (bank >= bankCount()) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Check the bank type.
	RvtH_BankEntry *const entry = &entries[bank];
	bool is_wii;
	switch (entry->type) {
		case RVTH_BankType_GCN:
			is_wii = false;
			break;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			is_wii = true;
			break;

		case RVTH_BankType_Unknown:
		default:
			// Unknown bank status...
			return RVTH_ERROR_BANK_UNKNOWN;

		case RVTH_BankType_Empty:
			// Bank is empty.
			return RVTH_ERROR_BANK_EMPTY;

		case RVTH_BankType_Wii_DL_Bank2:
			// Second bank of a dual-layer Wii disc image.
			// TODO: Automatically select the first bank?
			return RVTH_ERROR_BANK_DL_2;
	}

	// Make the RVT-H object writable.
	int ret = makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		if (ret < 0) {
			err = -ret;
		} else {
			err = EROFS;
		}
		errno = err;
		return ret;
	}

	// Get the GCN disc header.
	Reader *const reader = entry->reader;
	errno = 0;
	lba_size = reader->read(&sbuf.u8, 0, 1);
	if (lba_size != 1) {
		// Unable to read the disc header.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		errno = err;
		return ret;
	}

	memcpy(&gcn, &sbuf.gcn, sizeof(gcn));
	if (!is_wii) {
		// GCN. Write at 0x480.
		errno = 0;
		lba_size = reader->read(&sbuf.u8, BYTES_TO_LBA(0x400), 1);
		if (lba_size != 1) {
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			errno = err;
			return ret;
		}
		reader->flush();

		// Only if this area is empty!
		if (isBlockEmpty(&sbuf.u8[0x80], 256)) {
			rvth_create_id(&sbuf.u8[0x80], 256, &gcn, NULL);
			errno = 0;
			lba_size = reader->write(&sbuf.u8, BYTES_TO_LBA(0x400), 1);
			if (lba_size != 1) {
				err = errno;
				if (err == 0) {
					err = EIO;
				}
				errno = err;
				return ret;
			}
			reader->flush();
		}
	} else {
		// Wii. Write at the end of the partition header.

		// Make sure the partition table is loaded.
		ret = rvth_ptbl_load(entry);
		if (ret != 0 || entry->pt_count == 0 || !entry->ptbl) {
			// Unable to load the partition table.
			err = -ret;
			errno = err;
			return ret;
		}

		// Write the ID at the end of each partition header.
		const pt_entry_t *pte = entry->ptbl;
		for (unsigned int i = 0; i < entry->pt_count; i++, pte++) {
			uint8_t id_buf[512];	// need to read the LBA
			const uint32_t lba_id = pte->lba_start + BYTES_TO_LBA(0x7E00);

			// Read the last LBA of the partition header.
			errno = 0;
			lba_size = reader->read(id_buf, lba_id, BYTES_TO_LBA(sizeof(id_buf)));
			if (lba_size != BYTES_TO_LBA(sizeof(id_buf))) {
				// Read error.
				err = errno;
				if (err == 0) {
					err = EIO;
				}
				errno = err;
				return -err;
			}

			// Only if this area is empty!
			if (!isBlockEmpty(&id_buf[256], 256)) {
				continue;
			}

			// Write the identifier.
			char ptid_buf[24];
			snprintf(ptid_buf, sizeof(ptid_buf), "%up%u -> %up%u",
				pte->vg, pte->pt_orig,
				pte->vg, pte->pt);
			rvth_create_id(&id_buf[256], 256, &gcn, ptid_buf);

			// Write the updated LBA.
			errno = 0;
			lba_size = reader->write(id_buf, lba_id, BYTES_TO_LBA(sizeof(id_buf)));
			if (lba_size != BYTES_TO_LBA(sizeof(id_buf))) {
				// Write error.
				err = errno;
				if (err == 0) {
					err = EIO;
					errno = err;
				}
				return -err;
			}
			reader->flush();
		}
	}

	return ret;
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
 * will be left as-is; however, the tickets and TMDs wlil be re-signed.
 *
 * @param bank		[in] Bank number. (0-7)
 * @param cryptoType	[in] New encryption type.
 * @param callback	[in,opt] Progress callback.
 * @param userdata	[in,opt] User data for progress callback.
 * @param ios_force	[in,opt] IOS version to force. (-1 to use the existing IOS)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int RvtH::recryptWiiPartitions(unsigned int bank,
	RVL_CryptoType_e cryptoType,
	RvtH_Progress_Callback callback, void *userdata,
	int ios_force)
{
	uint32_t lba_size;

	int ret = 0;	// errno or RvtH_Errors

	// Certificates.
	const RVL_Cert_RSA2048 *cert_ticket;
	const RVL_Cert_RSA4096_RSA2048 *cert_CA;
	const RVL_Cert_RSA2048 *cert_TMD;
	const char *issuer_TMD;

	// Sector buffer.
	sbuf1_t sbuf;
	GCN_DiscHeader gcn;

	// Partitions to re-encrypt.
	// NOTE: This only contains the LBA starting address.
	// The actual partition length is in the partition header.
	// TODO: Unencrypted partitions will need special handling.
	const pt_entry_t *pte;

	// Callback state.
	RvtH_Progress_State state;

	if (cryptoType < RVL_CryptoType_Debug ||
	    cryptoType >= RVL_CryptoType_MAX)
	{
		errno = EINVAL;
		return -EINVAL;
	} else if (bank >= bankCount()) {
		// Bank number is out of range.
		errno = ERANGE;
		return -ERANGE;
	}

	// Check the bank type.
	RvtH_BankEntry *const entry = &d_ptr->entries[bank];
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
	if (entry->crypto_type <= RVL_CryptoType_None) {
		// Not encrypted. Cannot process it.
		return RVTH_ERROR_IS_UNENCRYPTED;
	}

	// Determine the key index.
	RVL_AES_Keys_e toKey;
	switch (cryptoType) {
		case RVL_CryptoType_Debug:
			toKey = RVL_KEY_DEBUG;
			break;
		case RVL_CryptoType_Retail:
			toKey = RVL_KEY_RETAIL;
			break;
		case RVL_CryptoType_Korean:
			// TODO: RVL_CryptoType_Korean_Debug?
			toKey = RVL_KEY_KOREAN;
			break;
		case RVL_CryptoType_vWii:
			// TODO: RVL_CryptoType_vWii_Debug?
			toKey = vWii_KEY_RETAIL;
			break;
		default:
			// Invalid key index.
			return -EINVAL;
	}

	// NOTE: We're not checking for encryption/signature type,
	// since we're doing that for each partition individually.

	// Make the RVT-H object writable.
	ret = d_ptr->makeWritable();
	if (ret != 0) {
		// Could not make the RVT-H object writable.
		int err;
		if (ret < 0) {
			err = -ret;
		} else {
			err = EROFS;
		}
		errno = err;
		return ret;
	}

	if (callback) {
		// Initialize the callback state.
		state.rvth = this;
		state.rvth_gcm = NULL;
		state.bank_rvth = bank;
		state.bank_gcm = ~0;
		// (0,1) because we're only recrypting the ticket(s) and TMD(s).
		// lba_processed == 0 indicates we're starting.
		// lba_processed == 1 indicates we're done.
		state.type = RVTH_PROGRESS_RECRYPT;
		state.lba_processed = 0;
		state.lba_total = 1;
		callback(&state, userdata);
	}

	// Get the GCN disc header.
	Reader *const reader = entry->reader;
	errno = 0;
	lba_size = reader->read(&sbuf.u8, 0, 1);
	if (lba_size != 1) {
		// Read error.
		int err = errno;
		if (err == 0) {
			err = EIO;
			errno = EIO;
		}
		return -err;
	}
	memcpy(&gcn, &sbuf.gcn, sizeof(gcn));

	// Make sure the partition table is loaded.
	ret = rvth_ptbl_load(entry);
	if (ret != 0 || entry->pt_count == 0 || !entry->ptbl) {
		// Unable to load the partition table.
		errno = -ret;
		return ret;
	}

	// Remove update partitions.
	// TODO: Actually zero them out instead of just
	// removing them from the partition table.
	ret = rvth_ptbl_RemoveUpdates(entry);
	if (ret != 0 || entry->pt_count == 0 || !entry->ptbl) {
		// Unable to remove update partitions.
		errno = -ret;
		return ret;
	}

	// Write the updated partition table.
	ret = rvth_ptbl_write(entry);
	if (ret != 0) {
		// Unable to write the updated partition table.
		errno = -ret;
		return ret;
	}

	// Get the certificates.
	if (toKey != RVL_KEY_DEBUG) {
		// Retail certificates.
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_TICKET);
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_TMD);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_PPKI_TMD];
	} else {
		// Debug certificates.
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_TICKET);
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_TMD);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_DPKI_TMD];
	}

	// Process the other partitions.
	pte = entry->ptbl;
	for (unsigned int i = 0; i < entry->pt_count; i++, pte++) {
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
		lba_size = reader->read(&hdr_orig, pte->lba_start, BYTES_TO_LBA(sizeof(hdr_orig.u8)));
		if (lba_size != BYTES_TO_LBA(sizeof(hdr_orig))) {
			// Read error.
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			return -err;
		}

		// TODO: Check if the partition is already encrypted with the target keys.
		// If it is, skip it.
		memset(&hdr_new, 0, sizeof(hdr_new));

		// Copy in the ticket.
		memcpy(&hdr_new.ticket, &hdr_orig.ticket, sizeof(hdr_new.ticket));
		// Recrypt the ticket. (This also updates the issuer.)
		ret = sig_recrypt_ticket(&hdr_new.ticket, toKey);
		if (ret != 0) {
			// Error recrypting the ticket.
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			return -err;
		}
		// Sign the ticket.
		// TODO: Error checking.
		// TODO: Support larger tickets.
		if (likely(toKey != RVL_KEY_DEBUG)) {
			// Retail: Fakesign the ticket.
			// Dolphin and cIOSes ignore the signature anyway.
			cert_fakesign_ticket((uint8_t*)&hdr_new.ticket, sizeof(hdr_new.ticket));
		} else {
			// Debug: Use the real signing keys.
			// Debug IOS requires a valid signature.
			cert_realsign_ticketOrTMD((uint8_t*)&hdr_new.ticket, sizeof(hdr_new.ticket), &rvth_privkey_RVL_dpki_ticket);
		}

		// Starting position.
		data_pos = offsetof(RVL_PartitionHeader, data);
		data_pos = ALIGN_BYTES(64, data_pos);

		// Copy in the TMD.
		tmd_size = be32_to_cpu(hdr_orig.tmd_size);
		tmd_offset_orig = be32_to_cpu(hdr_orig.tmd_offset) << 2;
		if (data_pos + tmd_size > sizeof(hdr_new)) {
			// Invalid...
			errno = EIO;
			return RVTH_ERROR_PARTITION_HEADER_CORRUPTED;
		}
		memcpy(&hdr_new.u8[data_pos], &hdr_orig.u8[tmd_offset_orig], tmd_size);

		// Change the issuer.
		tmdHeader = (RVL_TMD_Header*)&hdr_new.u8[data_pos];
		// NOTE: Clearing the buffer and using snprintf().
		memset(tmdHeader->issuer, 0, sizeof(tmdHeader->issuer));
		snprintf(tmdHeader->issuer, sizeof(tmdHeader->issuer), "%s", issuer_TMD);

		// Change the IOS if necessary.
		if (ios_force >= 3) {
			uint32_t ios_uint = static_cast<uint32_t>(ios_force);
			if (ios_uint != be32_to_cpu(tmdHeader->sys_version.lo)) {
				tmdHeader->sys_version.lo = cpu_to_be32(ios_uint);
			}
		}

		// Sign the TMD.
		// TODO: Error checking.
		if (likely(toKey != RVL_KEY_DEBUG)) {
			// Retail: Fakesign the TMD.
			// Dolphin and cIOSes ignore the signature anyway.
			cert_fakesign_tmd(&hdr_new.u8[data_pos], tmd_size);
		} else {
			// Debug: Use the real signing keys.
			// Debug IOS requires a valid signature.
			cert_realsign_ticketOrTMD(&hdr_new.u8[data_pos], tmd_size, &rvth_privkey_RVL_dpki_tmd);
		}

		// TMD parameters.
		hdr_new.tmd_size = hdr_orig.tmd_size;
		hdr_new.tmd_offset = cpu_to_be32(data_pos >> 2);
		data_pos += ALIGN_BYTES(64, tmd_size);

		// Write the new certificate chain.
		// NOTE: RVT-H images usually have a development certificate,
		// which makes the debug cert chain 0xC40 bytes. The retail
		// cert chain is 0xA00 bytes.
		cert_chain_size_new = sizeof(*cert_ticket) + sizeof(*cert_CA) + sizeof(*cert_TMD);
		if (data_pos + cert_chain_size_new > sizeof(hdr_new)) {
			// Invalid...
			errno = EIO;
			return RVTH_ERROR_PARTITION_HEADER_CORRUPTED;
		}

		// Certificate chain order for retail is Ticket, CA, TMD.
		// TODO: Verify for debug! (and write the dev cert?)
		// NOTE: WAD cert chain order is CA, Ticket, TMD...
		// (CA, Ticket, TMD, Dev for debug)
		// TODO: Verify all of this.
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

		// Write the identifier.
		// (Only if this area is empty!)
		if (d_ptr->isBlockEmpty(&hdr_new.data[sizeof(hdr_new.data)-256], 256)) {
			char ptid_buf[24];
			snprintf(ptid_buf, sizeof(ptid_buf), "%up%u -> %up%u",
				pte->vg, pte->pt_orig,
				pte->vg, pte->pt);
			rvth_create_id(&hdr_new.data[sizeof(hdr_new.data)-256], 256, &gcn, ptid_buf);
		}

		// Write the new partition header.
		errno = 0;
		lba_size = reader->write(&hdr_new, pte->lba_start, BYTES_TO_LBA(sizeof(hdr_new.u8)));
		if (lba_size != BYTES_TO_LBA(sizeof(hdr_new))) {
			// Write error.
			int err = errno;
			if (err == 0) {
				err = EIO;
				errno = EIO;
			}
			return -err;
		}
	}

	// Update the bank entry.
	switch (toKey) {
		case RVL_KEY_RETAIL:
			entry->crypto_type = RVL_CryptoType_Retail;
			entry->ticket.sig_type = RVL_SigType_Retail;
			entry->ticket.sig_status = RVL_SigStatus_Fake;
			entry->tmd.sig_type = RVL_SigType_Retail;
			entry->tmd.sig_status = RVL_SigStatus_Fake;
			break;
		case RVL_KEY_KOREAN:
			entry->crypto_type = RVL_CryptoType_Korean;
			entry->ticket.sig_type = RVL_SigType_Retail;
			entry->ticket.sig_status = RVL_SigStatus_Fake;
			entry->tmd.sig_type = RVL_SigType_Retail;
			entry->tmd.sig_status = RVL_SigStatus_Fake;
			break;
		case vWii_KEY_RETAIL:
			entry->crypto_type = RVL_CryptoType_vWii;
			entry->ticket.sig_type = RVL_SigType_Retail;
			entry->ticket.sig_status = RVL_SigStatus_Fake;
			entry->tmd.sig_type = RVL_SigType_Retail;
			entry->tmd.sig_status = RVL_SigStatus_Fake;
			break;

		case RVL_KEY_DEBUG:
			entry->crypto_type = RVL_CryptoType_Debug;
			entry->ticket.sig_type = RVL_SigType_Debug;
			entry->ticket.sig_status = RVL_SigStatus_OK;
			entry->tmd.sig_type = RVL_SigType_Debug;
			entry->tmd.sig_status = RVL_SigStatus_OK;
			break;
		case RVL_KEY_KOREAN_DEBUG:
			// TODO: RVL_CryptoType_Korean_Debug?
			entry->crypto_type = RVL_CryptoType_Korean;
			entry->ticket.sig_type = RVL_SigType_Debug;
			entry->ticket.sig_status = RVL_SigStatus_OK;
			entry->tmd.sig_type = RVL_SigType_Debug;
			entry->tmd.sig_status = RVL_SigStatus_OK;
			break;
		case vWii_KEY_DEBUG:
			// TODO: RVL_CryptoType_vWii_Debug?
			entry->crypto_type = RVL_CryptoType_vWii;
			entry->ticket.sig_type = RVL_SigType_Debug;
			entry->ticket.sig_status = RVL_SigStatus_OK;
			entry->tmd.sig_type = RVL_SigType_Debug;
			entry->tmd.sig_status = RVL_SigStatus_OK;
			break;
		default:
			// Should not happen...
			assert(false);
			break;
	}

	// If this is an HDD, write the bank table entry.
	if (isHDD()) {
		// TODO: Check for errors.
		d_ptr->writeBankEntry(bank);
	}

	// Finished processing the disc image.
	reader->flush();

	if (callback) {
		state.lba_processed = 1;
		callback(&state, userdata);
	}

	return ret;
}
