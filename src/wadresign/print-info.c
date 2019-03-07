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

#include "print-info.h"
#include "wad-fns.h"

// libwiicrypto
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/wii_wad.h"
#include "libwiicrypto/sig_tools.h"

// C includes.
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

/**
 * Is an issuer retail or debug?
 * @param issuer RVL_Cert_Issuer
 * @return "Retail", "Debug", or "Unknown".
 */
const char *issuer_type(RVL_Cert_Issuer issuer)
{
	switch (issuer) {
		default:
		case RVL_CERT_ISSUER_UNKNOWN:
			return "Unknown";

		case RVL_CERT_ISSUER_ROOT:
			// TODO: Separate roots for Debug and Retail?
			return "Root";

		case RVL_CERT_ISSUER_DEBUG_CA:
		case RVL_CERT_ISSUER_DEBUG_TICKET:
		case RVL_CERT_ISSUER_DEBUG_TMD:
		case RVL_CERT_ISSUER_DEBUG_DEV:
			return "Debug";

		case RVL_CERT_ISSUER_RETAIL_CA:
		case RVL_CERT_ISSUER_RETAIL_TICKET:
		case RVL_CERT_ISSUER_RETAIL_TMD:
			return "Retail";
	}
}

/**
 * Identify a WAD file's type.
 *
 * This is mostly for informational purposes, except for early devkit WAD files,
 * in which case the format is slightly different.
 *
 * @param pBuf		[in] Header data.
 * @param buf_len	[in] Length of buf. (Should be at least 64.)
 * @param pIsEarly [out] `bool` to store if the WAD file is an early devkit WAD file or not.
 * @return WAD file type as a string, or NULL on error.
 */
const char *identify_wad_type(const uint8_t *buf, size_t buf_len, bool *pIsEarly)
{
	const char *s_wad_type = NULL;
	const WAD_Header *const header = (const WAD_Header*)buf;
	if (buf_len < sizeof(Wii_WAD_Header)) {
		// Not enough data...
		return NULL;
	}

	// Identify the WAD type.
	// TODO: More extensive error handling?

	// Check if this WAD is valid.
	if (header->wad.header_size != cpu_to_be32(0x0020)) {
		// Wrong header size.
		return NULL;
	}

	*pIsEarly = false;
	if (header->wad.type == cpu_to_be32(WII_WAD_TYPE_Is)) {
		s_wad_type = "Is";
	} else if (header->wad.type == cpu_to_be32(WII_WAD_TYPE_ib)) {
		s_wad_type = "ib";
	} else if (header->wad.type == cpu_to_be32(WII_WAD_TYPE_Bk)) {
		s_wad_type = "Bk";
	} else {
		// This might be an early WAD.
		if (header->wadE.ticket_size == cpu_to_be32(sizeof(RVL_Ticket))) {
			// Ticket size is correct.
			// This is probably an early WAD.
			s_wad_type = "Early Devkit";
			*pIsEarly = true;
		}
	}

	return s_wad_type;
}

/**
 * Verify a content entry.
 * @param f_wad		[in] Opened WAD file.
 * @param encKey	[in] Encryption key.
 * @param content_addr	[in] Content address.
 * @param content	[in] Content entry.
 * @return 0 if the content is verified; non-zero if not.
 */
static int verify_content(FILE *f_wad, RVL_AES_Keys_e encKey,
	uint32_t content_addr, const RVL_Content_Entry *content)
{
	// TODO
	return 0;
}

/**
 * 'info' command. (internal function)
 * @param f_wad		[in] Opened WAD file.
 * @param wad_filename	[in] WAD filename. (for error messages)
 * @param verify	[in] If true, verify the contents.
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_wad_info_FILE(FILE *f_wad, const TCHAR *wad_filename, bool verify)
{
	int ret;
	size_t size;
	const char *s_wad_type = NULL;
	bool isEarly = false;
	WAD_Header header;
	WAD_Info_t wadInfo;

	RVL_Ticket ticket;
	uint8_t *tmd = NULL;
	const RVL_TMD_Header *tmdHeader = NULL;
	uint16_t title_version;
	uint8_t ios_version = 0;

	// Certificate validation.
	RVL_Cert_Issuer issuer_ticket;
	const char *s_issuer_ticket, *s_issuer_tmd;
	RVL_SigStatus_e sig_status_ticket, sig_status_tmd;

	// Encryption key.
	RVL_AES_Keys_e encKey;
	const char *s_encKey;
	const char *s_invalidKey = NULL;

	// Contents.
	unsigned int nbr_cont, nbr_cont_actual;
	uint16_t boot_index;
	const RVL_Content_Entry *content;
	uint32_t content_addr;

	// Read the WAD header.
	rewind(f_wad);
	size = fread(&header, 1, sizeof(header), f_wad);
	if (size != sizeof(header)) {
		int err = errno;
		fputs("*** ERROR reading WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "': %s\n", strerror(err));
		ret = -err;
		goto end;
	}

	// Identify the WAD type.
	// TODO: More extensive error handling?
	s_wad_type = identify_wad_type((const uint8_t*)&header, sizeof(header), &isEarly);
	if (!s_wad_type) {
		// Unrecognized WAD type.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "' is not valid.\n");
		ret = 1;
		goto end;
	}

	// Determine the sizes and addresses of various components.
	if (!isEarly) {
		ret = getWadInfo(&header.wad, &wadInfo);
	} else {
		ret = getWadInfo_early(&header.wadE, &wadInfo);
	}
	if (ret != 0) {
		// Unable to get WAD information.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "' is not valid.");
		ret = 2;
		goto end;
	}

	// Verify the ticket and TMD sizes.
	if (wadInfo.ticket_size != sizeof(RVL_Ticket)) {
		// Incorrect ticket size.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "' ticket size is incorrect. (%u; should be %u)\n",
			wadInfo.ticket_size, (uint32_t)sizeof(RVL_Ticket));
		ret = 3;
		goto end;
	} else if (wadInfo.tmd_size < sizeof(RVL_TMD_Header)) {
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "' TMD size is too small. (%u; should be at least %u)\n",
			wadInfo.tmd_size, (uint32_t)sizeof(RVL_TMD_Header));
		ret = 4;
		goto end;
	} else if (wadInfo.tmd_size > 1*1024*1024) {
		// Too big.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "' TMD size is too big. (%u; should be less than 1 MB)\n",
			wadInfo.tmd_size);
		ret = 5;
		goto end;
	}

	// Load the ticket and TMD.
	fseeko(f_wad, wadInfo.ticket_address, SEEK_SET);
	size = fread(&ticket, 1, sizeof(ticket), f_wad);
	if (size != sizeof(ticket)) {
		// Read error.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fputs("': Unable to read the ticket.\n", stderr);
		ret = 6;
		goto end;
	}
	tmd = malloc(wadInfo.tmd_size);
	if (!tmd) {
		fprintf(stderr, "*** ERROR: Unable to allocate %u bytes for the TMD.\n", wadInfo.tmd_size);
		ret = 7;
		goto end;
	}
	fseeko(f_wad, wadInfo.tmd_address, SEEK_SET);
	size = fread(tmd, 1, wadInfo.tmd_size, f_wad);
	if (size != wadInfo.tmd_size) {
		// Read error.
		fputs("*** ERROR: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fputs("': Unable to read the TMD.\n", stderr);
		ret = 8;
		goto end;
	}
	tmdHeader = (const RVL_TMD_Header*)tmd;

	// NOTE: Using TMD for most information.
	printf("Type: %s\n", s_wad_type);
	printf("- Title ID:      %08X-%08X\n", be32_to_cpu(tmdHeader->title_id.hi), be32_to_cpu(tmdHeader->title_id.lo));

	// Game ID, but only if all characters are alphanumeric.
	if (ISALNUM(tmdHeader->title_id.u8[4]) &&
	    ISALNUM(tmdHeader->title_id.u8[5]) &&
	    ISALNUM(tmdHeader->title_id.u8[6]) &&
	    ISALNUM(tmdHeader->title_id.u8[7]))
	{
		printf("- Game ID:       %.4s\n",
			(const char*)&tmdHeader->title_id.u8[4]);
	}

	// Title version
	title_version = be16_to_cpu(tmdHeader->title_version);
	printf("- Title version: %u.%u (v%u)\n",
		title_version >> 8, title_version & 0xFF, title_version);

	// IOS version
	// TODO: Error message if not an IOS?
	if (be32_to_cpu(tmdHeader->sys_version.hi) == 1) {
		uint32_t ios_tid_lo = be32_to_cpu(tmdHeader->sys_version.lo);
		if (ios_tid_lo < 256) {
			ios_version = (uint8_t)ios_tid_lo;
		}
	}
	printf("- IOS version:   %u\n", ios_version);

	// Determine the encryption key in use.
	issuer_ticket = cert_get_issuer_from_name(ticket.issuer);
	switch (issuer_ticket) {
		default:	// TODO: Show an error instead?
		case RVL_CERT_ISSUER_RETAIL_TICKET:
			// Retail may be either Common Key or Korean Key.
			switch (ticket.common_key_index) {
				case 0:
					encKey = RVL_KEY_RETAIL;
					s_encKey = "Retail";
					break;
				case 1:
					encKey = RVL_KEY_KOREAN;
					s_encKey = "Korean";
					break;
				default: {
					// NOTE: A good number of retail WADs have an
					// incorrect common key index for some reason.
					if (ticket.title_id.u8[7] == 'K') {
						s_invalidKey = "Korean";
						s_encKey = "Korean";
						encKey = RVL_KEY_KOREAN;
					} else {
						s_invalidKey = "retail";
						s_encKey = "Retail";
						encKey = RVL_KEY_RETAIL;
					}
					break;
				}
			}
			break;
		case RVL_CERT_ISSUER_DEBUG_TICKET:
			encKey = RVL_KEY_DEBUG;
			s_encKey = "Debug";
			break;
	}
	printf("- Encryption:    %s\n", s_encKey);

	// Check the ticket issuer and signature.
	s_issuer_ticket = issuer_type(issuer_ticket);
	sig_status_ticket = sig_verify((const uint8_t*)&ticket, sizeof(ticket));
	printf("- Ticket Signature: %s%s\n",
		s_issuer_ticket, RVL_SigStatus_toString_stsAppend(sig_status_ticket));

	// Check the ticket issuer and signature.
	s_issuer_tmd = issuer_type(cert_get_issuer_from_name(tmdHeader->issuer));
	sig_status_tmd = sig_verify(tmd, wadInfo.tmd_size);
	printf("- TMD Signature:    %s%s\n",
		s_issuer_tmd, RVL_SigStatus_toString_stsAppend(sig_status_tmd));

	putchar('\n');

	if (s_invalidKey) {
		// Invalid common key index for retail.
		// NOTE: A good number of retail WADs have an
		// incorrect common key index for some reason.
		fputs("*** WARNING: WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "': Invalid common key index %u.\n",
			ticket.common_key_index);
		fprintf(stderr, "*** Assuming %s common key based on game ID.\n\n", s_invalidKey);
	}

	// Print the contents.
	fputs("Contents:\n", stderr);
	nbr_cont = be16_to_cpu(tmdHeader->nbr_cont);
	boot_index = be16_to_cpu(tmdHeader->boot_index);

	// Make sure the TMD is big enough.
	// TODO: Show an error if it's not?
	content = (const RVL_Content_Entry*)(&tmd[sizeof(*tmdHeader)]);
	nbr_cont_actual = (wadInfo.tmd_size - sizeof(*tmdHeader)) / sizeof(*content);
	if (nbr_cont > nbr_cont_actual) {
		nbr_cont = nbr_cont_actual;
	}

	// TODO: Validate against data_size.
	content_addr = wadInfo.data_address;
	for (; nbr_cont > 0; nbr_cont--, content++) {
		// TODO: Show the actual table index, or just the
		// index field in the entry?
		uint16_t content_index = be16_to_cpu(content->index);
		printf("#%d: ID=%08x, type=%04X, size=%u",
			be16_to_cpu(content->index),
			be32_to_cpu(content->content_id),
			be16_to_cpu(content->type),
			(uint32_t)be64_to_cpu(content->size));
		if (content_index == boot_index) {
			fputs(", bootable", stdout);
		}
		putchar('\n');

		if (verify) {
			// Verify the content.
			// TODO: Only decrypt the title key once?
			verify_content(f_wad, encKey, content_addr, content);
		}

		// Next content.
		content_addr += (uint32_t)be64_to_cpu(content->size);
		if (likely(!isEarly)) {
			content_addr = ALIGN(64, content_addr);
		}
	}

	putchar('\n');
	ret = 0;

end:
	free(tmd);
	return ret;
}

/**
 * 'info' command.
 * @param wad_filename	[in] WAD filename.
 * @param verify	[in] If true, verify the contents.
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_wad_info(const TCHAR *wad_filename, bool verify)
{
	int ret;

	// Open the WAD file.
	FILE *f_wad = _tfopen(wad_filename, _T("rb"));
	if (!f_wad) {
		int err = errno;
		fputs("*** ERROR opening WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "': %s\n", strerror(err));
		return -err;
	}

	// Print the WAD info.
	ret = print_wad_info_FILE(f_wad, wad_filename, verify);
	fclose(f_wad);
	return ret;
}
