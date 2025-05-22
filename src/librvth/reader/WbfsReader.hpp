/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * WbfsReader.hpp: WBFS disc image reader class.                           *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "Reader.hpp"

struct wbfs_s;
typedef struct wbfs_s wbfs_t;
struct wbfs_disc_s;
typedef struct wbfs_disc_s wbfs_disc_t;
typedef uint16_t be16_t;

class WbfsReader : public Reader
{
public:
	/**
	 * Create a WBFS reader for a disc image.
	 *
	 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
	 * will be used.
	 *
	 * @param file		RefFile
	 * @param lba_start	[in] Starting LBA
	 * @param lba_len	[in] Length, in LBAs
	 * @return Reader*, or NULL on error.
	 */
	WbfsReader(const RefFilePtr &file, uint32_t lba_start, uint32_t lba_len);

	virtual ~WbfsReader();

private:
	typedef Reader super;
	DISABLE_COPY(WbfsReader);

public:
	/**
	 * Is a given disc image supported by the WBFS reader?
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
	// NOTE: reader.lba_len is the virtual image size.
	// real_lba_len is the actual image size.
	uint32_t m_real_lba_len;

	// WBFS block size, in LBAs.
	uint32_t m_block_size_lba;

	// WBFS structs.
	wbfs_t *m_wbfs;			// WBFS image.
	wbfs_disc_t *m_wbfs_disc;	// Current disc.

	const be16_t *m_wlba_table;	// Pointer to m_wbfs_disc->disc->header->wlba_table.
};
