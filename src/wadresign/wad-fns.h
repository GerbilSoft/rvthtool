/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * wad-fns.h: General WAD functions.                                       *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_WADRESIGN_WAD_FNS_H__
#define __RVTHTOOL_WADRESIGN_WAD_FNS_H__

#include <stdint.h>

#include "libwiicrypto/wii_wad.h"

#ifdef __cplusplus
extern "C" {
#endif

// Read buffer size.
#define READ_BUFFER_SIZE (1024*1024)

// Maximum ticket size supported by wadresign.
#define WAD_TICKET_SIZE_MAX 1024

// Maximum TMD size supported by wadresign.
#define WAD_TMD_SIZE_MAX (1024*1024)

// Maximum data size supported by wadresign.
#define WAD_DATA_SIZE_MAX (128*1024*1024)

// Maximum metadata size supported by wadresign.
#define WAD_META_SIZE_MAX (1024*1024)

/**
 * Struct of WAD section addresses and sizes.
 * Parsed from the WAD header.
 */
typedef struct _WAD_Info_t {
	uint32_t cert_chain_address;
	uint32_t cert_chain_size;
	uint32_t crl_address;
	uint32_t crl_size;
	uint32_t ticket_address;
	uint32_t ticket_size;
	uint32_t tmd_address;
	uint32_t tmd_size;
	uint32_t data_address;
	uint32_t data_size;	// If 0, assume "rest of file".
	uint32_t meta_address;
	uint32_t meta_size;
} WAD_Info_t;

/**
 * Get WAD info for a standard WAD file.
 * @param pWadHeader	[in] WAD header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo(const Wii_WAD_Header *pWadHeader, WAD_Info_t *pWadInfo);

/**
 * Get WAD info for a BroadOn WAD file.
 * @param pWadHeader	[in] BroadOn header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo_BWF(const Wii_WAD_Header_BWF *pWadHeader, WAD_Info_t *pWadInfo);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_WADRESIGN_PRINT_INFO_H__ */
