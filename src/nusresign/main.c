/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * main.c: Main program file.                                              *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "libwiicrypto/common.h"
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/priv_key_store.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wiiu_structs.h"

// C includes.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char *argv[])
{
	FILE *f_tik;
	FILE *f_tmd;
	uint8_t *tik_data;
	size_t tik_size;
	uint8_t *tmd_data;
	size_t tmd_size;

	// Convenience pointers.
	WUP_Ticket *pTicket;		// points at tik_data
	WUP_TMD_Header *pTmdHeader;	// points at tmd_data

	// Temporary values for ticket fixes.
	uint32_t feedface = 0;

	if (argc < 3) {
		fprintf(stderr, "NUS Resigner - converts retail to debug only right now...\n");
		fprintf(stderr, "Syntax: %s title.tik title.tmd\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Open the ticket and TMD.
	f_tik = fopen(argv[1], "rb+");
	f_tmd = fopen(argv[2], "rb+");
	if (!f_tik || !f_tmd) {
		fprintf(stderr, "*** ERROR opening ticket or TMD, try again!\n");
		return EXIT_FAILURE;
	}

	// Get the ticket and TMD sizes.
	fseeko(f_tik, 0, SEEK_END);
	tik_size = ftello(f_tik);
	rewind(f_tik);
	fseeko(f_tmd, 0, SEEK_END);
	tmd_size = ftello(f_tmd);
	rewind(f_tmd);

	// Read the ticket and TMD.
	tik_data = malloc(tik_size);
	fread(tik_data, 1, tik_size, f_tik);
	pTicket = (WUP_Ticket*)tik_data;
	tmd_data = malloc(tmd_size);
	fread(tmd_data, 1, tmd_size, f_tmd);
	pTmdHeader = (WUP_TMD_Header*)tmd_data;

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
			free(tik_data);
			free(tmd_data);
			fclose(f_tik);
			fclose(f_tmd);
			return EXIT_FAILURE;
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
			free(tik_data);
			free(tmd_data);
			fclose(f_tik);
			fclose(f_tmd);
			return EXIT_FAILURE;
	}

	// Some pirated games have "FEEDFACE" in the ECDH section.
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
	sig_recrypt_ticket_WUP(pTicket, WUP_KEY_DEBUG);

	// Update the extra certs in the ticket and TMD, if present.
	// TODO: PKI selection.
	updateExtraCerts(tik_data, tik_size, false, WUP_PKI_DPKI);
	updateExtraCerts(tmd_data, tmd_size, true, WUP_PKI_DPKI);

	// Re-sign the ticket.
	// TODO: Detect the original issuer.
	// NOTE: Does NOT include appended certificates.
	strncpy(pTicket->issuer, "Root-CA00000004-XS0000000f", sizeof(pTicket->issuer));
	cert_realsign_ticketOrTMD(tik_data, sizeof(*pTicket), &rvth_privkey_WUP_dpki_ticket);

	// Re-sign the TMD.
	// TODO: Detect the original issuer.
	strncpy(pTmdHeader->rvl.issuer, "Root-CA00000004-CP00000010", sizeof(pTmdHeader->rvl.issuer));
	// NOTE: Main hash only covers the TMD header.
	cert_realsign_ticketOrTMD(tmd_data, sizeof(*pTmdHeader), &rvth_privkey_WUP_dpki_tmd);

	// FIXME: Some TMDs have a cert chain at the end with the
	// CA and CP certs. Need to replace them with Debug versions.

	// Write the new ticket and TMD.
	rewind(f_tik);
	rewind(f_tmd);
	fwrite(tik_data, 1, tik_size, f_tik);
	fwrite(tmd_data, 1, tmd_size, f_tmd);
	fclose(f_tik);
	fclose(f_tmd);

	free(tik_data);
	free(tmd_data);
	printf("Re-encrypted retail Wii U NUS title as debug!\n");
}
