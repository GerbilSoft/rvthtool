/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * CisoReader.cpp: CISO disc image reader class.                           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "CisoReader.hpp"
#include "byteswap.h"

// For LBA_TO_BYTES()
#include "nhcd_structs.h"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

// C++ includes
#include <array>
using std::array;

// CISO magic
static const array<char, 4> CISO_MAGIC = {{'C','I','S','O'}};

typedef struct _CisoHeader {
	char magic[4];				// "CISO"
	uint32_t block_size;			// LE32
	uint8_t map[CisoReader::CISO_MAP_SIZE];	// 0 == unused; 1 == used; other == invalid
} CisoHeader;
ASSERT_STRUCT(CisoHeader, 4+4+CisoReader::CISO_MAP_SIZE);

/**
 * Is a given disc image supported by the CISO reader?
 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
 * @param size	[in] Size of sbuf. (should be 512 or larger)
 * @return True if supported; false if not.
 */
bool CisoReader::isSupported(const uint8_t *sbuf, size_t size)
{
	assert(sbuf != nullptr);
	assert(size >= LBA_SIZE);
	if (!sbuf || size < LBA_SIZE) {
		return false;
	}

	// Check for CISO magic.
	if (memcmp(sbuf, CISO_MAGIC.data(), CISO_MAGIC.size()) != 0) {
		// Invalid magic.
		return false;
	}

	// Check if the block size is a supported power of two.
	// - Minimum: CISO_BLOCK_SIZE_MIN (32 KB, 1 << 15)
	// - Maximum: CISO_BLOCK_SIZE_MAX (16 MB, 1 << 24)
	// TODO: isPow2() from rom-properties?
	// TODO: Convert to shift amount?
	const uint32_t block_size = sbuf[4] |
	                           (sbuf[5] << 8) |
	                           (sbuf[6] << 16) |
				   (sbuf[7] << 24);
	bool isPow2 = false;
	for (unsigned int i = 15; i <= 24; i++) {
		if (block_size == (1U << i)) {
			isPow2 = true;
			break;
		}
	}
	if (!isPow2) {
		// Block size is out of range.
		// If the block size is 0x18, then this is
		// actually a PSP CISO, and this field is
		// the CISO header size.
		return false;
	}

	// This is a CISO image file.
	return true;
}

/**
 * Create a CISO reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile
 * @param lba_start	[in] Starting LBA
 * @param lba_len	[in] Length, in LBAs
 * @return Reader*, or NULL on error.
 */
CisoReader::CisoReader(const RefFilePtr &file, uint32_t lba_start, uint32_t lba_len)
	: super(file, lba_start, lba_len)
	, m_real_lba_len(0)
	, m_block_size_lba(0)
{
	int ret;
	int err = 0;
	size_t size;
	uint16_t physBlockIdx = 0;
	uint16_t maxLogicalBlockUsed = 0;

	if (!isOpen()) {
		// File wasn't opened.
		return;
	}

	// Allocate memory for the CisoHeader object.
	CisoHeader *cisoHeader = new CisoHeader;

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// Determine the maximum LBA.
		off64_t offset = m_file->size();
		if (offset <= 0) {
			// Empty file and/or seek error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			goto fail;
		}

		// NOTE: If not a multiple of the LBA size,
		// the partial LBA will be ignored.
		lba_len = static_cast<uint32_t>(offset / LBA_SIZE);
	}
	m_lba_start = lba_start + BYTES_TO_LBA(sizeof(*cisoHeader));
	m_lba_len = 0;	// will be set after reading the CISO header
	m_real_lba_len = lba_len;

	// Read the CISO header.
	ret = m_file->seeko(lba_start * LBA_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}
	size = m_file->read(cisoHeader, 1, sizeof(*cisoHeader));
	if (size != sizeof(*cisoHeader)) {
		// Short read.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// Validate the CISO header.
	if (!isSupported(reinterpret_cast<const uint8_t*>(cisoHeader), LBA_SIZE)) {
		// Not a valid CISO header.
		err = EIO;
		goto fail;
	}
	m_block_size_lba = BYTES_TO_LBA(le32_to_cpu(cisoHeader->block_size));

	// Clear the CISO block map initially.
	m_blockMap.fill(0xFF);

	// Parse the CISO block map.
	for (unsigned int i = 0; i < static_cast<unsigned int>(m_blockMap.size()); i++) {
		switch (cisoHeader->map[i]) {
			case 0:
				// Empty block.
				continue;
			case 1:
				// Used block.
				m_blockMap[i] = physBlockIdx;
				physBlockIdx++;
				maxLogicalBlockUsed = i;
				break;
			default:
				// Invalid entry.
				goto fail;
		}
	}

	// Calculate the image size based on the highest logical block index.
	m_lba_len = static_cast<uint32_t>(maxLogicalBlockUsed + 1) * m_block_size_lba;

	// Reader initialized.
	delete cisoHeader;
	m_type = RVTH_ImageType_GCM;
	return;

fail:
	// Failed to initialize the reader.
	delete cisoHeader;
	m_file.reset();
	errno = err;
}

/**
 * Read data from the disc image.
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
uint32_t CisoReader::read(void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// Return value.
	uint32_t lbas_read = 0;

	// LBA bounds checking.
	// TODO: Check for overflow?
	assert(lba_start + m_lba_start + lba_len <=
	       m_lba_start + m_lba_len);
	if (lba_start + m_lba_start + lba_len >
	    m_lba_start + m_lba_len)
	{
		// Out of range.
		errno = EIO;
		return 0;
	}

	// TODO: Optimize reading multiple LBAs at once.
	// For now, we're looking up a single LBA at a time.
	uint8_t *ptr8 = static_cast<uint8_t*>(ptr);
	uint32_t lba_end = lba_start + lba_len;
	for (uint32_t lba = lba_start; lba < lba_end; lba++, ptr8 += LBA_SIZE) {
		unsigned int physBlockIdx = m_blockMap[lba / m_block_size_lba];
		if (physBlockIdx == 0xFFFF) {
			// Empty block.
			memset(ptr8, 0, LBA_SIZE);
		} else {
			// Determine the offset.
			const unsigned int blockStart = physBlockIdx * m_block_size_lba;
			const unsigned int offset = lba % m_block_size_lba;

			int ret = m_file->seeko(LBA_TO_BYTES(blockStart + offset + m_lba_start), SEEK_SET);
			if (ret != 0) {
				// Seek error.
				if (errno == 0) {
					errno = EIO;
				}
				return 0;
			}
			size_t size = m_file->read(ptr8, LBA_SIZE, 1);
			if (size != 1) {
				// Read error.
				if (errno == 0) {
					errno = EIO;
				}
				return 0;
			}
			lbas_read++;
		}
	}

	return lbas_read;
}
