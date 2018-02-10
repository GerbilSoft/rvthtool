/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader_plain.h: Plain disc image reader base class.                     *
 * Used for plain binary disc images, e.g. .gcm and RVT-H images.          *
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

#include "reader_plain.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#ifndef LBA_TO_BYTES
# define LBA_TO_BYTES(x) ((int64_t)(x) * LBA_SIZE)
#endif

// Functions.
static uint32_t reader_plain_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len);
static uint32_t reader_plain_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len);
static void reader_plain_flush(Reader *reader);
static void reader_plain_close(Reader *reader);

// vtable
static const Reader_Vtbl reader_plain_vtable = {
	reader_plain_read,
	reader_plain_write,
	reader_plain_flush,
	reader_plain_close,
};

// We're not using an internal struct for reader_plain,
// since we don't need to maintain any internal state
// other than what's provided by RefFile*.

/**
 * Create a plain reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 * @return Reader*, or NULL on error.
 */
Reader *reader_plain_open(RefFile *file, uint32_t lba_start, uint32_t lba_len)
{
	Reader *reader;
	int err = 0;

	// Validate parameters.
	if (!file || (lba_start > 0 && lba_len == 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return NULL;
	}

	// Allocate memory for the Reader object.
	reader = malloc(sizeof(*reader));
	if (!reader) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		return NULL;
	}

	// Duplicate the file.
	reader->file = ref_dup(file);
	if (!reader->file) {
		// Error duplicating the file.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto fail;
	}

	// Set the vtable.
	reader->vtbl = &reader_plain_vtable;

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// Determine the maximum LBA.
		int64_t offset;
		int err = ref_seeko(file, 0, SEEK_END);
		if (err != 0) {
			// Seek error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			goto fail;
		}
		offset = ref_tello(file);

		// NOTE: If not a multiple of the LBA size,
		// the partial LBA will be ignored.
		lba_len = (uint32_t)(offset / LBA_SIZE);
	}
	reader->lba_start = lba_start;
	reader->lba_len = lba_len;

	// Reader initialized.
	return reader;

fail:
	// Failed to initialize the reader.
	if (reader->file) {
		ref_close(reader->file);
	}
	free(reader);
	errno = err;
	return NULL;
}

/**
 * Read data from a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static uint32_t reader_plain_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	int ret;

	// LBA bounds checking.
	// TODO: Check for overflow?
	lba_start += reader->lba_start;
	assert(lba_start + lba_len <= reader->lba_start + reader->lba_len);
	if (lba_start + lba_len > reader->lba_start + reader->lba_len) {
		// Out of range.
		errno = EIO;
		return 0;
	}

	// Seek to lba_start.
	ret = ref_seeko(reader->file, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return 0;
	}

	// Read the data.
	return (uint32_t)ref_read(ptr, LBA_SIZE, lba_len, reader->file);
}

/**
 * Write data to a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[in] Write buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static uint32_t reader_plain_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	int ret;

	// LBA bounds checking.
	// TODO: Check for overflow?
	lba_start += reader->lba_start;
	assert(lba_start + lba_len <= reader->lba_start + reader->lba_len);
	if (lba_start + lba_len > reader->lba_start + reader->lba_len) {
		// Out of range.
		errno = EIO;
		return 0;
	}

	// Seek to lba_start.
	ret = ref_seeko(reader->file, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return 0;
	}

	// Write the data.
	return (uint32_t)ref_write(ptr, LBA_SIZE, lba_len, reader->file);
}

/**
 * Flush the file buffers.
 * @param reader	[in] Reader*
 * */
static void reader_plain_flush(Reader *reader)
{
	ref_flush(reader->file);
}

/**
 * Close a disc image.
 * @param reader	[in] Reader*
 */
static void reader_plain_close(Reader *reader)
{
	ref_close(reader->file);
	free(reader);
}
