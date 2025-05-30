/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * CisoReader.hpp: CISO disc image reader class.                           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "Reader.hpp"

// C++ STL classes
#include <array>

class CisoReader : public Reader
{
public:
	/**
	 * Create a CISO reader for a disc image.
	 *
	 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
	 * will be used.
	 *
	 * @param file		RefFile
	 * @param lba_start	[in] Starting LBA
	 * @param lba_len	[in] Length, in LBAs
	 */
	CisoReader(const RefFilePtr &file, uint32_t lba_start, uint32_t lba_len);

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

public:
	static constexpr size_t CISO_HEADER_SIZE = 0x8000U;
	static constexpr size_t CISO_MAP_SIZE = (CISO_HEADER_SIZE - sizeof(uint32_t) - (sizeof(char) * 4U));

	// 32 KB minimum block size (GCN/Wii sector)
	// 16 MB maximum block size
	static constexpr size_t CISO_BLOCK_SIZE_MIN = 32768;
	static constexpr size_t CISO_BLOCK_SIZE_MAX = 16U*1024U*1024U;

private:
	// NOTE: reader.lba_len is the virtual image size.
	// real_lba_len is the actual image size.
	uint32_t m_real_lba_len;

	// CISO block size, in LBAs.
	uint32_t m_block_size_lba;

	// Block map.
	// 0x0000 == first block after CISO header.
	// 0xFFFF == empty block.
	std::array<uint16_t, CISO_MAP_SIZE> m_blockMap;
};
