/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * print-info.c: Print WAD information.                                    *
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

#include "resign-wad.h"
#include "print-info.h"
#include "wad-fns.h"

// libwiicrypto
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/common.h"
#include "libwiicrypto/priv_key_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wii_wad.h"

// C includes.
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISALNUM(c) isalnum((unsigned char)c)

typedef union _WAD_Header {
	Wii_WAD_Header wad;
	Wii_WAD_Header_EARLY wadE;
} WAD_Header;

// Read buffer.
typedef union _rdbuf_t {
	uint8_t u8[READ_BUFFER_SIZE];
	RVL_Ticket ticket;
	RVL_TMD_Header tmdHeader;
} rdbuf_t;

/**
 * Align the file pointer to the next 64-byte boundary.
 * @param fp File pointer.
 */
static inline void fpAlign(FILE *fp)
{
	int64_t offset = ftello(fp);
	if ((offset % 64) != 0) {
		offset = ALIGN(64, offset);
		fseeko(fp, offset, SEEK_SET);
	}
}

/**
 * 'resign' command.
 * @param src_wad	[in] Source WAD.
 * @param dest_wad	[in] Destination WAD.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int resign_wad(const TCHAR *src_wad, const TCHAR *dest_wad, int recrypt_key)
{
	int ret;
	size_t size;
	bool isEarly = false;
	WAD_Header header;
	WAD_Info_t wadInfo;
	RVL_CryptoType_e src_key;
	RVL_AES_Keys_e toKey;

	// Key names.
	const char *s_fromKey, *s_toKey;

	// Files.
	FILE *f_src_wad = NULL, *f_dest_wad = NULL;
	int64_t src_file_size, offset;
	const char *s_footer_name;	// "footer" or "name"

	// Certificates.
	const RVL_Cert_RSA4096_RSA2048 *cert_CA;
	const RVL_Cert_RSA2048 *cert_TMD, *cert_ticket;
	const RVL_Cert_RSA2048_ECC *cert_dev;
	const char *issuer_TMD;

	// Read buffer.
	rdbuf_t *buf = NULL;
	uint32_t data_sz;

	// Open the source WAD file.
	errno = 0;
	f_src_wad = _tfopen(src_wad, _T("rb"));
	if (unlikely(!f_src_wad)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fputs("*** ERROR opening source WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "': %s\n", strerror(err));
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
	size = fread(&header, 1, sizeof(header), f_src_wad);
	if (size != sizeof(header)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fputs("*** ERROR reading WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "': %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// Identify the WAD type.
	// TODO: More extensive error handling?
	// NOTE: Not saving the string, since we only need to know if
	// it's an early WAD or not.
	if (identify_wad_type((const uint8_t*)&header, sizeof(header), &isEarly) == NULL) {
		// Unrecognized WAD type.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' is not valid.\n");
		ret = 1;
		goto end;
	}

	// Determine the sizes and addresses of various components.
	if (unlikely(!isEarly)) {
		ret = getWadInfo(&header.wad, &wadInfo);
		s_footer_name = "footer";
	} else {
		ret = getWadInfo_early(&header.wadE, &wadInfo);
		s_footer_name = "name";
	}
	if (ret != 0) {
		// Unable to get WAD information.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' is not valid.");
		ret = 2;
		goto end;
	}

	// Verify the various sizes.
	if (wadInfo.ticket_size != sizeof(RVL_Ticket)) {
		// Incorrect ticket size.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' ticket size is incorrect. (%u; should be %u)\n",
			wadInfo.ticket_size, (uint32_t)sizeof(RVL_Ticket));
		ret = 3;
		goto end;
	} else if (wadInfo.tmd_size < sizeof(RVL_TMD_Header)) {
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' TMD size is too small. (%u; should be at least %u)\n",
			wadInfo.tmd_size, (uint32_t)sizeof(RVL_TMD_Header));
		ret = 4;
		goto end;
	} else if (wadInfo.tmd_size > WAD_TMD_SIZE_MAX) {
		// Too big.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' TMD size is too big. (%u; should be less than 1 MB)\n",
			wadInfo.tmd_size);
		ret = 5;
		goto end;
	} else if (wadInfo.footer_size > WAD_FOOTER_SIZE_MAX) {
		// Too big.
		// TODO: Define a maximum footer size somewhere.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' %s size is too big. (%u; should be less than 128 KB)\n",
			s_footer_name, wadInfo.tmd_size);
		ret = 6;
		goto end;
	}

	// Verify the data size.
	fseeko(f_src_wad, 0, SEEK_END);
	src_file_size = (uint32_t)ftello(f_src_wad);
	if (isEarly) {
		// Data size is the rest of the file.
		if (src_file_size < wadInfo.data_address) {
			// Not valid...
			fputs("*** ERROR: WAD file '", stderr);
			_fputts(src_wad, stderr);
			fputs("' data size is invalid.\n", stderr);
			ret = 7;
			goto end;
		}
		wadInfo.data_size = (uint32_t)src_file_size - wadInfo.data_address;
	} else {
		// Verify the data size.
		if (src_file_size < wadInfo.data_address) {
			// File is too small.
			fputs("*** ERROR: WAD file '", stderr);
			_fputts(src_wad, stderr);
			fputs("' data address is invalid.\n", stderr);
			ret = 8;
			goto end;
		} else if (src_file_size - wadInfo.data_address < wadInfo.data_size) {
			// Data size is too small.
			fputs("*** ERROR: WAD file '", stderr);
			_fputts(src_wad, stderr);
			fputs("' data size is invalid.\n", stderr);
			ret = 9;
			goto end;
		}

		// TODO: More checks?
	}

	if (wadInfo.data_size > WAD_DATA_SIZE_MAX) {
		// Maximum of 128 MB.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fprintf(stderr, "' data size is too big. (%u; should be less than 128 MB)\n",
			wadInfo.data_size);
		ret = 10;
		goto end;
	}

	// Allocate the memory buffer.
	buf = malloc(sizeof(*buf));
	if (!buf) {
		fputs("*** ERROR: Unable to allocate memory buffer.\n", stderr);
		ret = 11;
		goto end;
	}

	// Load the ticket.
	fseeko(f_src_wad, wadInfo.ticket_address, SEEK_SET);
	size = fread(&buf->ticket, 1, sizeof(buf->ticket), f_src_wad);
	if (size != sizeof(RVL_Ticket)) {
		// Read error.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(src_wad, stderr);
		fputs("': Unable to read the ticket.\n", stderr);
		ret = 12;
		goto end;
	}

	// Check the encryption key.
	switch (cert_get_issuer_from_name(buf->ticket.issuer)) {
		case RVL_CERT_ISSUER_RETAIL_TICKET:
			// Retail may be either Common Key or Korean Key.
			switch (buf->ticket.common_key_index) {
				case 0:
					src_key = RVL_CryptoType_Retail;
					s_fromKey = "retail";
					break;
				case 1:
					src_key = RVL_CryptoType_Korean;
					s_fromKey = "Korean";
					break;
				default: {
					// NOTE: A good number of retail WADs have an
					// incorrect common key index for some reason.
					// NOTE 2: Warning is already printed by
					// print_wad_info(), so don't print one here.
					if (buf->ticket.title_id.u8[7] == 'K') {
						src_key = RVL_CryptoType_Korean;
						s_fromKey = "Korean";
					} else {
						src_key = RVL_CryptoType_Retail;
						s_fromKey = "retail";
					}
					break;
				}
			}
			break;
		case RVL_CERT_ISSUER_DEBUG_TICKET:
			src_key = RVL_CryptoType_Debug;
			s_fromKey = "debug";
			break;
		default:
			fputs("*** ERROR: WAD file '", stderr);
			_fputts(src_wad, stderr);
			fputs("': Unknown issuer.\n", stderr);
			ret = 13;
			goto end;
	}

	if (recrypt_key == -1) {
		// Select the "opposite" key.
		switch (src_key) {
			case RVL_CryptoType_Retail:
			case RVL_CryptoType_Korean:
				recrypt_key = RVL_CryptoType_Debug;
				break;
			case RVL_CryptoType_Debug:
				recrypt_key = RVL_CryptoType_Retail;
				break;
			default:
				// Should not happen...
				assert(!"src_key: Invalid cryptoType.");
				fputs("*** ERROR: Unable to select encryption key.\n", stderr);
				ret = 14;
				goto end;
		}
	} else {
		if ((RVL_CryptoType_e)recrypt_key == src_key) {
			// No point in recrypting to the same key...
			fputs("*** ERROR: Cannot recrypt to the same key.\n", stderr);
			ret = 15;
			goto end;
		}
	}

	// Determine the key index.
	switch (recrypt_key) {
		case RVL_CryptoType_Debug:
			toKey = RVL_KEY_DEBUG;
			s_toKey = "debug";
			break;
		case RVL_CryptoType_Retail:
			toKey = RVL_KEY_RETAIL;
			s_toKey = "retail (fakesigned)";
			break;
		case RVL_CryptoType_Korean:
			toKey = RVL_KEY_KOREAN;
			s_toKey = "Korean (fakesigned)";
			break;
		default:
			// Invalid key index.
			// This should not happen...
			assert(!"recrypt_key: Invalid key index.");
			fputs("*** ERROR: Invalid recrypt_key value.\n", stderr);
			ret = 16;
			goto end;
	}

	printf("Converting from %s to %s...\n", s_fromKey, s_toKey);

	// Open the destination WAD file.
	errno = 0;
	f_dest_wad = _tfopen(dest_wad, _T("wb"));
	if (!f_dest_wad) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fputs("*** ERROR opening destination WAD file '", stderr);
		_fputts(dest_wad, stderr);
		fprintf(stderr, "' for write: %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// Get the certificates.
	if (toKey != RVL_KEY_DEBUG) {
		// Retail certificates.
		// Order: CA, TMD, Ticket
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_TMD);
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_RETAIL_TICKET);
		cert_dev	= NULL;
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_RETAIL_TMD];
		header.wad.cert_chain_size = cpu_to_be32((uint32_t)(
			sizeof(*cert_CA) + sizeof(*cert_TMD) +
			sizeof(*cert_ticket)));
	} else {
		// Debug certificates.
		// Order: CA, TMD, Ticket
		cert_CA		= (const RVL_Cert_RSA4096_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_CA);
		cert_TMD	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_TMD);
		cert_ticket	= (const RVL_Cert_RSA2048*)cert_get(RVL_CERT_ISSUER_DEBUG_TICKET);
		cert_dev	= (const RVL_Cert_RSA2048_ECC*)cert_get(RVL_CERT_ISSUER_DEBUG_DEV);
		issuer_TMD	= RVL_Cert_Issuers[RVL_CERT_ISSUER_DEBUG_TMD];
		header.wad.cert_chain_size = cpu_to_be32((uint32_t)(
			sizeof(*cert_CA) + sizeof(*cert_TMD) +
			sizeof(*cert_ticket) + sizeof(*cert_dev)));
	}

	if (isEarly) {
		// Convert the WAD header to the standard format.
		printf("Converting the early devkit WAD header to standard WAD format...\n");
		// Type is 'Is' for most WADs, 'ib' for boot2.
		if (unlikely(
			buf->ticket.title_id.hi == cpu_to_be32(0x00000001) &&
			buf->ticket.title_id.lo == cpu_to_be32(0x00000001)))
		{
			header.wad.type = cpu_to_be32(WII_WAD_TYPE_ib);
		} else {
			header.wad.type = cpu_to_be32(WII_WAD_TYPE_Is);
		}

		header.wad.reserved = 0;
		header.wad.ticket_size = cpu_to_be32(wadInfo.ticket_size);
		header.wad.tmd_size = cpu_to_be32(wadInfo.tmd_size);
		header.wad.data_size = cpu_to_be32(wadInfo.data_size);
		header.wad.footer_size = cpu_to_be32(wadInfo.footer_size);
	}

	// Write the WAD header.
	errno = 0;
	size = fwrite(&header.wad, 1, sizeof(header.wad), f_dest_wad);
	if (size != sizeof(header.wad)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR writing destination WAD header: %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// 64-byte alignment.
	fpAlign(f_dest_wad);

	// Write the certificates.
	printf("Writing certificate chain...\n");
	errno = 0;
	size = fwrite(cert_CA, 1, sizeof(*cert_CA), f_dest_wad);
	if (size != sizeof(*cert_CA)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR writing destination WAD certificate chain: %s\n", strerror(err));
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
		fprintf(stderr, "*** ERROR writing destination WAD certificate chain: %s\n", strerror(err));
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
		fprintf(stderr, "*** ERROR writing destination WAD certificate chain: %s\n", strerror(err));
		ret = -err;
		goto end;
	}
	if (cert_dev) {
		errno = 0;
		size = fwrite(cert_dev, 1, sizeof(*cert_dev), f_dest_wad);
		if (size != sizeof(*cert_dev)) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR writing destination WAD certificate chain: %s\n", strerror(err));
			ret = -err;
			goto end;
		}
	}

	// 64-byte alignment.
	fpAlign(f_dest_wad);

	// Recrypt the ticket and TMD.
	printf("Recrypting the ticket and TMD...\n");

	// Ticket is already loaded, so recrypt and resign it.
	errno = 0;
	ret = sig_recrypt_ticket(&buf->ticket, toKey);
	if (ret != 0) {
		// Error recrypting the ticket.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR recrypting the ticket: %s\n", strerror(err));
		ret = -err;
		goto end;
	}
	// Sign the ticket.
	// TODO: Error checking.
	if (likely(toKey != RVL_KEY_DEBUG)) {
		// Retail: Fakesign the ticket.
		// Dolphin and cIOSes ignore the signature anyway.
		cert_fakesign_ticket(&buf->ticket);
	} else {
		// Debug: Use the real signing keys.
		// Debug IOS requires a valid signature.
		cert_realsign_ticket(&buf->ticket, &rvth_privkey_debug_ticket);
	}

	// Write the ticket.
	errno = 0;
	size = fwrite(&buf->ticket, 1, sizeof(buf->ticket), f_dest_wad);
	if (size != sizeof(buf->ticket)) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR writing destination WAD ticket: %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// 64-byte alignment.
	fpAlign(f_dest_wad);

	// Load the TMD.
	fseeko(f_src_wad, wadInfo.tmd_address, SEEK_SET);
	errno = 0;
	size = fread(buf->u8, 1, wadInfo.tmd_size, f_src_wad);
	if (size != wadInfo.tmd_size) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR reading source WAD TMD: %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// Change the issuer.
	// NOTE: MSVC Secure Overloads will change strncpy() to strncpy_s(),
	// which doesn't clear the buffer. Hence, we'll need to explicitly
	// clear the buffer first.
	memset(buf->tmdHeader.issuer, 0, sizeof(buf->tmdHeader.issuer));
	strncpy(buf->tmdHeader.issuer, issuer_TMD, sizeof(buf->tmdHeader.issuer));
	// Sign the TMD.
	// TODO: Error checking.
	if (likely(toKey != RVL_KEY_DEBUG)) {
		// Retail: Fakesign the TMD.
		// Dolphin and cIOSes ignore the signature anyway.
		cert_fakesign_tmd(buf->u8, wadInfo.tmd_size);
	} else {
		// Debug: Use the real signing keys.
		// Debug IOS requires a valid signature.
		cert_realsign_tmd(buf->u8, wadInfo.tmd_size, &rvth_privkey_debug_tmd);
	}

	// Write the TMD.
	errno = 0;
	size = fwrite(buf->u8, 1, wadInfo.tmd_size, f_dest_wad);
	if (size != wadInfo.tmd_size) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR writing destination WAD TMD: %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// 64-byte alignment.
	fpAlign(f_dest_wad);

	// Copy the data, one megabyte at a time.
	// TODO: Show progress? (WADs are small enough that this probably isn't needed...)
	// TODO: Check for fseeko() errors.
	printf("Copying the WAD data...\n");
	data_sz = wadInfo.data_size;
	fseeko(f_src_wad, wadInfo.data_address, SEEK_SET);
	for (; data_sz >= sizeof(buf->u8); data_sz -= sizeof(buf->u8)) {
		errno = 0;
		size = fread(buf->u8, 1, sizeof(buf->u8), f_src_wad);
		if (size != sizeof(buf->u8)) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR reading source WAD data: %s\n", strerror(err));
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
			fprintf(stderr, "*** ERROR writing destination WAD data: %s\n", strerror(err));
			ret = -err;
			goto end;
		}
	}

	// Remaining data.
	if (data_sz > 0) {
		errno = 0;
		size = fread(buf->u8, 1, data_sz, f_src_wad);
		if (size != data_sz) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR reading source WAD data: %s\n", strerror(err));
			ret = -err;
			goto end;
		}

		errno = 0;
		size = fwrite(buf->u8, 1, data_sz, f_dest_wad);
		if (size != data_sz) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR writing destination WAD data: %s\n", strerror(err));
			ret = -err;
			goto end;
		}
	}

	// Copy the footer/name.
	if (wadInfo.footer_size != 0) {
		if (likely(!isEarly)) {
			printf("Copying the WAD footer...\n");
		} else {
			printf("Converting the WAD name to a footer...\n");
		}

		fseeko(f_src_wad, wadInfo.footer_address, SEEK_SET);
		errno = 0;
		size = fread(buf->u8, 1, wadInfo.footer_size, f_src_wad);
		if (size != wadInfo.footer_size) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR reading source WAD %s: %s\n",
				s_footer_name, strerror(err));
			ret = -err;
			goto end;
		}

		// 64-byte alignment.
		fpAlign(f_dest_wad);

		size = fwrite(buf->u8, 1, wadInfo.footer_size, f_dest_wad);
		if (size != wadInfo.footer_size) {
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			fprintf(stderr, "*** ERROR writing destination WAD footer: %s\n", strerror(err));
			ret = -err;
			goto end;
		}
	}

	// Make sure the file is 64-byte aligned.
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
			fprintf(stderr, "*** ERROR writing destination WAD padding: %s\n", strerror(err));
			ret = -err;
			goto end;
		}
	}

	printf("WAD resigning complete.\n");
	ret = 0;

end:
	free(buf);
	if (f_dest_wad) {
		// TODO: Delete if an error occurred?
		fclose(f_dest_wad);
	}
	if (f_src_wad) {
		fclose(f_src_wad);
	}
	return ret;
}
