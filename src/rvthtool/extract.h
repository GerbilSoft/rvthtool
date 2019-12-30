/***************************************************************************
 * RVT-H Tool                                                              *
 * extract.h: Extract a bank from an RVT-H disk image.                     *
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

#ifndef __RVTHTOOL_RVTHTOOL_EXTRACT_H__
#define __RVTHTOOL_RVTHTOOL_EXTRACT_H__

#include "librvth/tcharx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'extract' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string). (If NULL, assumes bank 1.)
 * @param gcm_filename	Filename for the extracted GCM image.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
 * @return 0 on success; non-zero on error.
 */
int extract(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename, int recrypt_key, unsigned int flags);

/**
 * 'import' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @param gcm_filename	Filename of the GCM image to import.
 * @param ios_force	IOS version to force. (-1 to use the existing IOS)
 * @return 0 on success; non-zero on error.
 */
int import(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename, int ios_force);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_RVTHTOOL_EXTRACT_H__ */
