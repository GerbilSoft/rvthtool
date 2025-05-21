/***************************************************************************
 * RVT-H Tool                                                              *
 * list-banks.h: List banks in an RVT-H disk image.                        *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "tcharx.h"
#include "librvth/rvth.hpp"

#ifdef __cplusplus
/**
 * Print information for the specified bank.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @return 0 on success; non-zero on error.
 */
int print_bank(const RvtH *rvth, unsigned int bank);
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'list-banks' command.
 * @param rvth_filename RVT-H device or disk image filename.
 * @return 0 on success; non-zero on error.
 */
int list_banks(const TCHAR *rvth_filename);

#ifdef __cplusplus
}
#endif
