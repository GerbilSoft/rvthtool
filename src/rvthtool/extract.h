/***************************************************************************
 * RVT-H Tool                                                              *
 * extract.h: Extract a bank from an RVT-H disk image.                     *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "tcharx.h"

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
