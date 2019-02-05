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

#include "libwiicrypto/common.h"
#include "RefFile.hpp"
#include "rvth_imagetype.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// LBA size.
#define LBA_SIZE 512

// Convert LBA values to bytes.
#define LBA_TO_BYTES(x) ((int64_t)(x) * LBA_SIZE)
// Convert bytes to an LBA value.
// NOTE: Partial LBAs will be truncated!
#define BYTES_TO_LBA(x) ((uint32_t)((x) / LBA_SIZE))

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
	RvtH_ImageType_e type;		// Disc image type.
} Reader;

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
Reader *reader_open(RefFile *file, uint32_t lba_start, uint32_t lba_len);

/** Wrapper functions for the vtable. **/

/**
 * Read data from a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static inline uint32_t reader_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len)
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
static inline uint32_t reader_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len)
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
static inline void reader_close(Reader *reader)
{
	reader->vtbl->close(reader);
}

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_READER_H__ */
