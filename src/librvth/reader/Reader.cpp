/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * Reader.cpp: Disc image reader base class.                               *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "Reader.hpp"
#include "PlainReader.hpp"
#include "CisoReader.hpp"
#include "WbfsReader.hpp"

// For LBA_TO_BYTES()
#include "nhcd_structs.h"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

Reader::Reader(RefFile *file, uint32_t lba_start, uint32_t lba_len)
	: m_file(nullptr)
	, m_lba_start(lba_start)
	, m_lba_len(lba_len)
	, m_type(RVTH_ImageType_Unknown)
{
	// Validate parameters.
	assert(file != nullptr);
	assert(lba_start == 0 || lba_len != 0);
	if (!file || (lba_start > 0 && lba_len == 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return;
	}

	// ref() the file.
	m_file = file->ref();
}

Reader::~Reader()
{
	if (m_file) {
		m_file->unref();
	}
}

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
Reader *Reader::open(RefFile *file, uint32_t lba_start, uint32_t lba_len)
{
	assert(file != NULL);
	if (file->isDevice()) {
		// This is an RVT-H Reader system.
		// Only plain images are supported.
		// TODO: Also check for RVT-H Reader HDD images?
		return new PlainReader(file, lba_start, lba_len);
	}

	// This is a disc image file.
	// NOTE: Ignoring errors here because the file may be empty
	// if we're opening a file for writing.

	// Check for other disc image formats.
	uint8_t sbuf[4096];
	int ret = file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		if (errno == 0) {
			errno = EIO;
		}
		return nullptr;
	}
	errno = 0;
	size_t size = file->read(sbuf, 1, sizeof(sbuf));
	if (size != sizeof(sbuf)) {
		// Short read. May be empty.
		if (errno != 0) {
			// Actual error.
			return nullptr;
		} else {
			// Assume it's a new file.
			// Use the plain disc image reader.
			file->rewind();
			return new PlainReader(file, lba_start, lba_len);
		}
	}
	file->rewind();

	// Check the magic number.
	if (CisoReader::isSupported(sbuf, sizeof(sbuf))) {
		// This is a supported CISO image.
		return new CisoReader(file, lba_start, lba_len);
	} else if (WbfsReader::isSupported(sbuf, sizeof(sbuf))) {
		// This is a supported WBFS image.
		return new WbfsReader(file, lba_start, lba_len);
	}

	// Check for SDK headers.
	// TODO: Verify GC1L, NN2L. (These are all NN1L images.)
	// TODO: Checksum at 0x0830.
	static const uint8_t sdk_0x0000[4] = {0xFF,0xFF,0x00,0x00};
	static const uint8_t sdk_0x082C[4] = {0x00,0x00,0xE0,0x06};
	if (lba_len > BYTES_TO_LBA(32768) &&
	    !memcmp(&sbuf[0x0000], sdk_0x0000, sizeof(sdk_0x0000)) &&
	    !memcmp(&sbuf[0x082C], sdk_0x082C, sizeof(sdk_0x082C)) &&
	    sbuf[0x0844] == 0x01)
	{
		// This image has an SDK header.
		// Adjust the LBA values to skip it.
		lba_start += BYTES_TO_LBA(32768);
		lba_len -= BYTES_TO_LBA(32768);
	}

	// Use the plain disc image reader.
	return new PlainReader(file, lba_start, lba_len);
}

/**
 * Write data to a disc image.
 *
 * Base class implementation returns an error.
 * This is useful for read-only subclasses, since they won't
 * have to implement this function.
 *
 * @param ptr		[in] Write buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
uint32_t Reader::write(const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// Base class is not writable.
	UNUSED(ptr);
	UNUSED(lba_start);
	UNUSED(lba_len);

	errno = EROFS;
	return 0;
}

/**
 * Flush the file buffers.
 */
void Reader::flush(void)
{
	m_file->flush();
}
