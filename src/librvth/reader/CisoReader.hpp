/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * CisoReader.hpp: CISO disc image reader class.                           *
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

#ifndef __RVTHTOOL_LIBRVTH_READER_CISOREADER_HPP__
#define __RVTHTOOL_LIBRVTH_READER_CISOREADER_HPP__

#include "Reader.hpp"

class CisoReader : public Reader
{
	public:
		/**
		 * Create a CISO reader for a disc image.
		 *
		 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
		 * will be used.
		 *
		 * @param file		RefFile*.
		 * @param lba_start	[in] Starting LBA,
		 * @param lba_len	[in] Length, in LBAs.
		 */
		CisoReader(RefFile *file, uint32_t lba_start, uint32_t lba_len);

	private:
		typedef Reader super;
		DISABLE_COPY(CisoReader)

	public:
		/**
		 * Is a given disc image supported by the CISO reader?
		 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
		 * @param size	[in] Size of sbuf. (should be 512 or larger)
		 * @return True if supported; false if not.
		 */
		static bool isSupported(const uint8_t *sbuf, size_t size);

	public:
		/** I/O functions **/

		/**
		 * Read data from the disc image.
		 * @param ptr		[out] Read buffer.
		 * @param lba_start	[in] Starting LBA.
		 * @param lba_len	[in] Length, in LBAs.
		 * @return Number of LBAs read, or 0 on error.
		 */
		uint32_t read(void *ptr, uint32_t lba_start, uint32_t lba_len) final;

	private:
		#define CISO_HEADER_SIZE 0x8000
		#define CISO_MAP_SIZE (CISO_HEADER_SIZE - sizeof(uint32_t) - (sizeof(char) * 4))

		// 32 KB minimum block size (GCN/Wii sector)
		// 16 MB maximum block size
		#define CISO_BLOCK_SIZE_MIN (32768)
		#define CISO_BLOCK_SIZE_MAX (16*1024*1024)

		// NOTE: reader.lba_len is the virtual image size.
		// real_lba_len is the actual image size.
		uint32_t m_real_lba_len;

		// CISO block size, in LBAs.
		uint32_t m_block_size_lba;

		// Block map.
		// 0x0000 == first block after CISO header.
		// 0xFFFF == empty block.
		uint16_t m_blockMap[CISO_MAP_SIZE];
};

#endif /* __RVTHTOOL_LIBRVTH_READER_CISOREADER_HPP__ */
