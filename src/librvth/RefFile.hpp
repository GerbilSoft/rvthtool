/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * RefFile.hpp: Reference-counted FILE*. (use std::shared_ptr<>)           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "libwiicrypto/common.h"
#include "tcharx.h"

// C includes
#include <stdint.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstdio>

// C++ STL classes
#include <memory>
#include <string>

class RefFile
{
public:
	/**
	 * Open a file as a reference-counted file.
	 * The file is opened as a binary file in read-only mode.
	 *
	 * NOTE: Internal reference counting has been removed.
	 * Use shared_ptr<> and RefFilePtr instead.
	 *
	 * Check isOpen() after constructing the object to determine
	 * if the file was opened successfully.
	 *
	 * @param filename Filename.
	 * @param create If true, create the file if it doesn't exist.
	 *               File will be opened in read/write mode.
	 *               File will be truncated if it already exists.
	 *
	 * @return RefFile*, or NULL if an error occurred.
	 */
	RefFile(const TCHAR *filename, bool create = false);
	~RefFile();

private:
	DISABLE_COPY(RefFile)

public:
	/**
	 * Is the file open?
	 * @return True if open; false if not.
	 */
	inline bool isOpen(void) const
	{
		return (m_file != nullptr);
	}

	/**
	 * Get the last error.
	 * @return Last error.
	 */
	inline int lastError(void) const
	{
		return m_lastError;
	}

	/**
	 * Reopen the file with write access.
	 * @return 0 on success; negative POSIX error code on error.
	 */
	int makeWritable(void);

	/**
	 * Check if the file is a device file.
	 * @return True if this is a device file; false if it isn't.
	 */
	bool isDevice(void) const;

	/**
	 * Try to make this file a sparse file.
	 * @param size If not zero, try to set the file to this size.
	 * @return 0 on success; negative POSIX error code on error.
	 */
	int makeSparse(off64_t size = 0);

	/**
	 * Get the size of the file.
	 * @return Size of file, or -1 on error.
	 */
	off64_t size(void);

	/**
	 * Get the file's modification time.
	 * @return File modification time, or -1 on error.
	 */
	time_t mtime(void);

public:
	/** Convenience wrappers for stdio functions. **/
	// NOTE: These functions set errno, **NOT** m_lastError!

	inline size_t read(void *ptr, size_t size, size_t nmemb)
	{
		return ::fread(ptr, size, nmemb, m_file);
	}

	inline size_t write(const void *ptr, size_t size, size_t nmemb)
	{
		return ::fwrite(ptr, size, nmemb, m_file);
	}

	inline int seeko(off64_t offset, int whence)
	{
		return ::fseeko(m_file, offset, whence);
	}

	inline off64_t tello(void)
	{
		return ::ftello(m_file);
	}

	int flush(void);

	inline void rewind(void)
	{
		::rewind(m_file);
	}

	/** Convenience wrappers **/

	inline size_t seekoAndRead(off64_t offset, int whence, void *ptr, size_t size, size_t nmemb)
	{
		int ret = this->seeko(offset, whence);
		if (ret != 0) {
			if (errno == 0) {
				errno = EIO;
			}
			return 0;
		}

		return this->read(ptr, size, nmemb);
	}

	/** Convenience wrappers for various RefFile fields **/

	inline const TCHAR *filename(void) const
	{
		return m_filename.c_str();
	}

	inline bool isWritable(void) const
	{
		return m_isWritable;
	}

private:
	FILE *m_file;			// FILE pointer
	std::tstring m_filename;	// Filename for reopening as writable
	int m_lastError;		// Last error code
	bool m_isWritable;		// Is the file writable?
};

typedef std::shared_ptr<RefFile> RefFilePtr;
