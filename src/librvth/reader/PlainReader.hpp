/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * PlainReader.hpp: Plain disc image reader class.                         *
 * Used for plain binary disc images, e.g. .gcm and RVT-H images.          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "Reader.hpp"

class PlainReader : public Reader
{
public:
	/**
	 * Create a plain reader for a disc image.
	 *
	 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
	 * will be used.
	 *
	 * @param file		RefFile
	 * @param lba_start	[in] Starting LBA
	 * @param lba_len	[in] Length, in LBAs
	 */
	PlainReader(const RefFilePtr &file, uint32_t lba_start, uint32_t lba_len);

private:
	typedef Reader super;
	DISABLE_COPY(PlainReader)

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

	/**
	 * Write data to the disc image.
	 * @param ptr		[in] Write buffer.
	 * @param lba_start	[in] Starting LBA.
	 * @param lba_len	[in] Length, in LBAs.
	 * @return Number of LBAs read, or 0 on error.
	 */
	uint32_t write(const void *ptr, uint32_t lba_start, uint32_t lba_len) final;
};
