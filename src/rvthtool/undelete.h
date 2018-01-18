/***************************************************************************
 * RVT-H Tool                                                              *
 * undelete.h: Delete or undelete a bank in an RVT-H disk image.           *
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

#ifndef __RVTHTOOL_RVTHTOOL_UNDELETE_H__
#define __RVTHTOOL_RVTHTOOL_UNDELETE_H__

#include "librvth/tcharx.h"

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
