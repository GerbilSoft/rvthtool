/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader_wbfs.c: WBFS disc image reader class.                            *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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

#include "reader_wbfs.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef LBA_TO_BYTES
# define LBA_TO_BYTES(x) ((int64_t)(x) * LBA_SIZE)
#endif

#include "libwbfs.h"

// WBFS magic.
static const char WBFS_MAGIC[4] = {'W','B','F','S'};

/**
 * Is a given disc image supported by the WBFS reader?
 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
 * @param size	[in] Size of sbuf. (should be 512 or larger)
 * @return True if supported; false if not.
 */
bool reader_wbfs_is_supported(const uint8_t *sbuf, size_t size)
{
	wbfs_head_t *const head = (wbfs_head_t*)sbuf;

	assert(sbuf != NULL);
	assert(size >= LBA_SIZE);
	if (!sbuf || size < LBA_SIZE) {
		return false;
	}

	// Check for WBFS magic.
	if (memcmp(&head->magic, WBFS_MAGIC, sizeof(WBFS_MAGIC)) != 0) {
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

// Functions.
static uint32_t reader_wbfs_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len);
static uint32_t reader_wbfs_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len);
static void reader_wbfs_flush(Reader *reader);
static void reader_wbfs_close(Reader *reader);

// vtable
static const Reader_Vtbl reader_wbfs_vtable = {
	reader_wbfs_read,
	reader_wbfs_write,
	reader_wbfs_flush,
	reader_wbfs_close,
};

// WbfsReader struct.
typedef struct _WbfsReader {
	Reader reader;

	// NOTE: reader.lba_len is the virtual image size.
	// real_lba_len is the actual image size.
	uint32_t real_lba_len;

	// WBFS block size, in LBAs.
	uint32_t block_size_lba;

	// WBFS structs.
	wbfs_t *m_wbfs;			// WBFS image.
	wbfs_disc_t *m_wbfs_disc;	// Current disc.

	const be16_t *wlba_table;	// Pointer to m_wbfs_disc->disc->header->wlba_table.
} WbfsReader;

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
	wbfs_head_t *head = NULL;
	wbfs_t *p = NULL;

	unsigned int hd_sec_sz;
	int ret = -1;
	size_t size;

	// Assume 512-byte sectors initially.
	hd_sec_sz = 512;
	head = malloc(hd_sec_sz);
	if (!head) {
		// ENOMEM
		goto end;
	}

	// Read the WBFS header.
	ret = ref_seeko(file, LBA_TO_BYTES(lba_start), SEEK_SET);
	if (ret != 0) {
		// Seek error.
		goto end;
	}
	size = ref_read(head, 1, hd_sec_sz, file);
	if (size != hd_sec_sz) {
		// Read error.
		ret = -1;
		goto end;
	}

	// Make sure this is a valid WBFS.
	if (!reader_wbfs_is_supported((const uint8_t*)head, hd_sec_sz)) {
		// Not a valid WBFS.
		ret = -1;
		goto end;
	}

	// wbfs_t struct.
	p = malloc(sizeof(wbfs_t));
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
		head = malloc(hd_sec_sz);
		if (!head) {
			// ENOMEM
			goto end;
		}

		// Re-read the WBFS header.
		ret = ref_seeko(file, LBA_TO_BYTES(lba_start), SEEK_SET);
		if (ret != 0) {
			// Seek error.
			goto end;
		}
		size = ref_read(head, 1, hd_sec_sz, file);
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
	p->freeblks = NULL;
	p->max_disc = (p->freeblks_lba-1) / (p->disc_info_sz >> p->hd_sec_sz_s);
	if (p->max_disc > (p->hd_sec_sz - sizeof(wbfs_head_t)))
		p->max_disc = (uint16_t)(p->hd_sec_sz - sizeof(wbfs_head_t));

	p->n_disc_open = 0;

end:
	if (ret != 0) {
		// Error...
		free(head);
		free(p);
		return NULL;
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
	assert(p != NULL);
	assert(p->head != NULL);
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

				wbfs_disc_t *disc = malloc(sizeof(wbfs_disc_t));
				if (!disc) {
					// ENOMEM
					return NULL;
				}
				disc->p = p;
				disc->i = i;

				// Read the disc header.
				disc->header = malloc(p->disc_info_sz);
				if (!disc->header) {
					// ENOMEM
					free(disc);
					return NULL;
				}

				ret = ref_seeko(file, LBA_TO_BYTES(lba_start) + p->hd_sec_sz + (i*p->disc_info_sz), SEEK_SET);
				if (ret != 0) {
					// Seek error.
					free(disc);
					return NULL;
				}
				size = ref_read(disc->header, 1, p->disc_info_sz, file);
				if (size != p->disc_info_sz) {
					// Error reading the disc information.
					free(disc->header);
					free(disc);
					return NULL;
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
	return NULL;
}

/**
 * Close a WBFS disc.
 * This frees all associated structs.
 * @param disc wbfs_disc_t.
 */
static void closeWbfsDisc(wbfs_disc_t *disc)
{
	assert(disc != NULL);
	assert(disc->p != NULL);
	assert(disc->p->n_disc_open > 0);
	assert(disc->header != NULL);

	// Free the disc.
	disc->p->n_disc_open--;
	free(disc->header);
	free(disc);
}

/**
 * Get the non-sparse size of an open WBFS disc, in bytes.
 * This scans the block table to find the first block
 * from the end of wlba_table[] that has been allocated.
 * @param wbfsReader	[in] WbfsReader*
 * @param disc		[in] wbfs_disc_t*
 * @return Non-sparse size, in bytes.
 */
static int64_t getWbfsDiscSize(const WbfsReader *wbfsReader, const wbfs_disc_t *disc)
{
	// Find the last block that's used on the disc.
	// NOTE: This is in WBFS blocks, not Wii blocks.
	const wbfs_t *const p = disc->p;
	int lastBlock = p->n_wbfs_sec_per_disc - 1;
	for (; lastBlock >= 0; lastBlock--) {
		if (wbfsReader->wlba_table[lastBlock] != cpu_to_be16(0))
			break;
	}

	// lastBlock+1 * WBFS block size == filesize.
	return (int64_t)(lastBlock + 1) * (int64_t)(p->wbfs_sec_sz);
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
 * @return Reader*, or NULL on error.
 */
Reader *reader_wbfs_open(RefFile *file, uint32_t lba_start, uint32_t lba_len)
{
	WbfsReader *wbfsReader = NULL;
	int ret;
	int err = 0;

	// Validate parameters.
	if (!file || (lba_start > 0 && lba_len == 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return NULL;
	}

	// Allocate memory for the WbfsReader object.
	wbfsReader = calloc(1, sizeof(*wbfsReader));
	if (!wbfsReader) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		return NULL;
	}

	// Duplicate the file.
	wbfsReader->reader.file = ref_dup(file);
	if (!wbfsReader->reader.file) {
		// Error duplicating the file.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto fail;
	}

	// Set the vtable.
	wbfsReader->reader.vtbl = &reader_wbfs_vtable;

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// Determine the maximum LBA.
		int64_t offset = ref_get_size(file);
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
	wbfsReader->reader.lba_start = lba_start;
	wbfsReader->reader.lba_len = 0;	// will be set after reading the WBFS header
	wbfsReader->real_lba_len = lba_len;

	// Read the WBFS header.
	wbfsReader->m_wbfs = readWbfsHeader(file, lba_start);
	if (!wbfsReader->m_wbfs) {
		// Error reading the WBFS header.
		free(wbfsReader);
		return NULL;
	}

	// Open the first disc.
	wbfsReader->m_wbfs_disc = openWbfsDisc(file, lba_start, wbfsReader->m_wbfs, 0);
	if (!wbfsReader->m_wbfs_disc) {
		// Error opening the WBFS disc.
		freeWbfsHeader(wbfsReader->m_wbfs);
		free(wbfsReader);
		return NULL;
	}

	// Save important values for later.
	wbfsReader->wlba_table = wbfsReader->m_wbfs_disc->header->wlba_table;
	wbfsReader->block_size_lba = BYTES_TO_LBA(wbfsReader->m_wbfs->wbfs_sec_sz);

	// Get the size of the WBFS disc.
	wbfsReader->reader.lba_len = BYTES_TO_LBA(getWbfsDiscSize(wbfsReader, wbfsReader->m_wbfs_disc));

	// Reader initialized.
	wbfsReader->reader.type = RVTH_ImageType_GCM;
	return (Reader*)wbfsReader;

fail:
	// Failed to initialize the reader.
	if (wbfsReader->m_wbfs_disc) {
		closeWbfsDisc(wbfsReader->m_wbfs_disc);
	}
	if (wbfsReader->m_wbfs) {
		freeWbfsHeader(wbfsReader->m_wbfs);
	}
	free(wbfsReader);
	errno = err;
	return NULL;
}

/**
 * Read data from a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[out] Read buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static uint32_t reader_wbfs_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	WbfsReader *wbfsReader = (WbfsReader*)reader;
	uint8_t *ptr8 = (uint8_t*)ptr;
	uint32_t lba;
	uint32_t lba_end;

	// Return value.
	uint32_t lbas_read = 0;
	int ret;

	// LBA bounds checking.
	// TODO: Check for overflow?
	assert(lba_start + reader->lba_start + lba_len <=
	       reader->lba_start + reader->lba_len);
	if (lba_start + reader->lba_start + lba_len >
	    reader->lba_start + reader->lba_len)
	{
		// Out of range.
		errno = EIO;
		return 0;
	}

	// TODO: Optimize reading multiple LBAs at once.
	// For now, we're looking up a single LBA at a time.
	lba_end = lba_start + lba_len;
	for (lba = lba_start; lba < lba_end; lba++, ptr8 += LBA_SIZE) {
		unsigned int physBlockIdx = be16_to_cpu(wbfsReader->wlba_table[lba / wbfsReader->block_size_lba]);
		if (physBlockIdx == 0) {
			// Empty block.
			memset(ptr8, 0, LBA_SIZE);
		} else {
			// Determine the offset.
			size_t size;
			unsigned int blockStart = physBlockIdx * wbfsReader->block_size_lba;
			unsigned int offset = lba % wbfsReader->block_size_lba;
			ret = ref_seeko(reader->file, LBA_TO_BYTES(blockStart + offset + reader->lba_start), SEEK_SET);
			if (ret != 0) {
				// Seek error.
				if (errno == 0) {
					errno = EIO;
				}
				return 0;
			}
			size = ref_read(ptr8, LBA_SIZE, 1, reader->file);
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

/**
 * Write data to a disc image.
 * @param reader	[in] Reader*
 * @param ptr		[in] Write buffer.
 * @param lba_start	[in] Starting LBA.
 * @param lba_len	[in] Length, in LBAs.
 * @return Number of LBAs read, or 0 on error.
 */
static uint32_t reader_wbfs_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// WBFS is not writable.
	UNUSED(reader);
	UNUSED(ptr);
	UNUSED(lba_start);
	UNUSED(lba_len);

	errno = EROFS;
	return 0;
}

/**
 * Flush the file buffers.
 * @param reader	[in] Reader*
 * */
static void reader_wbfs_flush(Reader *reader)
{
	// WBFS is not writable.
	UNUSED(reader);
}

/**
 * Close a disc image.
 * @param reader	[in] Reader*
 */
static void reader_wbfs_close(Reader *reader)
{
	// Free the WBFS structs.
	WbfsReader *wbfsReader = (WbfsReader*)reader;
	if (wbfsReader->m_wbfs_disc) {
		closeWbfsDisc(wbfsReader->m_wbfs_disc);
	}
	if (wbfsReader->m_wbfs) {
		freeWbfsHeader(wbfsReader->m_wbfs);
	}

	ref_close(reader->file);
	free(reader);
}
