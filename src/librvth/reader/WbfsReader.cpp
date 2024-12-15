/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * WbfsReader.cpp: WBFS disc image reader class.                           *
 *                                                                         *
 * Copyright (c) 2018-2024 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "WbfsReader.hpp"
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

#include "libwbfs.h"

// WBFS magic.
static const array<char, 4> WBFS_MAGIC = {{'W','B','F','S'}};

/**
 * Is a given disc image supported by the WBFS reader?
 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
 * @param size	[in] Size of sbuf. (should be 512 or larger)
 * @return True if supported; false if not.
 */
bool WbfsReader::isSupported(const uint8_t *sbuf, size_t size)
{
	assert(sbuf != nullptr);
	assert(size >= LBA_SIZE);
	if (!sbuf || size < LBA_SIZE) {
		return false;
	}

	// Check for WBFS magic.
	wbfs_head_t *const head = (wbfs_head_t*)sbuf;
	if (memcmp(&head->magic, WBFS_MAGIC.data(), WBFS_MAGIC.size()) != 0) {
		// Invalid magic.
		return false;
	}

	// Sector size must be at least 512 bytes.
	if (head->hd_sec_sz_s < 0x09) {
		// Sector size is less than 512 bytes.
		// This isn't possible unless you're using
		// a Commodore 64 or an Apple ][.
		return false;
	}

	// This is a WBFS image file.
	return true;
}

// from libwbfs.c
// TODO: Optimize this?
static inline uint8_t size_to_shift(uint32_t size)
{
	uint8_t ret = 0;
	while (size) {
		ret++;
		size >>= 1;
	}
	return ret-1;
}

#define ALIGN_LBA(x) (((x)+p->hd_sec_sz-1)&(~(size_t)(p->hd_sec_sz-1)))

/**
 * Read the WBFS header.
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @return wbfs_t*, or NULL on error.
 */
static wbfs_t *readWbfsHeader(RefFile *file, uint32_t lba_start)
{
	wbfs_head_t *head = nullptr;
	wbfs_t *p = nullptr;

	unsigned int hd_sec_sz;
	int ret = -1;
	size_t size;

	// Assume 512-byte sectors initially.
	hd_sec_sz = 512;
	head = (wbfs_head_t*)malloc(hd_sec_sz);
	if (!head) {
		// ENOMEM
		goto end;
	}

	// Read the WBFS header.
	ret = file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		goto end;
	}
	size = file->read(head, 1, hd_sec_sz);
	if (size != hd_sec_sz) {
		// Read error.
		ret = -1;
		goto end;
	}

	// Make sure this is a valid WBFS image.
	if (!WbfsReader::isSupported((const uint8_t*)head, hd_sec_sz)) {
		// Not a valid WBFS.
		ret = -1;
		goto end;
	}

	// wbfs_t struct.
	p = (wbfs_t*)malloc(sizeof(wbfs_t));
	if (!p) {
		// ENOMEM
		ret = -1;
		goto end;
	}

	// Since this is a disc image, we don't know the HDD sector size.
	// Use the value present in the header.
	p->hd_sec_sz = (1 << head->hd_sec_sz_s);
	p->hd_sec_sz_s = head->hd_sec_sz_s;
	p->n_hd_sec = be32_to_cpu(head->n_hd_sec);	// TODO: Use file size?

	// If the sector size is wrong, reallocate and reread.
	if (p->hd_sec_sz != hd_sec_sz) {
		hd_sec_sz = p->hd_sec_sz;
		free(head);
		head = (wbfs_head_t*)malloc(hd_sec_sz);
		if (!head) {
			// ENOMEM
			goto end;
		}

		// Re-read the WBFS header.
		ret = file->seeko(LBA_TO_BYTES(lba_start), SEEK_SET);
		if (ret != 0) {
			// Seek error.
			goto end;
		}
		size = file->read(head, 1, hd_sec_sz);
		if (size != hd_sec_sz) {
			// Read error.
			ret = -1;
			goto end;
		}
	}
	
	// Save the wbfs_head_t in the wbfs_t struct.
	p->head = head;

	// Constants.
	p->wii_sec_sz = 0x8000;
	p->wii_sec_sz_s = size_to_shift(0x8000);
	p->n_wii_sec = (p->n_hd_sec/0x8000)*p->hd_sec_sz;
	p->n_wii_sec_per_disc = 143432*2;	// support for dual-layer discs

	// WBFS sector size.
	p->wbfs_sec_sz_s = head->wbfs_sec_sz_s;
	p->wbfs_sec_sz = (1 << head->wbfs_sec_sz_s);

	// Disc size.
	p->n_wbfs_sec = p->n_wii_sec >> (p->wbfs_sec_sz_s - p->wii_sec_sz_s);
	p->n_wbfs_sec_per_disc = p->n_wii_sec_per_disc >> (p->wbfs_sec_sz_s - p->wii_sec_sz_s);
	p->disc_info_sz = (uint16_t)ALIGN_LBA(sizeof(wbfs_disc_info_t) + p->n_wbfs_sec_per_disc*2);

	// Free blocks table.
	p->freeblks_lba = (p->wbfs_sec_sz - p->n_wbfs_sec/8) >> p->hd_sec_sz_s;
	p->freeblks = nullptr;
	p->max_disc = (p->freeblks_lba-1) / (p->disc_info_sz >> p->hd_sec_sz_s);
	if (p->max_disc > (p->hd_sec_sz - sizeof(wbfs_head_t)))
		p->max_disc = (uint16_t)(p->hd_sec_sz - sizeof(wbfs_head_t));

	p->n_disc_open = 0;

end:
	if (ret != 0) {
		// Error...
		free(head);
		free(p);
		return nullptr;
	}

	// wbfs_t opened.
	return p;
}

/**
 * Free an allocated WBFS header.
 * This frees all associated structs.
 * All opened discs *must* be closed.
 * @param p wbfs_t struct.
 */
static void freeWbfsHeader(wbfs_t *p)
{
	assert(p != nullptr);
	assert(p->head != nullptr);
	assert(p->n_disc_open == 0);

	// Free everything.
	free(p->head);
	free(p);
}

/**
 * Open a disc from the WBFS image.
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param p		wbfs_t struct.
 * @param index		[in] Disc index.
 * @return Allocated wbfs_disc_t on success; non-zero on error.
 */
static wbfs_disc_t *openWbfsDisc(RefFile *file, uint32_t lba_start, wbfs_t *p, uint32_t index)
{
	// Based on libwbfs.c's wbfs_open_disc()
	// and wbfs_get_disc_info().
	const wbfs_head_t *const head = p->head;
	uint32_t count = 0;
	uint32_t i;
	for (i = 0; i < p->max_disc; i++) {
		if (head->disc_table[i]) {
			if (count++ == index) {
				// Found the disc table index.
				int ret;
				size_t size;

				wbfs_disc_t *disc = (wbfs_disc_t*)malloc(sizeof(wbfs_disc_t));
				if (!disc) {
					// ENOMEM
					return nullptr;
				}
				disc->p = p;
				disc->i = i;

				// Read the disc header.
				disc->header = (wbfs_disc_info_t*)malloc(p->disc_info_sz);
				if (!disc->header) {
					// ENOMEM
					free(disc);
					return nullptr;
				}

				ret = file->seeko(LBA_TO_BYTES(lba_start) + p->hd_sec_sz + (i*p->disc_info_sz), SEEK_SET);
				if (ret != 0) {
					// Seek error.
					free(disc);
					return nullptr;
				}
				size = file->read(disc->header, 1, p->disc_info_sz);
				if (size != p->disc_info_sz) {
					// Error reading the disc information.
					free(disc->header);
					free(disc);
					return nullptr;
				}

				// TODO: Byteswap wlba_table[] here?
				// Removes unnecessary byteswaps when reading,
				// but may not be necessary if we're not reading
				// the entire disc.

				// Disc information read successfully.
				p->n_disc_open++;
				return disc;
			}
		}
	}

	// Disc not found.
	return nullptr;
}

/**
 * Close a WBFS disc.
 * This frees all associated structs.
 * @param disc wbfs_disc_t.
 */
static void closeWbfsDisc(wbfs_disc_t *disc)
{
	assert(disc != nullptr);
	assert(disc->p != nullptr);
	assert(disc->p->n_disc_open > 0);
	assert(disc->header != nullptr);

	// Free the disc.
	disc->p->n_disc_open--;
	free(disc->header);
	free(disc);
}

/**
 * Get the non-sparse size of an open WBFS disc, in bytes.
 * This scans the block table to find the first block
 * from the end of wlba_table[] that has been allocated.
 * @param wlba_table	[in] Pointer to the wlba table
 * @param disc		[in] wbfs_disc_t*
 * @return Non-sparse size, in bytes.
 */
static off64_t getWbfsDiscSize(const be16_t *wlba_table, const wbfs_disc_t *disc)
{
	// Find the last block that's used on the disc.
	// NOTE: This is in WBFS blocks, not Wii blocks.
	const wbfs_t *const p = disc->p;
	int lastBlock = p->n_wbfs_sec_per_disc - 1;
	for (; lastBlock >= 0; lastBlock--) {
		if (wlba_table[lastBlock] != cpu_to_be16(0))
			break;
	}

	// lastBlock+1 * WBFS block size == filesize.
	return (off64_t)(lastBlock + 1) * (off64_t)(p->wbfs_sec_sz);
}

/**
 * Create a WBFS reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 */
WbfsReader::WbfsReader(RefFile *file, uint32_t lba_start, uint32_t lba_len)
	: super(file, lba_start, lba_len)
	, m_real_lba_len(0)
	, m_block_size_lba(0)
	, m_wbfs(nullptr)
	, m_wbfs_disc(nullptr)
	, m_wlba_table(nullptr)
{
	int err = 0;

	if (!isOpen()) {
		// File wasn't opened.
		return;
	}

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// Determine the maximum LBA.
		off64_t offset = file->size();
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
		lba_len = (uint32_t)(offset / LBA_SIZE);
	}

	// WBFS physical block 0 is the header, so we don't
	// need to adjust the starting LBA.
	// NOTE: A block map entry of 0 means empty block.
	m_lba_start = lba_start;
	m_lba_len = 0;	// will be set after reading the WBFS header
	m_real_lba_len = lba_len;

	// Read the WBFS header.
	m_wbfs = readWbfsHeader(file, lba_start);
	if (!m_wbfs) {
		// Error reading the WBFS header.
		goto fail;
	}

	// Open the first disc.
	m_wbfs_disc = openWbfsDisc(file, lba_start, m_wbfs, 0);
	if (!m_wbfs_disc) {
		// Error opening the WBFS disc.
		goto fail;
	}

	// Save important values for later.
	m_wlba_table = m_wbfs_disc->header->wlba_table;
	// TODO: Convert to shift amount?
	m_block_size_lba = BYTES_TO_LBA(m_wbfs->wbfs_sec_sz);

	// Get the size of the WBFS disc.
	m_lba_len = BYTES_TO_LBA(getWbfsDiscSize(m_wlba_table, m_wbfs_disc));

	// Reader initialized.
	m_type = RVTH_ImageType_GCM;
	return;

fail:
	// Failed to initialize the reader.
	if (m_wbfs_disc) {
		closeWbfsDisc(m_wbfs_disc);
		m_wbfs_disc = nullptr;
	}
	if (m_wbfs) {
		freeWbfsHeader(m_wbfs);
		m_wbfs = nullptr;
	}
	m_file->unref();
	m_file = nullptr;
	errno = err;
	return;
}

WbfsReader::~WbfsReader()
{
	// Free the WBFS structs.
	if (m_wbfs_disc) {
		closeWbfsDisc(m_wbfs_disc);
	}
	if (m_wbfs) {
		freeWbfsHeader(m_wbfs);
	}

	// Superclass will unreference the file.
}

/**
 * Read data from a disc image.
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
uint32_t WbfsReader::read(void *ptr, uint32_t lba_start, uint32_t lba_len)
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
		unsigned int physBlockIdx = be16_to_cpu(m_wlba_table[lba / m_block_size_lba]);
		if (physBlockIdx == 0) {
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
