/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader.cpp: Disc image reader base class.                               *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                  *
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
#include "reader_ciso.h"
#include "reader_wbfs.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <string.h>

// SDK header values.
// TODO: Verify GC1L, NN2L. (These are all NN1L images.)
// TODO: Checksum at 0x0830.
static const uint8_t sdk_0x0000[4] = {0xFF,0xFF,0x00,0x00};
static const uint8_t sdk_0x082C[4] = {0x00,0x00,0xE0,0x06};

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
	uint8_t sbuf[4096];
	int ret;
	size_t size;

	assert(file != NULL);
	if (file->isDevice()) {
		// This is an RVT-H Reader system.
		// Only plain images are supported.
		// TODO: Also check for RVT-H Reader HDD images?
		return reader_plain_open(file, lba_start, lba_len);
	}

	// This is a disc image file.
	// NOTE: Ignoring errors here because the file may be empty
	// if we're opening a file for writing.

	// Check for other disc image formats.
	ret = file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return NULL;
	}
	errno = 0;
	size = file->read(sbuf, 1, sizeof(sbuf));
	if (size != sizeof(sbuf)) {
		// Short read. May be empty.
		if (errno != 0) {
			// Actual error.
			return NULL;
		} else {
			// Assume it's a new file.
			// Use the plain disc image reader.
			file->rewind();
			return reader_plain_open(file, lba_start, lba_len);
		}
	}
	file->rewind();

	// Check the magic number.
	if (reader_ciso_is_supported(sbuf, sizeof(sbuf))) {
		// This is a supported CISO image.
		return reader_ciso_open(file, lba_start, lba_len);
	} else if (reader_wbfs_is_supported(sbuf, sizeof(sbuf))) {
		// This is a supported WBFS image.
		return reader_wbfs_open(file, lba_start, lba_len);
	}

	// Check for SDK headers.
	// TODO: Verify GC1L, NN2L. (These are all NN1L images.)
	// TODO: Checksum at 0x0830.
	if (lba_len > BYTES_TO_LBA(32768) &&
	    !memcmp(&sbuf[0x0000], sdk_0x0000, sizeof(sdk_0x0000)) &&
	    !memcmp(&sbuf[0x082C], sdk_0x082C, sizeof(sdk_0x082C)) &&
	    sbuf[0x0844] == 0x01)
	{
		// This image has an SDK header.
		// Adjust the LBA values to skip it.
		lba_start += BYTES_TO_LBA(32768);
		lba_len -= BYTES_TO_LBA(32768);
	}

	// Use the plain disc image reader.
	return reader_plain_open(file, lba_start, lba_len);
}
