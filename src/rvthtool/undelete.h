/***************************************************************************
 * RVT-H Tool                                                              *
 * undelete.h: Delete or undelete a bank in an RVT-H disk image.           *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_RVTHTOOL_UNDELETE_H__
#define __RVTHTOOL_RVTHTOOL_UNDELETE_H__

#include "tcharx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 'delete' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @return 0 on success; non-zero on error.
 */
int delete_bank(const TCHAR *rvth_filename, const TCHAR *s_bank);

/**
 * 'undelete' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @return 0 on success; non-zero on error.
 */
int undelete_bank(const TCHAR *rvth_filename, const TCHAR *s_bank);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_RVTHTOOL_UNDELETE_H__ */
