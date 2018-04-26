/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader_ciso.c: CISO disc image reader class.                            *
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

#ifndef __RVTHTOOL_LIBRVTH_READER_CISO_H__
#define __RVTHTOOL_LIBRVTH_READER_CISO_H__

#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Is a given disc image supported by the CISO reader?
 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
 * @param size	[in] Size of sbuf. (should be 512 or larger)
 * @return True if supported; false if not.
 */
bool reader_ciso_is_supported(const uint8_t *sbuf, size_t size);

/**
 * Create a CISO reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 * @return Reader*, or NULL on error.
 */
Reader *reader_ciso_open(RefFile *file, uint32_t lba_start, uint32_t lba_len);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_READER_CISO_H__ */
