/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * resign-wad.c: Re-sign a WAD file.                                       *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "resign-wad.hpp"
#include "print-info.h"
#include "wad-fns.h"

// libwiicrypto
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/common.h"
#include "libwiicrypto/priv_key_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wii_wad.h"

// C includes
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ includes
#include <memory>
using std::unique_ptr;

#define ISALNUM(c) isalnum((unsigned char)c)

typedef union _WAD_Header {
	Wii_WAD_Header wad;
	Wii_WAD_Header_BWF bwf;
} WAD_Header;

// Read buffer.
typedef union _rdbuf_t {
	uint8_t u8[READ_BUFFER_SIZE];
	RVL_Ticket ticket;
} rdbuf_t;

/**
 * Align the file pointer to the next 64-byte boundary.
 * @param fp File pointer.
 */
static inline void fpAlign(FILE *fp)
{
	off64_t offset = ftello(fp);
	if ((offset % 64) != 0) {
		offset = ALIGN_BYTES(64, offset);
		fseeko(fp, offset, SEEK_SET);
	}
}

/**
 * 'resign' command.
 * @param src_wad	[in] Source WAD.
 * @param dest_wad	[in] Destination WAD.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @param output_format	[in] Output format. (-1 for default)
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int resign_wad(const TCHAR *src_wad, const TCHAR *dest_wad, int recrypt_key, int output_format)
{
	int ret;
	size_t size;
	RVL_CryptoType_e src_key;
	RVL_AES_Keys_e toKey;

	bool isSrcBwf = false;
	bool isDestBwf = false;

	// Data offset:
	// - 0: Based on 64-byte alignment values. (WAD)
	// - Non-zero: Manually calculated. (BWF)
	uint32_t data_offset;

	uint32_t cert_chain_size;
	WAD_Header srcHeader, outHeader;
	WAD_Info_t wadInfo;

	// TMD
	unique_ptr<uint8_t[]> tmd_buf;
	RVL_TMD_Header *tmdHeader;

	// Key names
	const TCHAR *s_fromKey, *s_toKey;

	// Files
	FILE *f_src_wad = NULL, *f_dest_wad = NULL;
	off64_t src_file_size, offset;

	// Certificates
	const RVL_Cert_RSA4096_RSA2048 *cert_CA;
	const RVL_Cert_RSA2048 *cert_TMD, *cert_ticket;
	const RVL_Cert_RSA2048_ECC *cert_ms;
	const char *issuer_TMD;

	// Contents
	unsigned int nbr_cont, nbr_cont_actual;
	const RVL_Content_Entry *content;
	uint32_t data_size_actual;

	// Read buffer
	unique_ptr<rdbuf_t> buf(new rdbuf_t);

	// Open the source WAD file.
	errno = 0;
	f_src_wad = _tfopen(src_wad, _T("rb"));
	if (unlikely(!f_src_wad)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR opening source WAD file '%s': %s\n"), src_wad, _tcserror(err));
		return -err;
	}

	// Print the WAD information.
	// TODO: Should we verify the SHA-1s?
	ret = print_wad_info_FILE(f_src_wad, src_wad, false);
	if (ret != 0) {
		// Error printing the WAD information.
		return ret;
	}

	// Re-read the WAD header and parse the addresses.
	rewind(f_src_wad);
	errno = 0;
	size = fread(&srcHeader, 1, sizeof(srcHeader), f_src_wad);
	if (size != sizeof(srcHeader)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR reading WAD file '%s': %s\n"),
			src_wad, _tcserror(err));
		ret = -err;
		goto end;
	}

	// Identify the WAD type.
	// TODO: More extensive error handling?
	// NOTE: Not saving the string, since we only need to know if
	// it's a BroadOn WAD or not.
	if (identify_wad_type((const uint8_t*)&srcHeader, sizeof(srcHeader), &isSrcBwf) == NULL) {
		// Unrecognized WAD type.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' is not valid."), src_wad);
		ret = 1;
		goto end;
	}

	// Determine the sizes and addresses of various components.
	if (unlikely(!isSrcBwf)) {
		ret = getWadInfo(&srcHeader.wad, &wadInfo);
	} else {
		ret = getWadInfo_BWF(&srcHeader.bwf, &wadInfo);
	}
	if (ret != 0) {
		// Unable to get WAD information.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' is not valid."), src_wad);
		ret = 2;
		goto end;
	}

	// Verify the various sizes.
	if (wadInfo.ticket_size < sizeof(RVL_Ticket)) {
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' ticket size is too small. (%u; should be %u)\n"),
			src_wad, wadInfo.ticket_size, (uint32_t)sizeof(RVL_Ticket));
		ret = 3;
		goto end;
	} else if (wadInfo.ticket_size > WAD_TICKET_SIZE_MAX) {
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' ticket size is too big. (%u; should be %u)\n"),
			src_wad, wadInfo.ticket_size, (uint32_t)sizeof(RVL_Ticket));
		ret = 4;
		goto end;
	} else if (wadInfo.tmd_size < sizeof(RVL_TMD_Header)) {
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' TMD size is too small. (%u; should be at least %u)\n"),
			src_wad, wadInfo.tmd_size, (uint32_t)sizeof(RVL_TMD_Header));
		ret = 5;
		goto end;
	} else if (wadInfo.tmd_size > WAD_TMD_SIZE_MAX) {
		// Too big.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' TMD size is too big. (%u; should be less than 1 MiB)\n"),
			src_wad, wadInfo.tmd_size);
		ret = 6;
		goto end;
	} else if (wadInfo.meta_size > WAD_META_SIZE_MAX) {
		// Too big.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' ' metadata size is too big. (%u; should be less than 1 MB)\n"),
			src_wad, wadInfo.meta_size);
		ret = 7;
		goto end;
	}

	// Verify the data size.
	fseeko(f_src_wad, 0, SEEK_END);
	// FIXME: Why cast to uint32_t?
	src_file_size = (uint32_t)ftello(f_src_wad);
	if (isSrcBwf) {
		// Data size is the rest of the file.
		if (src_file_size < wadInfo.data_address) {
			// Not valid...
			_ftprintf(stderr, _T("*** ERROR: WAD file '%s' data size is invalid.\n"), src_wad);
			ret = 8;
			goto end;
		}
		wadInfo.data_size = (uint32_t)src_file_size - wadInfo.data_address;
	} else {
		// Verify the data size.
		if (src_file_size < wadInfo.data_address) {
			// File is too small.
			_ftprintf(stderr, _T("*** ERROR: WAD file '%s' data address is invalid.\n"), src_wad);
			ret = 9;
			goto end;
		} else if (src_file_size - wadInfo.data_address < wadInfo.data_size) {
			// Data size is too small.
			_ftprintf(stderr, _T("*** ERROR: WAD file '%s' data size is invalid.\n"), src_wad);
			ret = 10;
			goto end;
		}

		// TODO: More checks?
	}

	if (wadInfo.data_size > WAD_DATA_SIZE_MAX) {
		// Maximum of 256 MB.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s' data size is too big. (%u; should be less than 128 MiB)\n"),
			src_wad, wadInfo.data_size);
		ret = 11;
		goto end;
	}

	// Load the ticket.
	fseeko(f_src_wad, wadInfo.ticket_address, SEEK_SET);
	size = fread(buf->u8, 1, wadInfo.ticket_size, f_src_wad);
	if (size != wadInfo.ticket_size) {
		// Read error.
		_ftprintf(stderr, _T("*** ERROR: WAD file '%s': Unable to read the ticket.\n"), src_wad);
		ret = 13;
		goto end;
	}

	// Check the encryption key.
	switch (cert_get_issuer_from_name(buf->ticket.issuer)) {
		case RVL_CERT_ISSUER_PPKI_TICKET:
			// Retail may be either Common Key or Korean Key.
			switch (buf->ticket.common_key_index) {
				case 0:
					src_key = RVL_CryptoType_Retail;
					s_fromKey = _T("retail");
					break;
				case 1:
					src_key = RVL_CryptoType_Korean;
					s_fromKey = _T("Korean");
					break;
				case 2:
					src_key = RVL_CryptoType_vWii;
					s_fromKey = _T("vWii");
					break;
				default: {
					// NOTE: A good number of retail WADs have an
					// incorrect common key index for some reason.
					// NOTE 2: Warning is already printed by
					// print_wad_info(), so don't print one here.
					if (buf->ticket.title_id.u8[7] == 'K') {
						src_key = RVL_CryptoType_Korean;
						s_fromKey = _T("Korean");
					} else {
						src_key = RVL_CryptoType_Retail;
						s_fromKey = _T("retail");
					}
					break;
				}
			}
			break;
		case RVL_CERT_ISSUER_DPKI_TICKET:
			src_key = RVL_CryptoType_Debug;
			s_fromKey = _T("debug");
			break;
		default:
			_ftprintf(stderr, _T("*** ERROR: WAD file '%s': Unknown issuer.\n"), src_wad);
			ret = 14;
			goto end;
	}

	if (recrypt_key == -1 && output_format == -1) {
		// No key or format specified.
		// Default to converting to the "opposite" key and the same format.
		isDestBwf = isSrcBwf;
		output_format = (isDestBwf ? WAD_Format_BroadOn : WAD_Format_Standard);
		switch (src_key) {
			case RVL_CryptoType_Retail:
			case RVL_CryptoType_Korean:
			case RVL_CryptoType_vWii:
				recrypt_key = RVL_CryptoType_Debug;
				break;
			case RVL_CryptoType_Debug:
				recrypt_key = RVL_CryptoType_Retail;
				break;
			default:
				// Should not happen...
				assert(!"src_key: Invalid cryptoType.");
				_fputts(_T("*** ERROR: Unable to select encryption key.\n"), stderr);
				ret = 15;
				goto end;
		}
	} else {
		// If only key *or* format is specified, default the unspecified
		// parameter to the same as the input file.
		if (output_format == -1) {
			isDestBwf = isSrcBwf;
			output_format = (isDestBwf ? WAD_Format_BroadOn : WAD_Format_Standard);
		} else if (recrypt_key == -1) {
			isDestBwf = (output_format == WAD_Format_BroadOn);
			recrypt_key = src_key;
		}
	}

	assert(output_format != -1);
	if (output_format == -1) {
		// Default to WAD if something screwed up.
		output_format = WAD_Format_Standard;
		isDestBwf = false;
	}

	if (static_cast<RVL_CryptoType_e>(recrypt_key) == src_key) {
		// Allow the same key only if converting to a different format.
		if (isSrcBwf == isDestBwf) {
			// No point in recrypting to the same key and format...
			_fputts(_T("*** ERROR: Cannot recrypt to the same key and format.\n"), stderr);
			ret = 16;
			goto end;
		}
	}

	// Determine the key index.
	switch (recrypt_key) {
		case RVL_CryptoType_Debug:
			toKey = RVL_KEY_DEBUG;
			s_toKey = _T("debug (realsigned)");
			break;
		case RVL_CryptoType_Retail:
			toKey = RVL_KEY_RETAIL;
			s_toKey = _T("retail (fakesigned)");
			break;
		case RVL_CryptoType_Korean:
			// TODO: Add RVL_CryptoType_Korean_Debug?
			toKey = RVL_KEY_KOREAN;
			s_toKey = _T("Korean (fakesigned)");
			break;
		case RVL_CryptoType_vWii:
			// TODO: Add RVL_CryptoType_vWii_Debug?
			toKey = vWii_KEY_RETAIL;
			s_toKey = _T("vWii (fakesigned)");
			break;
		default:
			// Invalid key index.
			// This should not happen...
			assert(!"recrypt_key: Invalid key index.");
			_fputts(_T("*** ERROR: Invalid recrypt_key value.\n"), stderr);
			ret = 17;
			goto end;
	}

	_fputtc(_T('\n'), stdout);
	_tprintf(_T("Converting from %s to %s [%s->%s]...\n"),
		s_fromKey, s_toKey,
		isSrcBwf  ? _T("bwf") : _T("wad"),
		isDestBwf ? _T("bwf") : _T("wad"));

	// Open the destination WAD file.
	errno = 0;
	f_dest_wad = _tfopen(dest_wad, _T("wb"));
	if (!f_dest_wad) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR opening destination WAD file '%s' for write: %s\n"),
			dest_wad, _tcserror(err));
		ret = -err;
		goto end;
	}

	// Get the certificates.
	// TODO: Parse existing certificate chain to determine the ordering.
	if (toKey != RVL_KEY_DEBUG) {
		// Retail certificates.
		// Order: CA, TMD, Ticket
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_TMD);
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_PPKI_TICKET);
		cert_ms		= NULL;
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_PPKI_TMD];
		cert_chain_size	= (uint32_t)(sizeof(*cert_CA) + sizeof(*cert_TMD) + sizeof(*cert_ticket));
	} else {
		// Debug certificates.
		// Order: CA, TMD, Ticket, MS
		// FIXME: Not "dev" - MS is the "Mastering Server".
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_TMD);
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DPKI_TICKET);
		cert_ms		= (const RVL_Cert_RSA2048_ECC*)cert_get(RVL_CERT_ISSUER_DPKI_MS);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_DPKI_TMD];
		cert_chain_size	= (uint32_t)(sizeof(*cert_CA) + sizeof(*cert_TMD) + sizeof(*cert_ticket) + sizeof(*cert_ms));
	}

	// Determine the data offset.
	if (isDestBwf) {
		// TODO: Add meta.
		// NOTE: Size of Wii BWF header is accounted for by padding. Not adding here for now...
		data_offset = /*sizeof(Wii_WAD_Header_BWF) +*/
			ALIGN_BYTES(64, cert_chain_size) +
			ALIGN_BYTES(64, wadInfo.ticket_size) +
			ALIGN_BYTES(64, wadInfo.tmd_size);
	} else {
		// Data offset is implied based on alignments.
		data_offset = 0;
	}

	// Do we need to convert bwf->wad or wad->bwf?
	if (isSrcBwf) {
		if (!isDestBwf) {
			// bwf->wad
			_tprintf(_T("Converting the BroadOn WAD header to standard WAD format...\n"));
			data_offset = 0;

			// Type is 'Is' for most WADs, 'ib' for boot2.
			if (unlikely(
				buf->ticket.title_id.hi == cpu_to_be32(0x00000001) &&
				buf->ticket.title_id.lo == cpu_to_be32(0x00000001)))
			{
				outHeader.wad.type = cpu_to_be32(WII_WAD_TYPE_ib);
			} else {
				outHeader.wad.type = cpu_to_be32(WII_WAD_TYPE_Is);
			}

			outHeader.wad.header_size = cpu_to_be32(sizeof(outHeader));
			outHeader.wad.cert_chain_size = cpu_to_be32(cert_chain_size);
			outHeader.wad.crl_size = cpu_to_be32(wadInfo.crl_size);
			outHeader.wad.ticket_size = cpu_to_be32(wadInfo.ticket_size);
			outHeader.wad.tmd_size = cpu_to_be32(wadInfo.tmd_size);
			outHeader.wad.data_size = cpu_to_be32(wadInfo.data_size);
			outHeader.wad.meta_size = cpu_to_be32(wadInfo.meta_size);
		} else {
			// bwf->bwf
			// Modify the data offset and certificate chain size.
			outHeader.bwf = srcHeader.bwf;
			outHeader.bwf.data_offset = cpu_to_be32(data_offset);
			outHeader.bwf.cert_chain_size = cpu_to_be32(cert_chain_size);

			// FIXME: Copy the metadata to BWF correctly.
			outHeader.bwf.meta_size = 0;	//cpu_to_be32(wadInfo.meta_size);
			outHeader.bwf.meta_cid = 0;
		}
	} else /*if (!isSrcBwf)*/ {
		if (isDestBwf) {
			// wad->bwf
			_tprintf(_T("Converting the standard WAD header to BroadOn WAD format...\n"));

			outHeader.bwf.header_size = cpu_to_be32(sizeof(outHeader));
			outHeader.bwf.data_offset = cpu_to_be32(data_offset);
			outHeader.bwf.cert_chain_size = cpu_to_be32(cert_chain_size);
			outHeader.bwf.ticket_size = cpu_to_be32(wadInfo.ticket_size);
			outHeader.bwf.tmd_size = cpu_to_be32(wadInfo.tmd_size);
			// FIXME: Copy the metadata to BWF correctly.
			outHeader.bwf.meta_size = 0;	//cpu_to_be32(wadInfo.meta_size);
			outHeader.bwf.meta_cid = 0;
			outHeader.bwf.crl_size = cpu_to_be32(wadInfo.crl_size);
		} else {
			// wad->wad
			// Modify the certificate chain size.
			outHeader.wad = srcHeader.wad;
			outHeader.wad.cert_chain_size = cpu_to_be32(cert_chain_size);
		}
	}

	// Write the initial header. It will be rewritten later.
	size = fwrite(&outHeader, 1, sizeof(outHeader), f_dest_wad);
	if (size != sizeof(outHeader)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing initial destination WAD header: %s\n"),
			_tcserror(err));
		ret = -err;
		goto end;
	}

	if (!isDestBwf) {
		// 64-byte alignment. (WAD only)
		fpAlign(f_dest_wad);
	}

	// Write the certificates.
	_tprintf(_T("Writing certificate chain...\n"));
	errno = 0;
	size = fwrite(cert_CA, 1, sizeof(*cert_CA), f_dest_wad);
	if (size != sizeof(*cert_CA)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD certificate chain: %s\n"),
			_tcserror(err));
		ret = -err;
		goto end;
	}
	errno = 0;
	size = fwrite(cert_TMD, 1, sizeof(*cert_TMD), f_dest_wad);
	if (size != sizeof(*cert_TMD)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD certificate chain: %s\n"),
			_tcserror(err));
		ret = -err;
		goto end;
	}
	errno = 0;
	size = fwrite(cert_ticket, 1, sizeof(*cert_ticket), f_dest_wad);
	if (size != sizeof(*cert_ticket)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD certificate chain: %s\n"),
			_tcserror(err));
		ret = -err;
		goto end;
	}
	if (cert_ms) {
		errno = 0;
		size = fwrite(cert_ms, 1, sizeof(*cert_ms), f_dest_wad);
		if (size != sizeof(*cert_ms)) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			_ftprintf(stderr, _T("*** ERROR writing destination WAD certificate chain: %s\n"),
				_tcserror(err));
			ret = -err;
			goto end;
		}
	}

	if (!isDestBwf) {
		// 64-byte alignment. (WAD only)
		fpAlign(f_dest_wad);
	}

	// TODO: Copy the CRL if it's present. (WAD: goes after cert chain)
	// It seems Nintendo never used the CRL feature...
	assert(wadInfo.crl_size == 0);

	// Recrypt the ticket and TMD.
	_tprintf(_T("Recrypting the ticket and TMD...\n"));

	// Ticket is already loaded, so recrypt and resign it.
	errno = 0;
	ret = sig_recrypt_ticket(&buf->ticket, toKey);
	if (ret != 0) {
		// Error recrypting the ticket.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR recrypting the ticket: %s\n"), _tcserror(err));
		ret = -err;
		goto end;
	}
	// Sign the ticket.
	// TODO: Error checking.
	if (likely(toKey != RVL_KEY_DEBUG)) {
		// Retail: Fakesign the ticket.
		// Dolphin and cIOSes ignore the signature anyway.
		cert_fakesign_ticket(buf->u8, wadInfo.ticket_size);
	} else {
		// Debug: Use the real signing keys.
		// Debug IOS requires a valid signature.
		cert_realsign_ticketOrTMD(buf->u8, wadInfo.ticket_size, &rvth_privkey_RVL_dpki_ticket);
	}

	// Write the ticket.
	errno = 0;
	size = fwrite(&buf->ticket, 1, sizeof(buf->ticket), f_dest_wad);
	if (size != sizeof(buf->ticket)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD ticket: %s\n"), _tcserror(err));
		ret = -err;
		goto end;
	}

	if (!isDestBwf) {
		// 64-byte alignment. (WAD only)
		fpAlign(f_dest_wad);
	}

	// Load the TMD.
	tmd_buf.reset(new uint8_t[wadInfo.tmd_size]);
	fseeko(f_src_wad, wadInfo.tmd_address, SEEK_SET);
	errno = 0;
	size = fread(tmd_buf.get(), 1, wadInfo.tmd_size, f_src_wad);
	if (size != wadInfo.tmd_size) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR reading source WAD TMD: %s\n"), _tcserror(err));
		ret = -err;
		goto end;
	}
	tmdHeader = reinterpret_cast<RVL_TMD_Header*>(tmd_buf.get());
	nbr_cont = be16_to_cpu(tmdHeader->nbr_cont);

	// Make sure the TMD is big enough.
	// TODO: Show an error if it's not?
	content = (const RVL_Content_Entry*)(&tmd_buf[sizeof(*tmdHeader)]);
	nbr_cont_actual = (wadInfo.tmd_size - sizeof(*tmdHeader)) / sizeof(*content);
	if (nbr_cont > nbr_cont_actual) {
		nbr_cont = nbr_cont_actual;
	}

	// Change the issuer.
	// NOTE: MSVC Secure Overloads will change strncpy() to strncpy_s(),
	// which doesn't clear the buffer. Hence, we'll need to explicitly
	// clear the buffer first.
	memset(tmdHeader->issuer, 0, sizeof(tmdHeader->issuer));
	strncpy(tmdHeader->issuer, issuer_TMD, sizeof(tmdHeader->issuer));
	// Sign the TMD.
	// TODO: Error checking.
	if (likely(toKey != RVL_KEY_DEBUG)) {
		// Retail: Fakesign the TMD.
		// Dolphin and cIOSes ignore the signature anyway.
		cert_fakesign_tmd(tmd_buf.get(), wadInfo.tmd_size);
	} else {
		// Debug: Use the real signing keys.
		// Debug IOS requires a valid signature.
		cert_realsign_ticketOrTMD(tmd_buf.get(), wadInfo.tmd_size, &rvth_privkey_RVL_dpki_tmd);
	}

	// Write the TMD.
	errno = 0;
	size = fwrite(tmd_buf.get(), 1, wadInfo.tmd_size, f_dest_wad);
	if (size != wadInfo.tmd_size) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD TMD: %s\n"), _tcserror(err));
		ret = -err;
		goto end;
	}

	if (!isDestBwf) {
		// 64-byte alignment. (WAD only)
		fpAlign(f_dest_wad);
	}

	if (data_offset > 0) {
		// Seek to the data offset.
		int fsret = fseeko(f_dest_wad, data_offset, SEEK_SET);
		if (fsret != 0) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			_ftprintf(stderr, _T("*** ERROR seeking in destination WAD: %s\n"), _tcserror(err));
			ret = -err;
			goto end;
		}
	}

	// Copy each content, one megabyte at a time.
	// TODO: Show progress? (WADs are small enough that this probably isn't needed...)
	// TODO: Check for errors.
	fseeko(f_src_wad, wadInfo.data_address, SEEK_SET);
	data_size_actual = 0;
	for (; nbr_cont > 0; nbr_cont--, content++) {
		// TODO: Show the actual table index, or just the
		// index field in the entry?
		const uint32_t content_size = (uint32_t)be64_to_cpu(content->size);
		uint32_t size_to_copy = ALIGN_BYTES(16, content_size);
		uint16_t content_index = be16_to_cpu(content->index);
		_tprintf(_T("Copying WAD content #%d...\n"), content_index);

		// Contents are always physically AES-aligned (16 bytes), but the
		// data size in the header does not include extra bytes at the end
		// of the last content.
		//
		// If this is not the last content, it's also 64-byte aligned for WAD.
		// Padding following the last content is *not* included in the data size.
		// FIXME: Should we always pad data size to 64 bytes for WAD? Check Wii update partitions.
		data_size_actual += content_size;
		if (nbr_cont != 1) {
			if (likely(!isDestBwf)) {
				data_size_actual = ALIGN_BYTES(64, data_size_actual);
			} else {
				data_size_actual = ALIGN_BYTES(16, data_size_actual);
			}

			if (likely(!isSrcBwf && !isDestBwf)) {
				// Copying WAD to WAD. Include the padding as-is.
				size_to_copy = ALIGN_BYTES(64, size_to_copy);
			}
		}

		for (; size_to_copy >= sizeof(buf->u8); size_to_copy -= sizeof(buf->u8)) {
			errno = 0;
			size = fread(buf->u8, 1, sizeof(buf->u8), f_src_wad);
			if (size != sizeof(buf->u8)) {
				int err = errno;
				if (err == 0) {
					err = EIO;
				}
				_ftprintf(stderr, _T("*** ERROR reading source WAD data: %s\n"),
					_tcserror(err));
				ret = -err;
				goto end;
			}

			errno = 0;
			size = fwrite(buf->u8, 1, sizeof(buf->u8), f_dest_wad);
			if (size != sizeof(buf->u8)) {
				int err = errno;
				if (err == 0) {
					err = EIO;
				}
				_ftprintf(stderr, _T("*** ERROR writing destination WAD data: %s\n"),
					_tcserror(err));
				ret = -err;
				goto end;
			}
		}

		// Remaining data.
		if (size_to_copy > 0) {
			errno = 0;
			size = fread(buf->u8, 1, size_to_copy, f_src_wad);
			if (size != size_to_copy) {
				int err = errno;
				if (err == 0) {
					err = EIO;
				}
				_ftprintf(stderr, _T("*** ERROR reading source WAD data: %s\n"),
					_tcserror(err));
				ret = -err;
				goto end;
			}

			errno = 0;
			size = fwrite(buf->u8, 1, size_to_copy, f_dest_wad);
			if (size != size_to_copy) {
				int err = errno;
				if (err == 0) {
					err = EIO;
				}
				_ftprintf(stderr, _T("*** ERROR writing destination WAD data: %s\n"),
					_tcserror(err));
				ret = -err;
				goto end;
			}
		}
	}

	// Copy the metadata.
	// FIXME: Copy before the data if the output format is BWF.
	if (wadInfo.meta_size != 0) {
		_tprintf(_T("Copying the WAD metadata...\n"));

		fseeko(f_src_wad, wadInfo.meta_address, SEEK_SET);
		errno = 0;
		size = fread(buf->u8, 1, wadInfo.meta_size, f_src_wad);
		if (size != wadInfo.meta_size) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			_ftprintf(stderr, _T("*** ERROR reading source WAD metadata: %s\n"),
				_tcserror(err));
			ret = -err;
			goto end;
		}

		// FIXME: 64-byte alignment on BWF?
		if (!isDestBwf) {
			// 64-byte alignment.
			fpAlign(f_dest_wad);
		}

		size = fwrite(buf->u8, 1, wadInfo.meta_size, f_dest_wad);
		if (size != wadInfo.meta_size) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			_ftprintf(stderr, _T("*** ERROR writing destination WAD metadata: %s\n"),
				_tcserror(err));
			ret = -err;
			goto end;
		}
	}

	// TODO: Copy the CRL if it's present. (BWF: goes after meta)
	// It seems Nintendo never used the CRL feature...
	assert(wadInfo.crl_size == 0);

	if (!isDestBwf) {
		// Make sure the file a multiple of 64 bytes. (WAD only)
		offset = ftello(f_dest_wad);
		if (offset % 64 != 0) {
			const unsigned int count = 64 - (unsigned int)(offset % 64);

			// Using a fixed memset() for performance reasons.
			memset(buf->u8, 0, 64);

			errno = 0;
			size = fwrite(buf->u8, 1, count, f_dest_wad);
			if (size != count) {
				int err = errno;
				if (err == 0) {
					err = EIO;
				}
				_ftprintf(stderr, _T("*** ERROR writing destination WAD padding: %s\n"),
					_tcserror(err));
				ret = -err;
				goto end;
			}
		}
	}

	// Do we need to update the data size?
	if (likely(!isDestBwf) && unlikely(wadInfo.data_size != data_size_actual)) {
		_ftprintf(stderr, _T("*** Fixing WAD header's data size field:\n")
		                  _T("    Old: 0x%08X, New: 0x%08X\n"),
			wadInfo.data_size, data_size_actual);
		outHeader.wad.data_size = cpu_to_be32(data_size_actual);
	}

	// Write the WAD header.
	rewind(f_dest_wad);
	errno = 0;
	size = fwrite(&outHeader, 1, sizeof(outHeader), f_dest_wad);
	if (size != sizeof(outHeader)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		_ftprintf(stderr, _T("*** ERROR writing destination WAD header: %s\n"),
			_tcserror(err));
		ret = -err;
		goto end;
	}

	_tprintf(_T("WAD resigning complete.\n"));
	ret = 0;

end:
	if (f_dest_wad) {
		// TODO: Delete if an error occurred?
		fclose(f_dest_wad);
	}
	if (f_src_wad) {
		fclose(f_src_wad);
	}
	return ret;
}
