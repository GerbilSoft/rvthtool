/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * resign-wad.hpp: Re-sign a WAD file.                                     *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_NUSRESIGN_RESIGN_NUS_HPP__
#define __RVTHTOOL_NUSRESIGN_RESIGN_NUS_HPP__

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

#endif /* __RVTHTOOL_NUSRESIGN_RESIGN_NUS_HPP__ */
