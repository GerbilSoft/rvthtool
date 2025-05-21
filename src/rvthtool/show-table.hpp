/***************************************************************************
 * RVT-H Tool                                                              *
 * show-table.hpp: Print out the NHCD Bank Table for debugging an invalid  *
 * bank.                                                                   *
 *                                                                         *
 * Copyright (c) 2023-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_RVTHTOOL_SHOW_TABLE_H__
#define __RVTHTOOL_RVTHTOOL_SHOW_TABLE_H__

#include "tcharx.h"
#include "librvth/rvth.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'show-table' command.
 * @param rvth_filename RVT-H device or disk image filename.
 * @return 0 on success; non-zero on error.
 */
int show_table(const TCHAR *rvth_filename);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_RVTHTOOL_SHOW_TABLE_H__ */
