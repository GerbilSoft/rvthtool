/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.h: RVT-H image handler.                                            *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_H__
#define __RVTHTOOL_LIBRVTH_RVTH_H__

#include "libwiicrypto/common.h"
#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/wii_structs.h"

#include "tcharx.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// Image types
#include "rvth_imagetype.h"

// Reader class
#ifdef __cplusplus
class Reader;
#else
struct Reader;
typedef struct Reader Reader;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RVTH_BANK_COUNT 8	/* Standard RVT-H HDD bank count. */
#define RVTH_BLOCK_SIZE 512

// RVT-H struct.
struct _RvtH;
typedef struct _RvtH RvtH;

// Wii signature information.
typedef struct _RvtH_SigInfo {
	uint8_t sig_type;	// Signature type. (See RvtH_SigType_e.)
	uint8_t sig_status;	// Signature status. (See RvtH_SigStatus_e.)
} RvtH_SigInfo;

// Disc image and RVT-H bank entry definition.
struct _pt_entry_t;
typedef struct _RvtH_BankEntry {
	Reader *reader;		// Disc image reader.
	uint32_t lba_start;	// Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// Length, in 512-byte sectors.
	time_t timestamp;	// Timestamp. (no timezone information)
	uint8_t type;		// Bank type. (See RvtH_BankType_e.)
	uint8_t region_code;	// Region code. (See GCN_Region_Code.)
	bool is_deleted;	// If true, this entry was deleted.

	uint8_t aplerr;		// AppLoader error. (See AppLoader_Error_e.)
	uint32_t aplerr_val[3];	// AppLoader values.

	// Disc header.
	GCN_DiscHeader discHeader;

	// Wii-specific
	uint8_t crypto_type;	// Encryption type. (See RvtH_CryptoType_e.)
	uint8_t ios_version;	// IOS version. (0 == ERROR)
	RvtH_SigInfo ticket;	// Ticket encryption/signature.
	RvtH_SigInfo tmd;	// TMD encryption/signature.

	// Wii partition table
	RVL_VolumeGroupTable vg_orig;	// Original volume group table, in host-endian.
	unsigned int pt_count;		// Number of entries in ptbl.
	struct _pt_entry_t *ptbl;	// Partition table.
} RvtH_BankEntry;

// RVT-H bank types.
typedef enum {
	RVTH_BankType_Empty		= 0,	// Magic is 0.
	RVTH_BankType_Unknown		= 1,	// Unknown magic.
	RVTH_BankType_GCN		= 2,
	RVTH_BankType_Wii_SL		= 3,
	RVTH_BankType_Wii_DL		= 4,
	RVTH_BankType_Wii_DL_Bank2	= 5,	// Bank 2 for DL images.

	RVTH_BankType_MAX
} RvtH_BankType_e;

// Encryption types.
typedef enum {
	RVTH_CryptoType_Unknown	= 0,	// Unknown encryption.
	RVTH_CryptoType_None	= 1,	// No encryption.
	RVTH_CryptoType_Debug	= 2,	// RVT-R encryption.
	RVTH_CryptoType_Retail	= 3,	// Retail encryption.
	RVTH_CryptoType_Korean	= 4,	// Korean retail encryption.

	RVTH_CryptoType_MAX
} RvtH_CryptoType_e;

// Signature types.
typedef enum {
	RVTH_SigType_Unknown	= 0,	// Unknown signature.
	RVTH_SigType_Debug	= 1,	// Debug signature.
	RVTH_SigType_Retail	= 2,	// Retail signature.

	RVTH_SigType_MAX
} RvtH_SigType_e;

// Signature status.
typedef enum {
	RVTH_SigStatus_Unknown	= 0,	// Unknown signature status.
	RVTH_SigStatus_OK	= 1,	// Valid
	RVTH_SigStatus_Invalid	= 2,	// Invalid
	RVTH_SigStatus_Fake	= 3,	// Fakesigned

	RVTH_SigStatus_MAX
} RvtH_SigStatus_e;

/** Functions. **/

/**
 * Open an RVT-H disk image.
 * TODO: R/W mode.
 * @param filename	[in] Filename.
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH struct pointer if the file is a valid RvtH disk image; NULL on error. (check errno)
 */
RvtH *rvth_open(const TCHAR *filename, int *pErr);

/**
 * Close an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 */
void rvth_close(RvtH *rvth);

/**
 * Get the number of banks in an opened RVT-H disk image.
 * @param rvth RVT-H disk image.
 * @return Number of banks.
 */
unsigned int rvth_get_BankCount(const RvtH *rvth);

/**
 * Is this RVT-H object an HDD image or a standalone disc image?
 * @param rvth RVT-H object.
 * @return True if the RVT-H object is an HDD image; false if it's a standalone disc image.
 */
bool rvth_is_hdd(const RvtH *rvth);

/**
 * Is an NHCD table present?
 *
 * This is always false for disc images,
 * and false for wiped RVT-H devices and disk images.
 *
 * @param rvth RVT-H object.
 * @return True if the RVT-H object has an NHCD table; false if not.
 */
bool rvth_has_NHCD(const RvtH *rvth);

/**
 * Get the RVT-H image type.
 * @param rvth RVT-H object.
 * @return RVT-H image type.
 */
RvtH_ImageType_e rvth_get_ImageType(const RvtH *rvth);

/**
 * Get a bank table entry.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return Bank table entry.
 */
const RvtH_BankEntry *rvth_get_BankEntry(const RvtH *rvth, unsigned int bank, int *pErr);

/** rvth_write.c **/

// Progress callback type.
typedef enum {
	RVTH_PROGRESS_UNKNOWN	= 0,
	RVTH_PROGRESS_EXTRACT,		// Extract image
	RVTH_PROGRESS_IMPORT,		// Import image
	RVTH_PROGRESS_RECRYPT,		// Recrypt image
} RvtH_Progress_Type;

// Progress callback status.
typedef struct _RvtH_Progress_State {
	RvtH_Progress_Type type;

	// RvtH objects.
	const RvtH *rvth;	// Primary RvtH.
	const RvtH *rvth_gcm;	// GCM being extracted or imported, if not NULL.

	// Bank numbers.
	unsigned int bank_rvth;	// Bank number in `rvth`.
	unsigned int bank_gcm;	// Bank number in `rvth_gcm`. (UINT_MAX if none)

	// Progress.
	// If RVTH_PROGRESS_RECRYPT and lba_total == 1,
	// we're only changing the ticket and TMD.
	// (lba_processed == 0 when starting, == 1 when done.)
	// Otherwise, we're encrypting/decrypting.
	uint32_t lba_processed;
	uint32_t lba_total;
} RvtH_Progress_State;

/**
 * RVT-H progress callback.
 * @param data Callback data.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Progress_Callback)(const RvtH_Progress_State *state);

/**
 * Delete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_delete(RvtH *rvth, unsigned int bank);

/**
 * Undelete a bank on an RVT-H device.
 * @param rvth	[in] RVT-H device.
 * @param bank	[in] Bank number. (0-7)
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_undelete(RvtH *rvth, unsigned int bank);

/** rvth_extract.c **/

/**
 * Create a writable disc image object.
 * @param filename	[in] Filename.
 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 * @return RvtH on success; NULL on error. (check errno)
 */
RvtH *rvth_create_gcm(const TCHAR *filename, uint32_t lba_len, int *pErr);

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to a writable standalone disc image.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_gcm(RvtH *rvth_dest, const RvtH *rvth_src, unsigned int bank_src, RvtH_Progress_Callback callback);

/**
 * RVT-H extraction flags.
 */
typedef enum {
	// Prepend a 32k SDK header.
	// Required for rvtwriter, NDEV ODEM, etc.
	RVTH_EXTRACT_PREPEND_SDK_HEADER		= (1 << 0),
} RvtH_Extract_Flags;

/**
 * Extract a disc image from the RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_create_gcm() and rvth_copy().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Destination filename.
 * @param recrypt_key	[in] Key for recryption. (-1 for default; otherwise, see RvtH_CryptoType_e)
 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_extract(const RvtH *rvth, unsigned int bank, const TCHAR *filename,
	int recrypt_key, uint32_t flags, RvtH_Progress_Callback callback);

/**
 * Copy a bank from an RVT-H HDD or standalone disc image to an RVT-H system.
 * @param rvth_dest	[out] Destination RvtH object.
 * @param bank_dest	[out] Destination bank number. (0-7)
 * @param rvth_src	[in] Source RvtH object.
 * @param bank_src	[in] Source bank number. (0-7)
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_copy_to_hdd(RvtH *rvth_dest, unsigned int bank_dest, const RvtH *rvth_src,
	unsigned int bank_src, RvtH_Progress_Callback callback);

/**
 * Import a disc image into an RVT-H disk image.
 * Compatibility wrapper; this function calls rvth_open() and rvth_copy_to_hdd().
 * @param rvth		[in] RVT-H disk image.
 * @param bank		[in] Bank number. (0-7)
 * @param filename	[in] Source GCM filename.
 * @param callback	[in,opt] Progress callback.
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_import(RvtH *rvth, unsigned int bank, const TCHAR *filename, RvtH_Progress_Callback callback);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_RVTH_H__ */
