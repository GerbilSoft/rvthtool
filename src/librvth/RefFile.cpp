/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * RefFile.cpp: Reference-counted FILE*. (use std::shared_ptr<>)           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.librvth.h"
#include "config.libc.h"

#include "RefFile.hpp"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cctype>
#include <cerrno>
#include <cstring>

// OS-specific includes
#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  include <winioctl.h>
#  include "libwiicrypto/win32/w32time.h"
#else /* !_WIN32 */
#  include <sys/ioctl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#  ifdef __linux__
#    include <linux/fs.h>
#  endif /* __linux__ */
#endif /* !_WIN32 */

// NOTE: Some functions don't check for nullptr, since they should
// loudly crash if nullptr is passed.

/**
 * Open a file as a reference-counted file.
 * The file is opened as a binary file in read-only mode.
 *
 * Check isOpen() after constructing the class to determine
 * if the file was opened successfully.
 *
 * @param filename Filename.
 * @param create If true, create the file if it doesn't exist.
 *               File will be opened in read/write mode.
 *               File will be truncated if it already exists.
 *
 * @return RefFile*, or NULL if an error occurred.
 */
RefFile::RefFile(const TCHAR *filename, bool create)
	: m_file(nullptr)
	, m_lastError(0)
	, m_isWritable(false)
{
	if (!filename) {
		// No filename...
		m_lastError = EINVAL;
		return;
	}

	// Save the filename.
	m_filename = filename;

	// Open the file.
	const TCHAR *const mode = (create ? _T("wb+") : _T("rb"));
	m_file = _tfopen(filename, mode);
	if (!m_file) {
		// Could not open the file.
		m_lastError = errno;
		if (m_lastError == 0) {
			m_lastError = EIO;
		}
		return;
	}

	// If the file was opened with 'create',
	// it should be considered writable.
	m_isWritable = create;
}

RefFile::~RefFile()
{
	if (m_file) {
		fclose(m_file);
	}
}

/**
 * Reopen the file with write access.
 * @param f RefFile*.
 * @return 0 on success; negative POSIX error code on error.
 */
int RefFile::makeWritable(void)
{
	if (m_isWritable) {
		// File is already writable.
		return 0;
	} else if (!m_file) {
		// File is not open.
		return -EBADF;
	}

	// Get the current position.
	off64_t pos = ftello(m_file);

	// Close and reopen the file as writable.
	// TODO: Potential race condition...
	int ret = 0;
	fclose(m_file);
	m_file = nullptr;

	// NOTE: O_SYNC breaks recryption somehow. (EREMOTEIO, error 121)
	// We'll just call flush() every so often instead.
	m_file = _tfopen(m_filename.c_str(), _T("rb+"));

	if (m_file) {
		// File reopened as writable.
		m_isWritable = true;
	} else {
		// Could not reopen as writable.
		ret = -errno;
		// Reopen as read-only.
		m_file = _tfopen(m_filename.c_str(), _T("rb"));
		assert(m_file != nullptr);
		// FIXME: If it's NULL, something's wrong...
	}

	// Seek to the original position.
	// TODO: Check for errors.
	fseeko(m_file, pos, SEEK_SET);
	return ret;
}

/**
 * Check if the file is a device file.
 * @return True if this is a device file; false if it isn't.
 */
bool RefFile::isDevice(void) const
{
	if (!m_file) {
		// No file...
		return false;
	}

#if defined(_WIN32)
	// Windows: Check the beginning of the filename.
	if (m_filename.empty()) {
		// No filename...
		return false;
	}

	return !_tcsnicmp(m_filename.c_str(), _T("\\\\.\\PhysicalDrive"), 17);
#elif defined(HAVE_STATX)
	struct statx sbx;
	int ret = statx(fileno(m_file), "", AT_EMPTY_PATH, STATX_TYPE, &sbx);
	if (ret != 0 || !(sbx.stx_mask & STATX_TYPE)) {
		// statx() failed and/or did not return the file type.
		return -1;
	}

	// NOTE: FreeBSD dropped "block" devices, so we need to
	// check for both block and character devices.
	return !!(S_ISBLK(sbx.stx_mode) | S_ISCHR(sbx.stx_mode));
#else
	struct stat sb;
	int ret = fstat(fileno(m_file), &sb);
	if (ret != 0) {
		// fstat() failed.
		return false;
	}

	// NOTE: FreeBSD dropped "block" devices, so we need to
	// check for both block and character devices.
	return !!(S_ISBLK(sb.st_mode) | S_ISCHR(sb.st_mode));
#endif
}

/**
 * Try to make this file a sparse file.
 * @param size If not zero, try to set the file to this size.
 * @return 0 on success; negative POSIX error code on error.
 */
int RefFile::makeSparse(off64_t size)
{
#ifdef _WIN32
	wchar_t root_dir[4];		// Root directory.
	wchar_t *p_root_dir;		// Pointer to root_dir, or NULL if relative.

	// Check if the file system supports sparse files.
	// TODO: Handle mount points?
	if (m_filename.size() >= 3 &&
	    _istalpha(m_filename[0]) && m_filename[1] == _T(':') &&
	    (m_filename[2] == _T('\\') || m_filename[2] == _T('/')))
	{
		// Absolute pathname.
		root_dir[0] = m_filename[0];
		root_dir[1] = L':';
		root_dir[2] = L'\\';
		root_dir[3] = 0;
		p_root_dir = root_dir;
	}
	else
	{
		// Relative pathname.
		p_root_dir = nullptr;
	}

	DWORD dwFileSystemFlags;
	BOOL bRet = GetVolumeInformation(p_root_dir, nullptr, 0, nullptr, nullptr,
		&dwFileSystemFlags, nullptr, 0);
	if (bRet != 0 && (dwFileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)) {
		// File system supports sparse files.
		// Mark the file as sparse.
		HANDLE h_extract = (HANDLE)_get_osfhandle(_fileno(m_file));
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
	if (size == 0) {
		// Nothing to do here.
		return 0;
	}

	int ret = ftruncate(fileno(m_file), size);
	if (ret != 0) {
		// Error setting the file size.
		// Allow all errors except for EINVAL or EFBIG,
		// since those mean the file system doesn't support
		// large files.
		int err = errno;
		if (err == EINVAL || err == EFBIG) {
			// File is too big.
			m_lastError = err;
			return -err;
		} else {
			// Ignore this error.
			errno = 0;
		}
	}
#endif /* HAVE_FTRUNCATE */

	return 0;
}

/**
 * Get the size of the file.
 * @return Size of file, or -1 on error.
 */
off64_t RefFile::size(void)
{
	if (!m_file) {
		// No file...
		return -1;
	}

	// If this is a device, try OS-specific device size functions first.
	// NOTE: _fseeki64(fp, 0, SEEK_END) isn't working on
	// device files on Windows for some reason...
	if (this->isDevice()) {
#ifdef _WIN32
		// Windows version.
		HANDLE hDevice = (HANDLE)_get_osfhandle(_fileno(m_file));
		if (hDevice && hDevice != INVALID_HANDLE_VALUE) {
			// Reference: https://docs.microsoft.com/en-us/windows/desktop/api/winioctl/ni-winioctl-ioctl_disk_get_length_info
			GET_LENGTH_INFORMATION gli;
			DWORD dwBytesReturned = 0;
			BOOL bRet = DeviceIoControl(hDevice,
				IOCTL_DISK_GET_LENGTH_INFO,	// dwIoControlCode
				nullptr,			// lpInBuffer
				0,				// nInBufferSize
				(LPVOID)&gli,			// output buffer
				(DWORD)sizeof(gli),		// size of output buffer
				&dwBytesReturned,		// number of bytes returned
				nullptr);			// OVERLAPPED structure
			if (bRet && dwBytesReturned == sizeof(gli)) {
				// Size obtained successfully.
				return gli.Length.QuadPart;
			}
		}
#elif defined(__linux__)
		// Linux version.
		// Reference: http://www.microhowto.info/howto/get_the_size_of_a_linux_block_special_device_in_c.html
		off64_t ret = -1;
		if (ioctl(fileno(m_file), BLKGETSIZE64, &ret) == 0) {
			// Size obtained successfully.
			return ret;
		}
#endif
	}

	// Not a device, or the OS-specific device size function failed.
	// Use this->seeko() / this->tello().
	off64_t orig_pos = this->tello();
	if (orig_pos < 0) {
		// Error.
		return -1;
	}
	int err = this->seeko(0, SEEK_END);
	if (err != 0) {
		// Error.
		return -1;
	}
	off64_t ret = this->tello();
	err = this->seeko(orig_pos, SEEK_SET);
	if (err != 0) {
		// Error.
		return -1;
	}
	return ret;
}

/**
 * Get the file's modification time.
 * @return File modification time, or -1 on error.
 */
time_t RefFile::mtime(void)
{
	if (!m_file) {
		// No file...
		return -1;
	}

#if defined(_WIN32)
	HANDLE h_file = (HANDLE)_get_osfhandle(_fileno(m_file));
	assert(h_file != nullptr);
	assert(h_file != INVALID_HANDLE_VALUE);
	if (!h_file || h_file == INVALID_HANDLE_VALUE) {
		// Can't get the underlying HANDLE...
		return -1;
	}

	FILETIME ft_mtime;
	BOOL bRet = GetFileTime(h_file, nullptr, nullptr, &ft_mtime);
	if (!bRet) {
		// GetFileTime() failed.
		return -1;
	}

	return FileTimeToUnixTime(&ft_mtime);
#elif defined(HAVE_STATX)
	struct statx sbx;
	int ret = statx(fileno(m_file), "", AT_EMPTY_PATH, STATX_MTIME, &sbx);
	if (ret != 0 || !(sbx.stx_mask & STATX_MTIME)) {
		// statx() failed and/or did not return the mtime.
		return -1;
	}
	return sbx.stx_mtime.tv_sec;
#else
	struct stat sb;
	int ret = fstat(fileno(m_file), &sb);
	if (ret != 0) {
		// fstat() failed.
		return -1;
	}
	return sb.st_mtime;
#endif
}

int RefFile::flush(void)
{
	int ret = ::fflush(m_file);
	if (ret != 0) return ret;
#ifdef _WIN32
	return !FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(m_file)));
#else /* !_WIN32 */
	return ::fsync(fileno(m_file));
#endif /* _WIN32 */
}
