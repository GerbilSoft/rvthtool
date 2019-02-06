/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_enums.h: RVT-H enums.                                              *
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

#ifndef __LIBRVTH_RVTH_ENUMS_H__
#define __LIBRVTH_RVTH_ENUMS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8	/* Standard RVT-H HDD bank count. */
#define RVTH_BLOCK_SIZE 512

// RVT-H bank types.
typedef enum {
	RVTH_BankType_Empty		= 0,	// Magic is 0.
	RVTH_BankType_Unknown		= 1,	// Unknown magic.
	RVTH_BankType_GCN		= 2,
	RVTH_BankType_Wii_SL		= 3,
	RVTH_BankType_Wii_DL		= 4,
	RVTH_BankType_Wii_DL_Bank2	= 5,	// Bank 2 for DL images.

	RVTH_BankType_MAX
} RvtH_BankType_e;

// Encryption types.
typedef enum {
	RVTH_CryptoType_Unknown	= 0,	// Unknown encryption.
	RVTH_CryptoType_None	= 1,	// No encryption.
	RVTH_CryptoType_Debug	= 2,	// RVT-R encryption.
	RVTH_CryptoType_Retail	= 3,	// Retail encryption.
	RVTH_CryptoType_Korean	= 4,	// Korean retail encryption.

	RVTH_CryptoType_MAX
} RvtH_CryptoType_e;

// Signature types.
typedef enum {
	RVTH_SigType_Unknown	= 0,	// Unknown signature.
	RVTH_SigType_Debug	= 1,	// Debug signature.
	RVTH_SigType_Retail	= 2,	// Retail signature.

	RVTH_SigType_MAX
} RvtH_SigType_e;

// Signature status.
typedef enum {
	RVTH_SigStatus_Unknown	= 0,	// Unknown signature status.
	RVTH_SigStatus_OK	= 1,	// Valid
	RVTH_SigStatus_Invalid	= 2,	// Invalid
	RVTH_SigStatus_Fake	= 3,	// Fakesigned

	RVTH_SigStatus_MAX
} RvtH_SigStatus_e;

// RVT-H image type.
typedef enum {
	RVTH_ImageType_Unknown = 0,

	// HDDs (multiple banks)
	RVTH_ImageType_HDD_Reader,	// RVT-H Reader connected via USB
	RVTH_ImageType_HDD_Image,	// RVT-H Reader disk image

	// GCMs (single banks)
	RVTH_ImageType_GCM,		// Standalone disc image
	RVTH_ImageType_GCM_SDK,		// Standalone disc image with SDK header
	// TODO: CISO/WBFS? (Handling as GCM for now.)

	RVTH_ImageType_MAX
} RvtH_ImageType_e;

// RVT-H extraction flags.
typedef enum {
	// Prepend a 32 KB SDK header.
	// Required for rvtwriter, NDEV ODEM, etc.
	RVTH_EXTRACT_PREPEND_SDK_HEADER		= (1 << 0),
} RvtH_Extract_Flags;

#ifdef __cplusplus
}
#endif

#endif /* __LIBRVTH_RVTH_ENUMS_H__ */
