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

#include "config.librvth.h"

#include "ref_file.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
# include <io.h>
# include <winioctl.h>
#else /* !_WIN32 */
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
#endif /* !_WIN32 */

// NOTE: Some functions don't check for NULL, since they should
// loudly crash if NULL is passed.

/**
 * Open a file as a reference-counted file.
 * The file is opened as a binary file using the specified mode.
 * @param filename Filename.
 * @param mode Mode.
 * @return RefFile*, or NULL if an error occurred.
 */
static RefFile *ref_open_int(const TCHAR *filename, const TCHAR *mode)
{
	int err = 0;

	RefFile *f = calloc(1, sizeof(RefFile));
	if (!f) {
		// Unable to allocate memory.
		return NULL;
	}

	// Save the filename.
	f->filename = _tcsdup(filename);
	if (!f->filename) {
		// Could not copy the filename.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto fail;
	}

	f->file = _tfopen(filename, mode);
	if (!f->file) {
		// Could not open the file.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// Initialize the reference count.
	f->ref_count = 1;
        // File writability depends on the mode.
	// NOTE: Only checking the first character.
        f->is_writable = (mode[0] == _T('w'));
	return f;

fail:
	// Failed to open the file.
	if (f->file) {
		fclose(f->file);
	}
	if (f->filename) {
		free(f->filename);
	}
	free(f);
	if (err != 0) {
		errno = err;
	}
	return NULL;
}

/**
 * Open a file as a reference-counted file.
 * The file is opened as a binary file in read-only mode.
 * @param filename Filename.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_open(const TCHAR *filename)
{
	return ref_open_int(filename, _T("rb"));
}

/**
 * Create a file as a reference-counted file.
 * The file is opened as a binary file in read/write mode.
 * NOTE: If the file exists, it will be truncated.
 * @param filename Filename.
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile *ref_create(const TCHAR *filename)
{
	return ref_open_int(filename, _T("wb+"));
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

/**
 * Check if a file is a device file.
 * @param f RefFile*.
 * @return True if this is a device file; false if it isn't.
 */
bool ref_is_device(RefFile *f)
{
#ifdef _WIN32
	// Windows: Check the beginning of the filename.
	if (!f->filename) {
		// No filename...
		return false;
	}

	return !_tcsnicmp(f->filename, _T("\\\\.\\PhysicalDrive"), 17);
#else /* !_WIN32 */
	// Other: Use fstat().
	struct stat buf;
	int ret = fstat(fileno(f->file), &buf);
	if (ret != 0) {
		// fstat() failed.
		return false;
	}

	// NOTE: FreeBSD dropped "block" devices, so we need to
	// check for both block and character devices.
	return !!(S_ISBLK(buf.st_mode) | S_ISCHR(buf.st_mode));
#endif
}

/**
 * Try to make this file a sparse file.
 * @param f RefFile*.
 * @param size If not zero, try to set the file to this size.
 * @return 0 on success; negative POSIX error code on error.
 */
int ref_make_sparse(RefFile *f, int64_t size)
{
#ifdef _WIN32
	wchar_t root_dir[4];		// Root directory.
	wchar_t *p_root_dir;		// Pointer to root_dir, or NULL if relative.
	DWORD dwFileSystemFlags;	// Flags from GetVolumeInformation().
	BOOL bRet;

	// Check if the file system supports sparse files.
	// TODO: Handle mount points?
	if (_istalpha(f->filename[0]) && f->filename[1] == _T(':') &&
	    (f->filename[2] == _T('\\') || f->filename[2] == _T('/')))
	{
		// Absolute pathname.
		root_dir[0] = f->filename[0];
		root_dir[1] = L':';
		root_dir[2] = L'\\';
		root_dir[3] = 0;
		p_root_dir = root_dir;
	}
	else
	{
		// Relative pathname.
		p_root_dir = NULL;
	}

	bRet = GetVolumeInformation(p_root_dir, NULL, 0, NULL, NULL,
		&dwFileSystemFlags, NULL, 0);
	if (bRet != 0 && (dwFileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)) {
		// File system supports sparse files.
		// Mark the file as sparse.
		HANDLE h_extract = (HANDLE)_get_osfhandle(_fileno(f->file));
		if (h_extract != NULL && h_extract != INVALID_HANDLE_VALUE) {
			DWORD bytesReturned;
			FILE_SET_SPARSE_BUFFER fssb;
			fssb.SetSparse = TRUE;

			// TODO: Use SetEndOfFile() if this succeeds?
			// Note that SetEndOfFile() isn't guaranteed to fill the
			// resulting file with zero bytes...
			((void)size);
			DeviceIoControl(h_extract, FSCTL_SET_SPARSE,
				&fssb, sizeof(fssb),
				NULL, 0, &bytesReturned, NULL);
		}
	}
#elif defined(HAVE_FTRUNCATE)
	// Set the file size.
	// NOTE: If the underlying file system doesn't support sparse files,
	// this may take a long time. (TODO: Check this.)
	int ret = ftruncate(fileno(f->file), size);
	if (ret != 0) {
		// Error setting the file size.
		// Allow all errors except for EINVAL or EFBIG,
		// since those mean the file system doesn't support
		// large files.
		int err = errno;
		if (err == EINVAL || err == EFBIG) {
			// File is too big.
			return -err;
		} else {
			// Ignore this error.
			errno = 0;
		}
	}
#endif /* HAVE_FTRUNCATE */

	return 0;
}
