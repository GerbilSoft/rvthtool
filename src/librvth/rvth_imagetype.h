/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_imagetype.h: Disc image type enumeration.                          *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_IMAGETYPE_H__
#define __RVTHTOOL_LIBRVTH_RVTH_IMAGETYPE_H__

// This file is separate in order to allow usage by the Reader
// classes without including the whole rvth.h file.

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_IMAGETYPE_H__ */
