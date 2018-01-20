/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * ref_file.h: Reference-counted FILE*.                                    *
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

#ifndef __RVTHTOOL_LIBRVTH_REF_FILE_H__
#define __RVTHTOOL_LIBRVTH_REF_FILE_H__

#include "common.h"
#include "tcharx.h"

#include <stdint.h>
#include <stdio.h>

typedef struct _RefFile {
	FILE *file;
	int ref_count;

	// Filename for reopening as writable.
	TCHAR *filename;
	bool is_writable;
} RefFile;

/**
 * Open a file as a reference-counted file.
 * The file is opened as a binary file in read-only mode.
 * @param filename Filename.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_open(const TCHAR *filename);

/**
 * Create a file as a reference-counted file.
 * The file is opened as a binary file in read/write mode.
 * NOTE: If the file exists, it will be truncated.
 * @param filename Filename.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_create(const TCHAR *filename);

/**
 * Close a reference-counted file.
 * Note that the file isn't actually closed until all references are removed.
 * @param f RefFile*.
 */
void ref_close(RefFile *f);

/**
 * Duplicate a reference-counted file.
 * This increments the reference count and returns the pointer.
 * @param f RefFile*.
 * @return f
 */
RefFile *ref_dup(RefFile *f);

/**
 * Reopen a file with write access.
 * @param f RefFile*.
 * @return 0 on success; negative POSIX error code on error.
 */
int ref_make_writable(RefFile *f);

/**
 * Check if a file is a device file.
 * @param f RefFile*.
 * @return True if this is a device file; false if it isn't.
 */
bool ref_is_device(RefFile *f);

/**
 * Try to make this file a sparse file.
 * @param f RefFile*.
 * @param size If not zero, try to set the file to this size.
 * @return 0 on success; negative POSIX error code on error.
 */
int ref_make_sparse(RefFile *f, int64_t size);

/** Convenience wrappers for stdio functions. **/

static inline size_t ref_read(void *ptr, size_t size, size_t nmemb, RefFile *stream)
{
	return fread(ptr, size, nmemb, stream->file);
}

static inline size_t ref_write(const void *ptr, size_t size, size_t nmemb, RefFile *stream)
{
	return fwrite(ptr, size, nmemb, stream->file);
}

static inline int ref_seeko(RefFile *stream, int64_t offset, int whence)
{
	return fseeko(stream->file, offset, whence);
}

static inline int64_t ref_tello(RefFile *stream)
{
	return ftello(stream->file);
}

/** Convenience wrappers for various RefFile fields. **/

static inline const TCHAR *ref_filename(const RefFile *f)
{
	return f->filename;
}

static inline bool ref_is_writable(const RefFile *f)
{
	return f->is_writable;
}

#endif /* __RVTHTOOL_LIBRVTH_REF_FILE_H__ */
