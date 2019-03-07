/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * PlainReader.cpp: Plain disc image reader class.                         *
 * Used for plain binary disc images, e.g. .gcm and RVT-H images.          *
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

#include "PlainReader.hpp"

// For LBA_TO_BYTES()
#include "nhcd_structs.h"

// C includes.
#include <stdlib.h>

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>

/**
 * Create a plain reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 * @return Reader*, or NULL on error.
 */
PlainReader::PlainReader(RefFile *file, uint32_t lba_start, uint32_t lba_len)
	: super(file, lba_start, lba_len)
{
	if (!isOpen()) {
		// File wasn't opened.
		return;
	}

	// Get the file size.
	errno = 0;
	int64_t filesize = m_file->size();
	if (filesize < 0) {
		// Seek error.
		// NOTE: Not failing on empty file, since that happens
		// when creating a new file to extract an image.
		if (errno == 0) {
			errno = EIO;
		}
		m_file->unref();
		m_file = nullptr;
		return;
	}

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// NOTE: If not a multiple of the LBA size,
		// the partial LBA will be ignored.
		lba_len = (uint32_t)(filesize / LBA_SIZE);
	}
	m_lba_start = lba_start;
	m_lba_len = lba_len;

	// Set the reader type.
	if (m_file->isDevice()) {
		// This is an RVT-H Reader.
		m_type = RVTH_ImageType_HDD_Reader;
	} else {
		// If the file is larger than 10 GB, assume it's an RVT-H Reader disk image.
		// Otherwise, it's a standalone disc image.
		if (filesize > 10LL*1024LL*1024LL*1024LL) {
			// RVT-H Reader disk image.
			m_type = RVTH_ImageType_HDD_Image;
		} else {
			// If the starting LBA is 0, it's a standard GCM.
			// Otherwise, it has an SDK header.
			m_type = (lba_start == 0
				? RVTH_ImageType_GCM
				: RVTH_ImageType_GCM_SDK);
		}
	}

	// Reader initialized.
}

/**
 * Read data from the disc image.
 * @param reader	[in] Reader*
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
uint32_t PlainReader::read(void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// LBA bounds checking.
	// TODO: Check for overflow?
	lba_start += m_lba_start;
	assert(lba_start + lba_len <= m_lba_start + m_lba_len);
	if (lba_start + lba_len > m_lba_start + m_lba_len) {
		// Out of range.
		errno = EIO;
		return 0;
	}

	// Seek to lba_start.
	int ret = m_file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return 0;
	}

	// Read the data.
	return (uint32_t)m_file->read(ptr, LBA_SIZE, lba_len);
}

/**
 * Write data to the disc image.
 * @param reader	[in] Reader*
 * @param ptr		[in] Write buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
uint32_t PlainReader::write(const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// LBA bounds checking.
	// TODO: Check for overflow?
	lba_start += m_lba_start;
	assert(lba_start + lba_len <= m_lba_start + m_lba_len);
	if (lba_start + lba_len > m_lba_start + m_lba_len) {
		// Out of range.
		errno = EIO;
		return 0;
	}

	// Seek to lba_start.
	int ret = m_file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return 0;
	}

	// Write the data.
	return (uint32_t)m_file->write(ptr, LBA_SIZE, lba_len);
}
