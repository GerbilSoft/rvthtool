/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader.h: Disc image reader base class.                                 *
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

#include "reader.h"
#include "reader_plain.h"

/**
 * Create a Reader object for a disc image.
 *
 * If the disc image is using a supported compressed/compacted
 * format, the appropriate reader will be chosen. Otherwise,
 * the plain reader will be used.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 * @return Reader*, or NULL on error.
 */
Reader *reader_open(RefFile *file, uint32_t lba_start, uint32_t lba_len)
{
	// TODO: Add support for other formats.
	return reader_plain_open(file, lba_start, lba_len);
}
