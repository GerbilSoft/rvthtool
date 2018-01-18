/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * ref_file.c: Reference-counted FILE*.                                    *
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

#include "ref_file.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Open a file as a reference-counted file.
 * The file is opened as a binary file in read-only mode.
 * @param filename Filename.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_open(const TCHAR *filename)
{
	// TODO: Convert UTF-8 to UTF-16 on Windows?
	RefFile *f = malloc(sizeof(RefFile));
	if (!f) {
		// Unable to allocate memory.
		return NULL;
	}

	// Save the filename.
	f->filename = _tcsdup(filename);
	if (!f->filename) {
		// Could not copy the filename.
		int err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		free(f);
		errno = err;
		return NULL;
	}

	f->file = _tfopen(filename, _T("rb"));
	if (!f->file) {
		// Could not open the file.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		free(f->filename);
		free(f);
		errno = err;
		return NULL;
	}

	// Initialize the reference count.
	f->ref_count = 1;
	return f;
}

/**
 * Close a reference-counted file.
 * Note that the file isn't actually closed until all references are removed.
 * @param f RefFile*.
 */
void ref_close(RefFile *f)
{
	// TODO: Atomic decrement?
	assert(f->ref_count > 0);
	if (--f->ref_count <= 0) {
		// Close the file.
		fclose(f->file);
		free(f->filename);
		free(f);
	}
}

/**
 * Duplicate a reference-counted file.
 * This increments the reference count and returns the pointer.
 * @param f RefFile*.
 * @return f
 */
RefFile *ref_dup(RefFile *f)
{
	// TODO: Atomic increment?
	f->ref_count++;
	return f;
}

/**
 * Reopen a file with write access.
 * @param f RefFile*.
 * @return 0 on success; negative POSIX error code on error.
 */
int ref_make_writable(RefFile *f)
{
	int64_t pos;
	int ret = 0;

	if (f->is_writable) {
		// File is already writable.
		return 0;
	}

	// Get the current position.
	pos = ftello(f->file);

	// Close and reopen the file as writable.
	// TODO: Potential race condition...
	fclose(f->file);
	f->file = _tfopen(f->filename, _T("rb+"));
	if (f->file) {
		// File reopened as writable.
		f->is_writable = true;
	} else {
		// Could not reopen as writable.
		ret = -errno;
		// Reopen as read-only.
		f->file = _tfopen(f->filename, _T("rb"));
		assert(f->file != NULL);
		// FIXME: If it's NULL, something's wrong...
	}

	// Seek to the original position.
	// TODO: Check for errors.
	fseeko(f->file, pos, SEEK_SET);
	return ret;
}
