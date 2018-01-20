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

#ifndef __RVTHTOOL_LIBRVTH_READER_H__
#define __RVTHTOOL_LIBRVTH_READER_H__

#include "common.h"
#include "ref_file.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Consolidate the block size values.
#define LBA_SIZE 512

struct _Reader;

/**
 * vtable for disc image readers.
 */
typedef struct _Reader_Vtbl {
	// TODO: lba_start param, or separate seek()?
	// Regardless, we have to seek on each read because another
	// disc image may have altered the file pointer.

	/**
	 * Read data from a disc image.
	 * @param reader	[in] Reader*
	 * @param ptr		[out] Read buffer.
	 * @param lba_start	[in] Starting LBA.
	 * @param lba_len	[in] Length, in LBAs.
	 * @return Number of LBAs read, or 0 on error.
	 */
	uint32_t (*read)(struct _Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len);

	/**
	 * Write data to a disc image.
	 * @param reader	[in] Reader*
	 * @param ptr		[in] Write buffer.
	 * @param lba_start	[in] Starting LBA.
	 * @param lba_len	[in] Length, in LBAs.
	 * @return Number of LBAs read, or 0 on error.
	 */
	uint32_t (*write)(struct _Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len);

	/**
	 * Flush the file buffers.
	 * @param reader	[in] Reader*
	 * */
	void (*flush)(struct _Reader *reader);

	/**
	 * Close a disc image.
	 * @param reader	[in] Reader*
	 */
	void (*close)(struct _Reader *reader);
} Reader_Vtbl;

/**
 * Disc image reader base class.
 * TODO: Make this opaque?
 */
typedef struct _Reader {
	const Reader_Vtbl *vtbl;	// vtable
	RefFile *file;			// Disc image file.
	uint32_t lba_start;		// Starting LBA.
	uint32_t lba_len;		// Length of image, in LBAs.
} Reader;

/** Wrapper functions for the vtable. **/

/**
 * Read data from a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static inline uint32_t reader_read(struct _Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	return reader->vtbl->read(reader, ptr, lba_start, lba_len);
}

/**
 * Write data to a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[in] Write buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static inline uint32_t reader_write(struct _Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	return reader->vtbl->write(reader, ptr, lba_start, lba_len);
}

/**
 * Flush the file buffers.
 * @param reader	[in] Reader*
 * */
static inline void reader_flush(Reader *reader)
{
	reader->vtbl->flush(reader);
}

/**
 * Close a disc image.
 * @param reader	[in] Reader*
 */
static inline void reader_close(struct _Reader *reader)
{
	reader->vtbl->close(reader);
}

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_READER_H__ */
