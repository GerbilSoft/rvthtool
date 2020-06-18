/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * wad-fns.h: General WAD functions.                                       *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "wad-fns.h"

#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/common.h"

/**
 * Get WAD info for a standard WAD file.
 * @param pWadHeader	[in] WAD header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo(const Wii_WAD_Header *pWadHeader, WAD_Info_t *pWadInfo)
{
	// Make sure this is in fact a standard WAD.
	if (pWadHeader->header_size != cpu_to_be32(0x0020)) {
		// Wrong header size.
		return 1;
	}

	if (pWadHeader->type != cpu_to_be32(WII_WAD_TYPE_Is) &&
	    pWadHeader->type != cpu_to_be32(WII_WAD_TYPE_ib) &&
	    pWadHeader->type != cpu_to_be32(WII_WAD_TYPE_Bk))
	{
		// Unsupported type.
		return 2;
	}

	// Standard WAD files have 64-byte aligned sections,
	// starting with the certificate chain.

	pWadInfo->cert_chain_address = ALIGN_BYTES(64, be32_to_cpu(pWadHeader->header_size));
	pWadInfo->cert_chain_size = be32_to_cpu(pWadHeader->cert_chain_size);

	pWadInfo->ticket_address = ALIGN_BYTES(64, pWadInfo->cert_chain_address + pWadInfo->cert_chain_size);
	pWadInfo->ticket_size = be32_to_cpu(pWadHeader->ticket_size);

	pWadInfo->tmd_address = ALIGN_BYTES(64, pWadInfo->ticket_address + pWadInfo->ticket_size);
	pWadInfo->tmd_size = be32_to_cpu(pWadHeader->tmd_size);

	pWadInfo->data_address = ALIGN_BYTES(64, pWadInfo->tmd_address + pWadInfo->tmd_size);
	pWadInfo->data_size = be32_to_cpu(pWadHeader->data_size);

	if (pWadInfo->footer_size != 0) {
		pWadInfo->footer_address = ALIGN_BYTES(64, pWadInfo->data_address + pWadInfo->data_size);
		pWadInfo->footer_size = be32_to_cpu(pWadHeader->footer_size);
	} else {
		// No footer.
		pWadInfo->footer_address = 0;
		pWadInfo->footer_size = 0;
	}
	return 0;
}

/**
 * Get WAD info for a BroadOn WAD file.
 * @param pWadHeader	[in] BroadOn WAD header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo_BWF(const Wii_WAD_Header_BWF *pWadHeader, WAD_Info_t *pWadInfo)
{
	// Make sure this is in fact a BroadOn WAD.
	if (pWadHeader->header_size != cpu_to_be32(0x0020)) {
		// Wrong header size.
		return 1;
	}

	if (pWadHeader->ticket_size != cpu_to_be32(sizeof(RVL_Ticket))) {
		// Wrong ticket size.
		return 2;
	}

	// BroadOn WAD files have NO alignment.

	pWadInfo->cert_chain_address = be32_to_cpu(pWadHeader->header_size);
	pWadInfo->cert_chain_size = be32_to_cpu(pWadHeader->cert_chain_size);

	pWadInfo->ticket_address = pWadInfo->cert_chain_address + pWadInfo->cert_chain_size;
	pWadInfo->ticket_size = be32_to_cpu(pWadHeader->ticket_size);

	pWadInfo->tmd_address = pWadInfo->ticket_address + pWadInfo->ticket_size;
	pWadInfo->tmd_size = be32_to_cpu(pWadHeader->tmd_size);

	// Optional name field. (using the "footer" fields in WAD info)
	if (pWadHeader->name_size != 0) {
		pWadInfo->footer_address = pWadInfo->tmd_address + pWadInfo->tmd_size;
		pWadInfo->footer_size = be32_to_cpu(pWadHeader->name_size);
	} else {
		// No footer.
		pWadInfo->footer_address = 0;
		pWadInfo->footer_size = 0;
	}

	// Data offset is explicitly specified.
	// Size is assumed to be the rest of the file.
	pWadInfo->data_address = be32_to_cpu(pWadHeader->data_offset);
	pWadInfo->data_size = 0;
	return 0;
}
