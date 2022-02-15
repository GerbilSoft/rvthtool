/***************************************************************************
 * RVT-H Tool                                                              *
 * query.h: Query available RVT-H Reader devices.                          *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_RVTHTOOL_QUERY_H__
#define __RVTHTOOL_RVTHTOOL_QUERY_H__

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

#endif /* __RVTHTOOL_RVTHTOOL_QUERY_H__ */
