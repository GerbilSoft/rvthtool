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

#include <stdint.h>
#include <stdio.h>

typedef struct _RefFile {
	FILE *file;
	int ref_count;
} RefFile;

/**
 * Open a file as a reference-counted file.
 * @param filename Filename.
 * @param mode Mode.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_open(const char *filename, const char *mode);

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

#endif /* __RVTHTOOL_LIBRVTH_REF_FILE_H__ */
