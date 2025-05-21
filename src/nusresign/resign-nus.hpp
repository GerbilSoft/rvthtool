/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * resign-wad.hpp: Re-sign a WAD file.                                     *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "tcharx.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'resign' command.
 * @param nus_dir	[in] NUS directory.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int resign_nus(const TCHAR *nus_dir, int recrypt_key);

#ifdef __cplusplus
}
#endif
