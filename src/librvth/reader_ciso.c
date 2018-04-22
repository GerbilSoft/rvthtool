/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * reader_ciso.c: CISO disc image reader class.                            *
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

#include "reader_ciso.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef LBA_TO_BYTES
# define LBA_TO_BYTES(x) ((int64_t)(x) * LBA_SIZE)
#endif

// CISO magic.
static const char CISO_MAGIC[4] = {'C','I','S','O'};

#define CISO_HEADER_SIZE 0x8000
#define CISO_MAP_SIZE (CISO_HEADER_SIZE - sizeof(uint32_t) - (sizeof(char) * 4))

// 32 KB minimum block size (GCN/Wii sector)
// 16 MB maximum block size
#define CISO_BLOCK_SIZE_MIN (32768)
#define CISO_BLOCK_SIZE_MAX (16*1024*1024)

typedef struct PACKED _CisoHeader {
	char magic[4];			// "CISO"
	uint32_t block_size;		// LE32
	uint8_t map[CISO_MAP_SIZE];	// 0 == unused; 1 == used; other == invalid
} CisoHeader;

/**
 * Is a given disc image supported by the CISO reader?
 * @param sbuf	[in] Sector buffer. (first LBA of the disc)
 * @param size	[in] Size of sbuf. (should be 512 or larger)
 * @return True if supported; false if not.
 */
bool reader_ciso_is_supported(const uint8_t *sbuf, size_t size)
{
	unsigned int block_size;
	unsigned int i;
	bool isPow2 = false;

	assert(sbuf != NULL);
	assert(size >= LBA_SIZE);
	if (!sbuf || size < LBA_SIZE) {
		return false;
	}

	// Check for CISO magic.
	if (memcmp(sbuf, CISO_MAGIC, sizeof(CISO_MAGIC)) != 0) {
		// Invalid magic.
		return false;
	}

	// Check if the block size is a supported power of two.
	// - Minimum: CISO_BLOCK_SIZE_MIN (32 KB, 1 << 15)
	// - Maximum: CISO_BLOCK_SIZE_MAX (16 MB, 1 << 24)
	block_size = sbuf[4] | (sbuf[5] << 8) | (sbuf[6] << 16) | (sbuf[7] << 24);
	for (i = 15; i <= 24; i++) {
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

// Functions.
static uint32_t reader_ciso_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len);
static uint32_t reader_ciso_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len);
static void reader_ciso_flush(Reader *reader);
static void reader_ciso_close(Reader *reader);

// vtable
static const Reader_Vtbl reader_ciso_vtable = {
	reader_ciso_read,
	reader_ciso_write,
	reader_ciso_flush,
	reader_ciso_close,
};

// CisoReader struct.
typedef struct _CisoReader {
	Reader reader;

	// NOTE: reader.lba_len is the virtual image size.
	// real_lba_len is the actual image size.
	uint32_t real_lba_len;

	// CISO block size, in LBAs.
	uint32_t block_size_lba;

	// Block map.
	// 0x0000 == first block after CISO header.
	// 0xFFFF == empty block.
	uint16_t blockMap[CISO_MAP_SIZE];
} CisoReader;

/**
 * Create a CISO reader for a disc image.
 *
 * NOTE: If lba_start == 0 and lba_len == 0, the entire file
 * will be used.
 *
 * @param file		RefFile*.
 * @param lba_start	[in] Starting LBA,
 * @param lba_len	[in] Length, in LBAs.
 * @return Reader*, or NULL on error.
 */
Reader *reader_ciso_open(RefFile *file, uint32_t lba_start, uint32_t lba_len)
{
	CisoReader *cisoReader = NULL;
	CisoHeader *cisoHeader = NULL;
	int ret;
	int err = 0;
	size_t size;
	unsigned int i;
	uint16_t physBlockIdx = 0;
	uint16_t maxLogicalBlockUsed = 0;

	// Validate parameters.
	if (!file || (lba_start > 0 && lba_len == 0)) {
		// Invalid parameters.
		errno = EINVAL;
		return NULL;
	}

	// Allocate memory for the CisoReader and CisoHeader objects.
	cisoReader = malloc(sizeof(*cisoReader));
	if (!cisoReader) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		return NULL;
	}
	cisoHeader = malloc(sizeof(*cisoHeader));
	if (!cisoHeader) {
		// Error allocating memory.
		if (errno == 0) {
			errno = ENOMEM;
		}
		free(cisoReader);
		return NULL;
	}

	// Duplicate the file.
	cisoReader->reader.file = ref_dup(file);
	if (!cisoReader->reader.file) {
		// Error duplicating the file.
		err = errno;
		if (err == 0) {
			err = ENOMEM;
		}
		goto fail;
	}

	// Set the vtable.
	cisoReader->reader.vtbl = &reader_ciso_vtable;

	// Set the LBAs.
	if (lba_start == 0 && lba_len == 0) {
		// Determine the maximum LBA.
		int64_t offset;
		ret = ref_seeko(file, 0, SEEK_END);
		if (ret != 0) {
			// Seek error.
			err = errno;
			if (err == 0) {
				err = EIO;
			}
			goto fail;
		}
		offset = ref_tello(file);

		// NOTE: If not a multiple of the LBA size,
		// the partial LBA will be ignored.
		lba_len = (uint32_t)(offset / LBA_SIZE);
	}
	cisoReader->reader.lba_start = lba_start + BYTES_TO_LBA(sizeof(*cisoHeader));
	cisoReader->reader.lba_len = 0;	// will be set after reading the CISO header
	cisoReader->real_lba_len = lba_len;

	// Read the CISO header.
	ret = ref_seeko(file, lba_start * LBA_SIZE, SEEK_SET);
	if (ret != 0) {
		// Seek error.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}
	size = ref_read(cisoHeader, 1, sizeof(*cisoHeader), cisoReader->reader.file);
	if (size != sizeof(*cisoHeader)) {
		// Short read.
		err = errno;
		if (err == 0) {
			err = EIO;
		}
		goto fail;
	}

	// Validate the CISO header.
	if (!reader_ciso_is_supported((const uint8_t*)cisoHeader, LBA_SIZE)) {
		// Not a valid CISO header.
		return false;
	}
	cisoReader->block_size_lba = BYTES_TO_LBA(le32_to_cpu(cisoHeader->block_size));

	// Clear the CISO block map initially.
	memset(cisoReader->blockMap, 0xFF, sizeof(cisoReader->blockMap));

	// Parse the CISO block map.
	for (i = 0; i < ARRAY_SIZE(cisoReader->blockMap); i++) {
		switch (cisoHeader->map[i]) {
			case 0:
				// Empty block.
				continue;
			case 1:
				// Used block.
				cisoReader->blockMap[i] = physBlockIdx;
				physBlockIdx++;
				maxLogicalBlockUsed = i;
				break;
			default:
				// Invalid entry.
				goto fail;
		}
	}

	// Calculate the image size based on the highest logical block index.
	cisoReader->reader.lba_len =
		(uint32_t)(maxLogicalBlockUsed + 1) * cisoReader->block_size_lba;

	// Reader initialized.
	free(cisoHeader);
	return (Reader*)cisoReader;

fail:
	// Failed to initialize the reader.
	if (cisoReader->reader.file) {
		ref_close(cisoReader->reader.file);
	}
	free(cisoReader);
	free(cisoHeader);
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
static uint32_t reader_ciso_read(Reader *reader, void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	CisoReader *cisoReader = (CisoReader*)reader;
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
		unsigned int physBlockIdx = cisoReader->blockMap[lba / cisoReader->block_size_lba];
		if (physBlockIdx == 0xFFFF) {
			// Empty block.
			memset(ptr8, 0, LBA_SIZE);
		} else {
			// Determine the offset.
			size_t size;
			unsigned int offset = lba % cisoReader->block_size_lba;
			ret = ref_seeko(reader->file, LBA_TO_BYTES(lba + offset + reader->lba_start), SEEK_SET);
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
static uint32_t reader_ciso_write(Reader *reader, const void *ptr, uint32_t lba_start, uint32_t lba_len)
{
	// CISO is not writable.
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
static void reader_ciso_flush(Reader *reader)
{
	// CISO is not writable.
	UNUSED(reader);
}

/**
 * Close a disc image.
 * @param reader	[in] Reader*
 */
static void reader_ciso_close(Reader *reader)
{
	ref_close(reader->file);
	free(reader);
}
