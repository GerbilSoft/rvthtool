/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.hpp: RVT-H image handler.                                          *
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

#ifndef __RVTHTOOL_LIBRVTH_RVTH_HPP__
#define __RVTHTOOL_LIBRVTH_RVTH_HPP__

#include "libwiicrypto/common.h"
#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/wii_structs.h"

#include "tcharx.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// Enums
#include "rvth_enums.h"

// RefFile class
#ifdef __cplusplus
class RefFile;
#else
struct RefFile;
typedef struct RefFile RefFile;
#endif

// Reader class
#ifdef __cplusplus
class Reader;
#else
struct Reader;
typedef struct Reader Reader;
#endif

// RvtH forward declarations
#ifdef __cplusplus
class RvtH;
#else
struct RvtH;
typedef struct RvtH RvtH;
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/** Progress callback for write functions **/

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
 * @param state Current progress.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Progress_Callback)(const RvtH_Progress_State *state);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/** Main class **/

class RvtH {
	public:
		/**
		 * Open an RVT-H disk image.
		 * TODO: R/W mode.
		 *
		 * Check isOpen() after constructing the object to determine
		 * if the file was opened successfully.
		 *
		 * @param filename	[in] Filename.
		 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		RvtH(const TCHAR *filename, int *pErr = nullptr);

		/**
		 * Create a writable RVT-H disc image object.
		 *
		 * Check isOpen() after constructing the object to determine
		 * if the file was opened successfully.
		 *
		 * @param filename	[in] Filename.
		 * @param lba_len	[in] LBA length. (Will NOT be allocated initially.)
		 * @param pErr		[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		RvtH(const TCHAR *filename, uint32_t lba_len, int *pErr = nullptr);

		~RvtH();

	private:
		/** Constructor functions (rvth.cpp) **/

		/**
		 * Open a Wii or GameCube disc image.
		 * @param f_img	[in] RefFile*
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int openGcm(RefFile *f_img);

		/**
		 * Open an RVT-H disk image.
		 * @param f_img	[in] RefFile*
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int openHDD(RefFile *f_img);

	public:
		/** General utility functions. **/
		// TODO: Move out of RvtH?
		/**
		 * Check if a block is empty.
		 * @param block Block.
		 * @param size Block size. (Must be a multiple of 64 bytes.)
		 * @return True if the block is all zeroes; false if not.
		 */
		static bool isBlockEmpty(const uint8_t *block, unsigned int size);

	private:
		/** Private functions (rvth_p.cpp) **/

		/**
		 * Make the RVT-H object writable.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int makeWritable(void);

		/**
		 * Write a bank table entry to disk.
		 * @param bank	[in] Bank number. (0-7)
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int writeBankEntry(unsigned int bank);

	private:
		DISABLE_COPY(RvtH)

	public:
		/**
		 * Is the file open?
		 * @return True if open; false if not.
		 */
		inline bool isOpen(void) const
		{
			return (m_file != nullptr);
		}

	public:
		/** Accessors **/

		/**
		 * Get the number of banks in the RVT-H disk image.
		 * @return Number of banks.
		 */
		inline unsigned int bankCount(void) const { return m_bankCount; }

		/**
		 * Is this RVT-H object an HDD image or a standalone disc image?
		 * @param rvth RVT-H object.
		 * @return True if the RVT-H object is an HDD image; false if it's a standalone disc image.
		 */
		bool isHDD(void) const;

		/**
		 * Get the RVT-H image type.
		 * @param rvth RVT-H object.
		 * @return RVT-H image type.
		 */
		RvtH_ImageType_e imageType(void) const { return m_imageType; }

		/**
		 * Is an NHCD table present?
		 *
		 * This is always false for optical disc images,
		 * and false for wiped RVT-H devices and disk images.
		 *
		 * @return True if the RVT-H object has an NHCD table; false if not.
		 */
		inline bool hasNHCD(void) const { return m_has_NHCD; }

		/**
		 * Get a bank table entry.
		 * @param bank	[in] Bank number. (0-7)
		 * @param pErr	[out,opt] Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 * @return Bank table entry.
		 */
		const RvtH_BankEntry *bankEntry(unsigned int bank, int *pErr = nullptr) const;

	public:
		/** Write functions (write.cpp) **/

		/**
		 * Delete a bank on an RVT-H device.
		 * @param bank	[in] Bank number. (0-7)
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int deleteBank(unsigned int bank);

		/**
		 * Undelete a bank on an RVT-H device.
		 * @param bank	[in] Bank number. (0-7)
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int undeleteBank(unsigned int bank);

	public:
		/** Extract functions (extract.cpp, extract_crypt.cpp) **/

		/**
		 * Copy a bank from this RVT-H HDD or standalone disc image to a writable standalone disc image.
		 * @param rvth_dest	[out] Destination RvtH object.
		 * @param bank_src	[in] Source bank number. (0-7)
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int copyToGcm(RvtH *rvth_dest, unsigned int bank_src,
			RvtH_Progress_Callback callback = nullptr);

		/**
		 * Copy a bank from this RVT-H HDD or standalone disc image to a writable standalone disc image.
		 *
		 * This function copies an unencrypted Game Partition and encrypts it
		 * using the existing title key. It does *not* change the encryption
		 * method or signature, so recryption will be needed afterwards.
		 *
		 * @param rvth_dest	[out] Destination RvtH object.
		 * @param bank_src	[in] Source bank number. (0-7)
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int copyToGcm_doCrypt(RvtH *rvth_dest, unsigned int bank_src,
			RvtH_Progress_Callback callback = nullptr);

		/**
		 * Extract a disc image from this RVT-H disk image.
		 * Compatibility wrapper; this function creates a new RvtH
		 * using the GCM constructor and then copyToGcm().
		 * @param bank		[in] Bank number. (0-7)
		 * @param filename	[in] Destination filename.
		 * @param recrypt_key	[in] Key for recryption. (-1 for default; otherwise, see RvtH_CryptoType_e)
		 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int extract(unsigned int bank, const TCHAR *filename,
			int recrypt_key, uint32_t flags,
			RvtH_Progress_Callback callback = nullptr);

		/**
		 * Copy a bank from this HDD or standalone disc image to an RVT-H system.
		 * @param rvth_dest	[in] Destination RvtH object.
		 * @param bank_dest	[in] Destination bank number. (0-7)
		 * @param bank_src	[in] Source bank number. (0-7)
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int copyToHDD(RvtH *rvth_dest, unsigned int bank_dest,
			unsigned int bank_src,
			RvtH_Progress_Callback callback = nullptr);

		/**
		 * Import a disc image into this RVT-H disk image.
		 * Compatibility wrapper; this function creates an RvtH object for the
		 * RVT-H disk image and then copyToHDD().
		 * @param bank		[in] Bank number. (0-7)
		 * @param filename	[in] Source GCM filename.
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int import(unsigned int bank, const TCHAR *filename,
			RvtH_Progress_Callback callback = nullptr);

	public:
		/** Recryption functions (recrypt.cpp) **/

		int recryptID(unsigned int bank);

		/**
		 * Re-encrypt partitions in a Wii disc image.
		 *
		 * This operation will *wipe* the update partition, since installing
		 * retail updates on a debug system and vice-versa can result in a brick.
		 *
		 * NOTE: This function only supports converting from one encryption to
		 * another. It does not support converting unencrypted to encrypted or
		 * vice-versa.
		 *
		 * NOTE 2: Any partitions that are already encrypted with the specified key
		 * will be left as-is; however, the tickets and TMDs wlil be re-signed.
		 *
		 * @param bank		[in] Bank number. (0-7)
		 * @param cryptoType	[in] New encryption type.
		 * @param callback	[in,opt] Progress callback.
		 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
		 */
		int recryptWiiPartitions(unsigned int bank,
			RvtH_CryptoType_e cryptoType,
			RvtH_Progress_Callback callback);
		
	private:
		// Reference-counted FILE*.
		RefFile *m_file;

		// Number of banks.
		// - RVT-H system or disk image: 8
		// - Standalone disc image: 1
		unsigned int m_bankCount;

		// Image type.
		RvtH_ImageType_e m_imageType;

		// Is the NHCD header present?
		bool m_has_NHCD;

		// BankEntry objects.
		RvtH_BankEntry *m_entries;
};

#endif /* __cplusplus */

#endif /* __RVTHTOOL_LIBRVTH_RVTH_HPP__ */
