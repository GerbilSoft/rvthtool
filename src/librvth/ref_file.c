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

/**
 * Open a file as a reference-counted file.
 * @param filename Filename.
 * @param mode Mode.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_open(const char *filename, const char *mode)
{
	// TODO: Convert UTF-8 to UTF-16 on Windows?
	RefFile *f = malloc(sizeof(RefFile));
	if (!f) {
		// Unable to allocate memory.
		return NULL;
	}

	f->file = fopen(filename, mode);
	if (!f->file) {
		int err = errno;
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
