/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_time.h: RVT-H timestamp functions.                                 *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_TIME_H__
#define __RVTHTOOL_LIBRVTH_RVTH_TIME_H__

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse an RVT-H timestamp.
 * @param nhcd_timestamp Pointer to NHCD timestamp string. (Must be 14 characters; NOT NULL-terminated.)
 * @return Timestamp, or -1 if invalid.
 */
time_t rvth_timestamp_parse(const char *nhcd_timestamp);

/**
 * Create an RVT-H timestamp.
 * @param buf	[out] Pointer to NHCD timestamp buffer. (Must be 14 characters; will NOT be NULL-terminated.)
 * @param size	[in] Size of nhcd_timestamp.
 * @param now	[in] Time to set.
 * @return 0 on success; negative POSIX error code on error.
 */
int rvth_timestamp_create(char *buf, size_t size, time_t now);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_TIME_H__ */
