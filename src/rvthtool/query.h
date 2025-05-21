/***************************************************************************
 * RVT-H Tool                                                              *
 * query.h: Query available RVT-H Reader devices.                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'query' command.
 * @return 0 on success; non-zero on error.
 */
int query(void);

#ifdef __cplusplus
}
#endif
