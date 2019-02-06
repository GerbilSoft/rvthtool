/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * Reader.hpp: Disc image reader base class.                               *
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

#ifndef __RVTHTOOL_LIBRVTH_READER_READER_HPP__
#define __RVTHTOOL_LIBRVTH_READER_READER_HPP__

#include "libwiicrypto/common.h"
#include "RefFile.hpp"
#include "rvth_imagetype.h"

#include <stdint.h>

#ifdef __cplusplus

class Reader
{
	protected:
		/**
		 * Reader base class.
		 * @param file		RefFile*.
		 * @param lba_start	[in] Starting LBA,
		 * @param lba_len	[in] Length, in LBAs.
		 */
		Reader(RefFile *file, uint32_t lba_start, uint32_t lba_len);
	public:
		virtual ~Reader();

	private:
		DISABLE_COPY(Reader)

	public:
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
		static Reader *open(RefFile *file, uint32_t lba_start, uint32_t lba_len);

	public:
		/**
		 * Is the Reader object open?
		 * @return True if open; false if not.
		 */
		inline bool isOpen(void) const
		{
			return (m_file != nullptr);
		}

	public:
		/** I/O functions **/

		// TODO: lba_start param, or separate seek()?
		// Regardless, we have to seek on each read because another
		// disc image may have altered the file pointer.

		/**
		 * Read data from the disc image.
		 * @param ptr		[out] Read buffer.
		 * @param lba_start	[in] Starting LBA.
		 * @param lba_len	[in] Length, in LBAs.
		 * @return Number of LBAs read, or 0 on error.
		 */
		virtual uint32_t read(void *ptr, uint32_t lba_start, uint32_t lba_len) = 0;

		/**
		 * Write data to the disc image.
		 * @param ptr		[in] Write buffer.
		 * @param lba_start	[in] Starting LBA.
		 * @param lba_len	[in] Length, in LBAs.
		 * @return Number of LBAs read, or 0 on error.
		 */
		virtual uint32_t write(const void *ptr, uint32_t lba_start, uint32_t lba_len);

		/**
		 * Flush the file buffers.
		 */
		void flush(void);

	public:
		/** Accessors **/

		/**
		 * Get the starting LBA.
		 * @return Starting LBA.
		 */
		inline uint32_t lba_start(void) const { return m_lba_start; }

		/**
		 * Get the length, in LBAs.
		 * @return Length, in LBAs.
		 */
		inline uint32_t lba_len(void) const { return m_lba_len; }

		/**
		 * Get the image type.
		 * @return Image type.
		 */
		inline RvtH_ImageType_e type(void) const { return m_type; }

	public:
		/** Special functions **/

		/**
		 * Adjust the LBA starting offset for SDK headers.
		 * @param lba_count Number of LBAs to adjust.
		 */
		inline void lba_adjust(uint32_t lba_count)
		{
			if (lba_count > m_lba_len) {
				lba_count = m_lba_len;
			}
			m_lba_start += lba_count;
			m_lba_len -= lba_count;
		}

	protected:
		RefFile *m_file;		// Disc image file
		uint32_t m_lba_start;		// Starting LBA
		uint32_t m_lba_len;		// Length of image, in LBAs
		RvtH_ImageType_e m_type;	// Disc image type
};

#else /* !__cplusplus */
// FIXME: C compatibility hack - remove this once everything is ported to C++.
struct Reader;
typedef struct Reader Reader;
#endif /* __cplusplus */

#endif /* __RVTHTOOL_LIBRVTH_READER_READER_HPP__ */
