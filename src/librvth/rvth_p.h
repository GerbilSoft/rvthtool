/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_p.h: RVT-H image handler. (PRIVATE HEADER)                         *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_P_H__
#define __RVTHTOOL_LIBRVTH_RVTH_P_H__

#include "common.h"

#include "rvth.h"
#include "ref_file.h"

// RVT-H main struct.
struct _RvtH {
	// Reference-counted FILE*.
	RefFile *f_img;

	// Number of banks.
	// - RVT-H system or disk image: 8
	// - Standalone disc image: 1
	unsigned int bank_count;

	// Is this an HDD image?
	bool is_hdd;

	// BankEntry objects.
	RvtH_BankEntry *entries;
};

#endif /* __RVTHTOOL_LIBRVTH_RVTH_P_H__ */
