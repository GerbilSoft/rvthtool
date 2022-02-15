/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_enums.h: RVT-H enums.                                              *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __LIBRVTH_RVTH_ENUMS_H__
#define __LIBRVTH_RVTH_ENUMS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8	/* Standard RVT-H HDD bank count. */

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
