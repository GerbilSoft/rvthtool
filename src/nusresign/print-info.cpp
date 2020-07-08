/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * print-info.c: Print WAD information.                                    *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "print-info.hpp"

// libwiicrypto
#include "libwiicrypto/aesw.h"
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wiiu_structs.h"

// Nettle
#include <nettle/sha1.h>
#include <nettle/sha2.h>

// C includes.
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ includes.
#include <memory>
#include <string>
using std::tstring;
using std::unique_ptr;

/**
 * Is an issuer retail or debug?
 * @param issuer RVL_Cert_Issuer
 * @return "Retail", "Debug", or "Unknown".
 */
static const char *issuer_type(RVL_Cert_Issuer issuer)
{
	switch (issuer) {
		default:
		case RVL_CERT_ISSUER_UNKNOWN:
			return "Unknown";

		// NOTE: CTR certs are needed here because
		// CTR and WUP share certificates.
		case RVL_CERT_ISSUER_DPKI_ROOT:
		case CTR_CERT_ISSUER_DPKI_CA:
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case CTR_CERT_ISSUER_DPKI_TMD:
		case WUP_CERT_ISSUER_DPKI_CA:
		case WUP_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TMD:
		case WUP_CERT_ISSUER_DPKI_SP:
			return "Debug";

		case RVL_CERT_ISSUER_PPKI_ROOT:
		case CTR_CERT_ISSUER_PPKI_CA:
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case CTR_CERT_ISSUER_PPKI_TMD:
		case WUP_CERT_ISSUER_PPKI_CA:
		case WUP_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TMD:
			return "Retail";
	}
}

/**
 * 'info' command.
 * @param nus_dir	[in] NUS directory.
 * @param verify	[in] If true, verify the contents. [TODO]
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_nus_info(const TCHAR *nus_dir, bool verify)
{
	int ret;
	size_t size;

	// Construct the filenames.
	tstring sf_tik = nus_dir;
	sf_tik += DIR_SEP_CHR;
	sf_tik += "title.tik";

	tstring sf_tmd = nus_dir;
	sf_tmd += DIR_SEP_CHR;
	sf_tmd += "title.tmd";

	// Open the ticket and TMD.
	FILE *f_tik = _tfopen(sf_tik.c_str(), "rb");
	if (!f_tik) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR opening ticket file: %s\n", strerror(err));
		return -err;
	}

	FILE *f_tmd = _tfopen(sf_tmd.c_str(), "rb+");
	if (!f_tmd) {
		fclose(f_tik);

		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR opening TMD file: %s\n", strerror(err));
		return -err;
	}

	// Get the ticket and TMD sizes.
	fseeko(f_tik, 0, SEEK_END);
	const size_t tik_size = ftello(f_tik);
	if (tik_size < sizeof(WUP_Ticket)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading ticket file: Too small.\n");
		return -EIO;
	}
	fseeko(f_tmd, 0, SEEK_END);
	const size_t tmd_size = ftello(f_tmd);
	if (tmd_size < (sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable))) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading TMD file: Too small.\n");
		return -EIO;
	}
	rewind(f_tik);
	rewind(f_tmd);

	// Read the ticket and TMD.
	// TODO: Check for errors?
	unique_ptr<uint8_t[]> tik_data(new uint8_t[tik_size]);
	fread(tik_data.get(), 1, tik_size, f_tik);
	fclose(f_tik);
	WUP_Ticket *const pTicket = reinterpret_cast<WUP_Ticket*>(tik_data.get());

	unique_ptr<uint8_t[]> tmd_data(new uint8_t[tmd_size]);
	fread(tmd_data.get(), 1, tmd_size, f_tmd);
	fclose(f_tmd);
	WUP_TMD_Header *const pTmdHeader = reinterpret_cast<WUP_TMD_Header*>(tmd_data.get());

	// NOTE: Using TMD for most information.
	_tprintf(_T("%s:\n"), nus_dir);
	// FIXME: Get the actual Wii U application type.
	//printf("Type: %08X\n", be32_to_cpu(pTmdHeader->rvl.title_type));
	printf("- Title ID:      %08X-%08X\n", be32_to_cpu(pTmdHeader->rvl.title_id.hi), be32_to_cpu(pTmdHeader->rvl.title_id.lo));

	// TODO: Figure out where the game ID is stored.

	// Title version (TODO: Wii U changes?)
	uint16_t title_version = be16_to_cpu(pTmdHeader->rvl.title_version);
	printf("- Title version: %u.%u (v%u)\n",
		title_version >> 8, title_version & 0xFF, title_version);

	// OS version
	// TODO: Error message if not an OS title?
	uint8_t os_version = 0;
	if (be32_to_cpu(pTmdHeader->rvl.sys_version.hi) == 0x00050010) {
		const uint32_t ios_tid_lo = be32_to_cpu(pTmdHeader->rvl.sys_version.lo);
		const uint32_t tid24 = (ios_tid_lo >> 8);
		if (tid24 == 0x100040 || tid24 == 0x100080) {
			os_version = (uint8_t)ios_tid_lo;
		}
	}
	printf("- OS version:    OSv%u\n", os_version);

	// Determine the encryption key in use.
	// NOTE: May use CTR since CTR and WUP have the same certificates.
	RVL_Cert_Issuer issuer_ticket = cert_get_issuer_from_name(pTicket->issuer);
	RVL_AES_Keys_e encKey;
	const char *s_encKey;
	switch (issuer_ticket) {
		default:	// TODO: Show an error instead?
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TICKET:
			encKey = WUP_KEY_RETAIL;
			s_encKey = "Retail";
			break;
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TICKET:
			encKey = WUP_KEY_DEBUG;
			s_encKey = "Debug";
			break;
	}
	printf("- Encryption:    %s\n", s_encKey);

	// Check the ticket issuer and signature.
	const char *const s_issuer_ticket = issuer_type(issuer_ticket);
	RVL_SigStatus_e sig_status_ticket = sig_verify(tik_data.get(), tik_size);
	printf("- Ticket Signature: %s%s\n",
		s_issuer_ticket, RVL_SigStatus_toString_stsAppend(sig_status_ticket));

	// Check the TMD issuer and signature.
	// NOTE: TMD signature only covers the TMD header.
	// We'll need to check the content info hash afterwards.
	const char *const s_issuer_tmd = issuer_type(cert_get_issuer_from_name(pTmdHeader->rvl.issuer));
	RVL_SigStatus_e sig_status_tmd = sig_verify(tmd_data.get(), sizeof(WUP_TMD_Header));
	if (sig_status_tmd == RVL_SigStatus_OK) {
		// Verify the content info hash.
		const WUP_TMD_ContentInfoTable *const cinfotbl =
			reinterpret_cast<const WUP_TMD_ContentInfoTable*>(&tmd_data[sizeof(WUP_TMD_Header)]);
		struct sha256_ctx sha256;
		uint8_t digest[SHA256_DIGEST_SIZE];
		sha256_init(&sha256);
		sha256_update(&sha256, sizeof(*cinfotbl), reinterpret_cast<const uint8_t*>(cinfotbl));
		sha256_digest(&sha256, sizeof(digest), digest);

		if (!memcmp(digest, pTmdHeader->contentInfo_sha256, sizeof(digest))) {
			// Check the hashes of each content info section.
			const WUP_ContentInfo *cinfo = cinfotbl->info;
			const WUP_ContentInfo *const cinfo_end = &cinfo[WUP_CONTENTINFO_ENTRIES];

			size_t cstart = sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable);
			for (; cinfo < cinfo_end; cinfo++) {
				// Check the number of commands and hash that many entries.
				const unsigned int indexOffset = be16_to_cpu(cinfo->indexOffset);
				const unsigned int commandCount = be16_to_cpu(cinfo->commandCount);
				if (indexOffset == 0 && commandCount == 0) {
					// End of table.
					break;
				}

				const size_t pos = cstart + (indexOffset * sizeof(WUP_Content_Entry));
				const size_t len = (commandCount * sizeof(WUP_Content_Entry));
				if (pos + len > tmd_size) {
					// Out of bounds.
					sig_status_tmd = RVL_SigStatus_Invalid;
					break;
				}

				// Hash the contents.
				sha256_init(&sha256);
				sha256_update(&sha256, len, &tmd_data[pos]);
				sha256_digest(&sha256, sizeof(digest), digest);
				if (memcmp(digest, cinfo->sha256, sizeof(digest)) != 0) {
					// Content hashes are invalid.
					// TODO: Show a specific error?
					sig_status_tmd = RVL_SigStatus_Invalid;
					break;
				}
			}
		} else {
			// Content info table hash is invalid.
			// TODO: Show a specific error?
			sig_status_tmd = RVL_SigStatus_Invalid;
		}
	}

	printf("- TMD Signature:    %s%s\n",
		s_issuer_tmd, RVL_SigStatus_toString_stsAppend(sig_status_tmd));

	putchar('\n');

	return 0;
}
