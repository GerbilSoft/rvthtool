/***************************************************************************
 * RVT-H Tool                                                              *
 * list-banks.h: List banks in an RVT-H disk image.                        *
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

#ifndef __RVTHTOOL_RVTHTOOL_LIST_BANKS_H__
#define __RVTHTOOL_RVTHTOOL_LIST_BANKS_H__

#include "librvth/tcharx.h"
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

#endif /* __RVTHTOOL_RVTHTOOL_LIST_BANKS_H__ */
