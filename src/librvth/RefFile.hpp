/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * RefFile.hpp: Reference-counted FILE*.                                   *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#ifndef __RVTHTOOL_LIBRVTH_REFFILE_HPP__
#define __RVTHTOOL_LIBRVTH_REFFILE_HPP__

#ifdef __cplusplus

#include "libwiicrypto/common.h"
#include "tcharx.h"

// C includes.
#include <stdint.h>

// C includes. (C++ namespace)
#include <cassert>
#include <cstdio>

// C++ includes.
#include <string>

class RefFile
{
	public:
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
		RefFile(const TCHAR *filename, bool create = false);
	private:
		~RefFile();	// call unref() instead

	private:
		DISABLE_COPY(RefFile)

	public:
		/**
		 * Take a reference to this RefFile.
		 * @return RefFile*
		 */
		inline RefFile *ref(void)
		{
			// TODO: Atomic increment?
			m_refCount++;
			return this;
		}

		/**
		 * Release a reference to this RefFile.
		 */
		inline void unref(void)
		{
			// TODO: Atomic decrement?
			assert(m_refCount > 0);
			if (--m_refCount <= 0) {
				// Delete the object.
				delete this;
			}
		}

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
		int makeSparse(int64_t size = 0);

		/**
		 * Get the size of the file.
		 * @return Size of file, or -1 on error.
		 */
		int64_t size(void);

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

		inline int seeko(int64_t offset, int whence)
		{
			return ::fseeko(m_file, offset, whence);
		}

		inline int64_t tello(void)
		{
			return ::ftello(m_file);
		}

		inline int64_t flush(void)
		{
			return ::fflush(m_file);
		}

		inline void rewind(void)
		{
			::rewind(m_file);
		}

		/** Convenience wrappers for various RefFile fields. **/

		inline const TCHAR *filename(void) const
		{
			return m_filename.c_str();
		}

		inline bool isWritable(void) const
		{
			return m_isWritable;
		}

	private:
		int m_refCount;			// Reference count
		int m_lastError;		// Last error code
		FILE *m_file;			// FILE pointer
		std::tstring m_filename;	// Filename for reopening as writable
		bool m_isWritable;		// Is the file writable?
};

#else /* !__cplusplus */
// FIXME: C compatibility hack - remove this once everything is ported to C++.
struct RefFile;
typedef struct RefFile RefFile;
#endif /* __cplusplus */

#endif /* __RVTHTOOL_LIBRVTH_REFFILE_HPP__ */
