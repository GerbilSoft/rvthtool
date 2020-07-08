/***************************************************************************
 * RVT-H Tool: NUS Resigner                                                *
 * print-info.hpp: Print NUS information.                                  *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_NUSRESIGN_PRINT_INFO_HPP__
#define __RVTHTOOL_NUSRESIGN_PRINT_INFO_HPP__

#include <stdint.h>
#include <stdio.h>

#include "tcharx.h"

// TODO: Custom stdbool.x instead of libwiicrypto/common.h.
#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'info' command.
 * @param nus_dir	[in] NUS directory.
 * @param verify	[in] If true, verify the contents. [TODO]
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_nus_info(const TCHAR *nus_dir, bool verify);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_NUSRESIGN_PRINT_INFO_HPP__ */
