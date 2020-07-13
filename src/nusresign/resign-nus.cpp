/***************************************************************************
 * RVT-H Tool: NUS Resigner                                                *
 * resign-nus.cpp: Re-sign an NUS directory. (Wii U)                       *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "resign-nus.hpp"
#include "print-info.hpp"

#include "libwiicrypto/common.h"
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/priv_key_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wiiu_structs.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ includes.
#include <memory>
#include <string>
using std::tstring;
using std::unique_ptr;

/**
 * Update the certs at the end of the ticket or TMD, if present.
 * If the certs don't match the expected PKI, they will be replaced.
 * @param data	[in/out] Ticket or TMD data.
 * @param size	[in] Size of ticket or TMD data.
 * @param isTMD	[in] True for TMD; false for ticket.
 * @param pki	[in] New PKI to use: WUP_PKI_DPKI or WUP_PKI_PPKI.
 */
static void updateExtraCerts(uint8_t *data, size_t size, bool isTMD, RVL_PKI pki)
{
	RVL_Cert_RSA4096_RSA2048 *ca_cert;	// New CA cert
	RVL_Cert_RSA2048 *other_cert;		// New CP/XS cert
	size_t cert_pos;

	// New issuers to use.
	RVL_Cert_Issuer issuer_ca, issuer_other;

	if (size < (sizeof(RVL_Cert_RSA4096_RSA2048) + sizeof(RVL_Cert_RSA2048))) {
		// Ticket or TMD is too small. It can't have the certs.
		return;
	}

	switch (pki) {
		case WUP_PKI_DPKI:
			issuer_ca = WUP_CERT_ISSUER_DPKI_CA;
			issuer_other = (isTMD ? WUP_CERT_ISSUER_DPKI_TMD : WUP_CERT_ISSUER_DPKI_TICKET);
			break;
		case WUP_PKI_PPKI:
			issuer_ca = WUP_CERT_ISSUER_PPKI_CA;
			issuer_other = (isTMD ? WUP_CERT_ISSUER_PPKI_TMD : WUP_CERT_ISSUER_PPKI_TICKET);
			break;
		default:
			assert(!"Invalid PKI for Wii U.");
			return;
	}

	// Certificates is stored after the TMD header, content info table,
	// and contents table.
	// Ordering: CP/XS, CA

	if (isTMD) {
		// TMD header and content info table are fixed sizes.
		// Determine the size of the content table.
		unsigned int contentCount = 0;
		const WUP_TMD_ContentInfoTable *contentInfoTable =
			(const WUP_TMD_ContentInfoTable*)&data[sizeof(WUP_TMD_Header)];

		cert_pos = sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable);
		for (unsigned int i = 0; i < ARRAY_SIZE(contentInfoTable->info); i++) {
			contentCount += be16_to_cpu(contentInfoTable->info[i].commandCount);
		}
		cert_pos += (contentCount * sizeof(WUP_Content_Entry));
	} else {
		// Ticket is a fixed size.
		cert_pos = sizeof(WUP_Ticket);
	}

	if (cert_pos > (size - (sizeof(RVL_Cert_RSA4096_RSA2048) + sizeof(RVL_Cert_RSA2048)))) {
		// Out of bounds...
		// TODO: Report an error.
		return;
	}

	other_cert = (RVL_Cert_RSA2048*)&data[cert_pos];
	ca_cert = (RVL_Cert_RSA4096_RSA2048*)&data[cert_pos + sizeof(*other_cert)];

	// CA cert: Check if we need to change the issuer.
	if (ca_cert->sig.type == cpu_to_be32(WUP_CERT_SIGTYPE_RSA4096_SHA256) &&
	    strncmp(ca_cert->sig.issuer, RVL_Cert_Issuers[issuer_ca], sizeof(ca_cert->sig.issuer)) != 0)
	{
		// CA cert is not the correct PKI. Update it.
		const RVL_Cert_RSA4096_RSA2048 *const new_ca_cert =
			(const RVL_Cert_RSA4096_RSA2048*)cert_get(issuer_ca);
		assert(new_ca_cert != nullptr);
		assert(new_ca_cert->sig.type == ca_cert->sig.type);
		memcpy(ca_cert, new_ca_cert, sizeof(*ca_cert));
	}

	// CP/CX cert: Check if we need to change the issuer.
	if (other_cert->sig.type == cpu_to_be32(WUP_CERT_SIGTYPE_RSA2048_SHA256) &&
	    strncmp(other_cert->sig.issuer, RVL_Cert_Issuers[issuer_other], sizeof(other_cert->sig.issuer)) != 0)
	{
		// CP cert is not the correct PKI. Update it.
		const RVL_Cert_RSA2048 *const new_other_cert =
			(const RVL_Cert_RSA2048*)cert_get(issuer_other);
		assert(new_other_cert != nullptr);
		assert(new_other_cert->sig.type == other_cert->sig.type);
		memcpy(other_cert, new_other_cert, sizeof(*other_cert));
	}
}

/**
 * 'resign' command.
 * @param nus_dir	[in] NUS directory.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int resign_nus(const TCHAR *nus_dir, int recrypt_key)
{
	// Print the NUS information.
	// TODO: Should we verify the hashes?
	int ret = print_nus_info(nus_dir, false);
	if (ret != 0) {
		// Error printing the NUS information.
		return ret;
	}

	// Construct the filenames.
	tstring sf_tik = nus_dir;
	sf_tik += DIR_SEP_CHR;
	sf_tik += _T("title.tik");

	tstring sf_tmd = nus_dir;
	sf_tmd += DIR_SEP_CHR;
	sf_tmd += _T("title.tmd");

	// Open the ticket and TMD.
	FILE *f_tik = _tfopen(sf_tik.c_str(), _T("rb+"));
	if (!f_tik) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR opening ticket file: %s\n", strerror(err));
		return -err;
	}

	FILE *f_tmd = _tfopen(sf_tmd.c_str(), _T("rb+"));
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
	} else if (tik_size > (64*1024)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading ticket file: Too big.\n");
		return -EIO;
	}
	fseeko(f_tmd, 0, SEEK_END);
	const size_t tmd_size = ftello(f_tmd);
	if (tmd_size < (sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable))) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading TMD file: Too small.\n");
		return -EIO;
	} else if (tmd_size > (128*1024)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading TMD file: Too big.\n");
		return -EIO;
	}
	rewind(f_tik);
	rewind(f_tmd);

	// Read the ticket and TMD.
	// TODO: Check for errors?
	unique_ptr<uint8_t[]> tik_data(new uint8_t[tik_size]);
	fread(tik_data.get(), 1, tik_size, f_tik);
	WUP_Ticket *const pTicket = reinterpret_cast<WUP_Ticket*>(tik_data.get());

	unique_ptr<uint8_t[]> tmd_data(new uint8_t[tmd_size]);
	fread(tmd_data.get(), 1, tmd_size, f_tmd);
	WUP_TMD_Header *const pTmdHeader = reinterpret_cast<WUP_TMD_Header*>(tmd_data.get());

	// Check the encryption key.
	// NOTE: Not checking the TMD key. Assuming it's the same as Ticket.
	// NOTE: Need to check for 3DS
	RVL_CryptoType_e src_key;		// TODO: WUP constants?
	const char *s_fromKey;
	switch (cert_get_issuer_from_name(pTicket->issuer)) {
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TICKET:
			src_key = RVL_CryptoType_Retail;
			s_fromKey = "retail";
			break;
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TICKET:
			src_key = RVL_CryptoType_Debug;
			s_fromKey = "debug";
			break;
		default:
			fputs("*** ERROR: NUS ticket has an unknown issuer.\n", stderr);
			printf("issuer: %s == %d\n", pTicket->issuer, cert_get_issuer_from_name(pTicket->issuer));
			return 1;
	}

	// Check the recryption key.
	if (recrypt_key == -1) {
		// "Default". Use the "other" key.
		recrypt_key = (src_key == RVL_CryptoType_Retail)
			? RVL_CryptoType_Debug
			: RVL_CryptoType_Retail;
	} else {
		// If the specified key matches the current key, fail.
		if (recrypt_key == src_key) {
			fputs("*** ERROR: Cannot recrypt to the same key.\n", stderr);
			return 2;
		}
	}

	// Determine the new issuers.
	RVL_Cert_Issuer issuer_ca, issuer_xs, issuer_cp, issuer_sp;
	const char *s_issuer_xs, *s_issuer_cp;
	const char *s_toKey;
	RVL_AES_Keys_e toKey;
	RVL_PKI toPki;
	switch (recrypt_key) {
		case RVL_CryptoType_Retail:
			issuer_ca = WUP_CERT_ISSUER_PPKI_CA;
			issuer_xs = WUP_CERT_ISSUER_PPKI_TICKET;
			issuer_cp = WUP_CERT_ISSUER_PPKI_TMD;
			issuer_sp = RVL_CERT_ISSUER_UNKNOWN;
			s_toKey = "retail";
			toKey = WUP_KEY_RETAIL;
			toPki = WUP_PKI_PPKI;
			break;
		case RVL_CryptoType_Debug:
			issuer_ca = WUP_CERT_ISSUER_DPKI_CA;
			issuer_xs = WUP_CERT_ISSUER_DPKI_TICKET;
			issuer_cp = WUP_CERT_ISSUER_DPKI_TMD;
			issuer_sp = WUP_CERT_ISSUER_DPKI_SP;
			s_toKey = "debug";
			toKey = WUP_KEY_DEBUG;
			toPki = WUP_PKI_DPKI;
			break;
		default:
			// Shouldn't get here...
			assert(!"Invalid crypto type...");
			return 3;
	}
	s_issuer_xs = RVL_Cert_Issuers[issuer_xs];
	s_issuer_cp = RVL_Cert_Issuers[issuer_cp];

	printf("Converting NUS from %s to %s...\n", s_fromKey, s_toKey);

	/** Ticket fixups **/

	// Signature type for both ticket and TMD must be 0x10004.
	// NOTE: If it's 0x30004, change it for disc compatibility.
	switch (be32_to_cpu(pTicket->signature_type)) {
		case 0x10004:
			break;
		case 0x30004:
			fprintf(stderr, "*** Changing ticket signature type from Disc to Installable.\n");
			pTicket->signature_type = cpu_to_be32(0x10004);
			break;
		default:
			fprintf(stderr, "*** ERROR: Ticket has unsupported signature type: 0x%08X\n",
				be32_to_cpu(pTicket->signature_type));
			fclose(f_tik);
			fclose(f_tmd);
			return 4;
	}
	switch (be32_to_cpu(pTmdHeader->rvl.signature_type)) {
		case 0x10004:
			break;
		case 0x30004:
			fprintf(stderr, "*** Changing TMD signature type from Disc to Installable.\n");
			pTmdHeader->rvl.signature_type = cpu_to_be32(0x10004);
			break;
		default:
			fprintf(stderr, "*** ERROR: TMD has unsupported signature type: 0x%08X\n",
				be32_to_cpu(pTmdHeader->rvl.signature_type));
			fclose(f_tik);
			fclose(f_tmd);
			return 5;
	}

	// Some pirated games have "FEEDFACE" in the ECDH section.
	uint32_t feedface = 0;
	memcpy(&feedface, pTicket->ecdh_data, sizeof(feedface));
	if (feedface == cpu_to_be32(0xFEEDFACE)) {
		// Clear the ECDH section.
		memset(pTicket->ecdh_data, 0, sizeof(pTicket->ecdh_data));
	}

	// Some copies of games have broken ticket and CRL versions.
	pTicket->version = 1;
	pTicket->ca_crl_version = 0;
	pTicket->signer_crl_version = 0;

	// Ticket ID may be blank.
	// If it is, copy the title ID.
	/* (FIXME: Is this needed?)
	if (pTicket->ticket_id == 0) {
		pTicket->ticket_id = pTicket->title_id.id;
	}
	*/

	// Re-encrypt the ticket.
	sig_recrypt_ticket_WUP(pTicket, toKey);

	// Update the extra certs in the ticket and TMD, if present.
	// TODO: PKI selection.
	updateExtraCerts(tik_data.get(), tik_size, false, toPki);
	updateExtraCerts(tmd_data.get(), tmd_size, true, toPki);

	// Set the new issuers.
	strncpy(pTicket->issuer, s_issuer_xs, sizeof(pTicket->issuer));
	strncpy(pTmdHeader->rvl.issuer, s_issuer_cp, sizeof(pTmdHeader->rvl.issuer));

	// Re-sign the ticket and TMD.
	// NOTE: TMD signature only covers the TMD header.
	if (toPki == WUP_PKI_DPKI) {
		// dpki: Use the real private keys.
		cert_realsign_ticketOrTMD(tik_data.get(), sizeof(*pTicket), &rvth_privkey_WUP_dpki_ticket);
		cert_realsign_ticketOrTMD(tmd_data.get(), sizeof(*pTmdHeader), &rvth_privkey_WUP_dpki_tmd);
	} else /*if (toPki == WUP_PKI_PPKI)*/ {
		// ppki: Fill the signature area with 0xD15EA5ED.
		// Also fill the ticket's ECDH area with 0xFEEDFACE.
		// This matches FunKiiU.
		uint32_t *p = reinterpret_cast<uint32_t*>(pTicket->signature);
		for (unsigned int i = sizeof(pTicket->signature)/sizeof(uint32_t)/4; i > 0; i--, p += 4) {
			p[0] = 0xD15EA5ED;
			p[1] = 0xD15EA5ED;
			p[2] = 0xD15EA5ED;
			p[3] = 0xD15EA5ED;
		}
		p = reinterpret_cast<uint32_t*>(pTmdHeader->rvl.signature);
		for (unsigned int i = sizeof(pTmdHeader->rvl.signature)/sizeof(uint32_t)/4; i > 0; i--, p += 4) {
			p[0] = 0xD15EA5ED;
			p[1] = 0xD15EA5ED;
			p[2] = 0xD15EA5ED;
			p[3] = 0xD15EA5ED;
		}
		p = reinterpret_cast<uint32_t*>(pTicket->ecdh_data);
		for (unsigned int i = sizeof(pTicket->ecdh_data)/sizeof(uint32_t); i > 0; i--, p++) {
			*p = 0xFEEDFACE;
		}
	}

	// Write the new ticket and TMD.
	rewind(f_tik);
	rewind(f_tmd);
	fwrite(tik_data.get(), 1, tik_size, f_tik);
	fwrite(tmd_data.get(), 1, tmd_size, f_tmd);
	fclose(f_tik);
	fclose(f_tmd);

	// Write title.cert.
	tstring sf_cert = nus_dir;
	sf_cert += DIR_SEP_CHR;
	sf_cert += _T("title.cert");
	FILE *f_cert = _tfopen(sf_cert.c_str(), _T("wb"));
	if (!f_cert) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR: Unable to write title.cert: %s\n", strerror(err));
		return -err;
	}

	// Certificate order: CA, CP, XS, SP (dev only)
	const RVL_Cert *cert_data = cert_get(issuer_ca);
	size_t cert_size = cert_get_size(issuer_ca);
	fwrite(cert_data, 1, cert_size, f_cert);

	cert_data = cert_get(issuer_cp);
	cert_size = cert_get_size(issuer_cp);
	fwrite(cert_data, 1, cert_size, f_cert);

	cert_data = cert_get(issuer_xs);
	cert_size = cert_get_size(issuer_xs);
	fwrite(cert_data, 1, cert_size, f_cert);

	if (issuer_sp != RVL_CERT_ISSUER_UNKNOWN) {
		cert_data = cert_get(issuer_sp);
		cert_size = cert_get_size(issuer_sp);
		fwrite(cert_data, 1, cert_size, f_cert);
	}

	fclose(f_cert);

	printf("NUS resigning complete.\n");
	return 0;
}
