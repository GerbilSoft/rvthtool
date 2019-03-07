/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * wad-fns.h: General WAD functions.                                       *
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

#ifndef __RVTHTOOL_WADRESIGN_WAD_FNS_H__
#define __RVTHTOOL_WADRESIGN_WAD_FNS_H__

#include <stdint.h>

#include "libwiicrypto/wii_wad.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Struct of WAD section addresses and sizes.
 * Parsed from the WAD header.
 */
typedef struct _WAD_Info_t {
	uint32_t cert_chain_address;
	uint32_t cert_chain_size;
	uint32_t ticket_address;
	uint32_t ticket_size;
	uint32_t tmd_address;
	uint32_t tmd_size;
	uint32_t data_address;
	uint32_t data_size;	// If 0, assume "rest of file".

	// Standard WADs have an optional footer.
	// Early devkit WADs have an optional name,
	// usually located before the data.
	uint32_t footer_address;
	uint32_t footer_size;
} WAD_Info_t;

/**
 * Get WAD info for a standard WAD file.
 * @param pWadHeader	[in] WAD header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo(const Wii_WAD_Header *pWadHeader, WAD_Info_t *pWadInfo);

/**
 * Get WAD info for an early WAD file.
 * @param pWadHeader	[in] Early WAD header.
 * @param pWadInfo	[out] WAD info struct.
 * @return 0 on success; non-zero on error;
 */
int getWadInfo_early(const Wii_WAD_Header_EARLY *pWadHeader, WAD_Info_t *pWadInfo);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_WADRESIGN_PRINT_INFO_H__ */
