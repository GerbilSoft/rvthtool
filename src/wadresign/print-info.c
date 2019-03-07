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
 * 'info' command.
 * @param wad_filename WAD filename.
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_wad_info(const TCHAR *wad_filename)
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
	const char *issuer_ticket, *issuer_tmd;
	RVL_SigStatus_e sig_status_ticket, sig_status_tmd;

	// Open the WAD file.
	FILE *f_wad = _tfopen(wad_filename, _T("rb"));
	if (!f_wad) {
		int err = errno;
		fputs("*** ERROR opening WAD file '", stderr);
		_fputts(wad_filename, stderr);
		fprintf(stderr, "': %s\n", strerror(err));
		return -err;
	}

	// Read the WAD header.
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
	fseek(f_wad, wadInfo.ticket_address, SEEK_SET);
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
	fseek(f_wad, wadInfo.tmd_address, SEEK_SET);
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

	// Check the ticket issuer and signature.
	issuer_ticket = issuer_type(cert_get_issuer_from_name(ticket.issuer));
	sig_status_ticket = sig_verify((const uint8_t*)&ticket, sizeof(ticket));
	printf("- Ticket Signature: %s%s\n",
		issuer_ticket, RVL_SigStatus_toString_stsAppend(sig_status_ticket));

	// Check the ticket issuer and signature.
	issuer_tmd = issuer_type(cert_get_issuer_from_name(tmdHeader->issuer));
	sig_status_tmd = sig_verify(tmd, wadInfo.tmd_size);
	printf("- TMD Signature:    %s%s\n",
		issuer_tmd, RVL_SigStatus_toString_stsAppend(sig_status_tmd));

	putchar('\n');
end:
	free(tmd);
	fclose(f_wad);
	return 0;
}
