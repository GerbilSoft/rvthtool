/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth.hpp: RVT-H image handler.                                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_LIBRVTH_RVTH_HPP__
#define __RVTHTOOL_LIBRVTH_RVTH_HPP__

#include "libwiicrypto/common.h"
#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/wii_structs.h"
#include "libwiicrypto/sig_tools.h"

#include "stdboolx.h"
#include "tcharx.h"

#include <stdint.h>
#include <stdio.h>
#include "time_r.h"

// Enums
#include "rvth_enums.h"
#include "nhcd_structs.h"

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
	uint8_t sig_type;	// Signature type. (See RVL_SigType_e.)
	uint8_t sig_status;	// Signature status. (See RVL_SigStatus_e.)
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
	uint8_t crypto_type;	// Encryption type. (See RVL_CryptoType_e.)
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

// General progress callback status.
typedef struct _RvtH_Progress_State {
	// RvtH objects.
	const RvtH *rvth;	// Primary RvtH.
	const RvtH *rvth_gcm;	// GCM being extracted or imported, if not NULL.

	// Bank numbers.
	unsigned int bank_rvth;	// Bank number in `rvth`.
	unsigned int bank_gcm;	// Bank number in `rvth_gcm`. (UINT_MAX if none)

	// Progress type.
	RvtH_Progress_Type type;

	// Progress.
	// If RVTH_PROGRESS_RECRYPT and lba_total == 1,
	// we're only changing the ticket and TMD.
	// (lba_processed == 0 when starting, == 1 when done.)
	// Otherwise, we're encrypting/decrypting.
	uint32_t lba_processed;
	uint32_t lba_total;
} RvtH_Progress_State;

/**
 * General progress callback.
 * @param state		[in] Current progress.
 * @param userdata	[in] User data specified when calling the RVT-H function.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Progress_Callback)(const RvtH_Progress_State *state, void *userdata);

// Verify progress callback type.
// NOTE: This indicates the message type, whereas the
// regular progress type indicates the operation type.
typedef enum {
	RVTH_VERIFY_UNKNOWN = 0,
	RVTH_VERIFY_STATUS,		// Current status
	RVTH_VERIFY_ERROR_REPORT,	// Reporting an error
} RvtH_Verify_Progress_Type;

typedef enum {
	RVTH_VERIFY_ERROR_UNKNOWN = 0,
	RVTH_VERIFY_ERROR_BAD_HASH,	// SHA-1 hash is incorrect
	RVTH_VERIFY_ERROR_TABLE_COPY,	// Sector's hash table copy doesn't match base sector.
} RvtH_Verify_Error_Type;

// Verification progress callback status.
typedef struct _RvtH_Verify_Progress_State {
	const RvtH *rvth;
	unsigned int bank;

	// NOTE: Ignoring volume groups here.
	// Using a total partition count.
	uint32_t pt_type;	// Current partition type.
	uint8_t pt_current;	// Current partition number being verified.
	uint8_t pt_total;	// Total number of partitions.

	// Current partition
	unsigned int group_cur;		// Current 2 MB group index
	unsigned int group_total;	// Total number of 2 MB groups

	// Progress type.
	RvtH_Verify_Progress_Type type;

	// If RVTH_VERIFY_ERROR_REPORT, what type of error?
	uint8_t hash_level;	// 0, 1, 2, 3, 4
	uint8_t sector;		// Sector 0-63 in the current group
	uint8_t kb;		// Kilobyte 1-31 (H0 only) [KB 0 == hashes]
	uint8_t err_type;	// Error type (see RvtH_Verify_Error_Type)
	bool is_zero;		// If true, sector is zeroed. (scrubbed/truncated)
} RvtH_Verify_Progress_State;

/**
 * Verify progress callback.
 * @param state		[in] Current progress.
 * @param userdata	[in] User data specified when calling the RVT-H function.
 * @return True to continue; false to abort.
 */
typedef bool (*RvtH_Verify_Progress_Callback)(const RvtH_Verify_Progress_State *state, void *userdata);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// C++ STL classes
#include <memory>
#include <vector>

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
	 * Check for MBR and/or GPT.
	 * @param f_img	[in] RefFile*
	 * @param pMBR	[out] True if the HDD has an MBR.
	 * @param pGPT	[out] True if the HDD has a GPT.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	static int checkMBR(RefFile *f_img, bool *pMBR, bool *pGPT);

	/**
	 * Open an RVT-H disk image.
	 * @param f_img	[in] RefFile*
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int openHDD(RefFile *f_img);

public:
	/** General utility functions **/
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
	 * @param bank		[in] Bank number. (0-7)
	 * @param pTimestamp	[out,opt] Timestamp written to the bank entry.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int writeBankEntry(unsigned int bank, time_t *pTimestamp = nullptr);

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
	inline unsigned int bankCount(void) const
	{
		return static_cast<unsigned int>(m_entries.size());
	}

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
	inline RvtH_ImageType_e imageType(void) const
	{
		return m_imageType;
	}

	/**
	 * Get the NHCD table status.
	 *
	 * For optical disc images, this is always NHCD_STATUS_MISSING.
	 * For HDD images, this should be NHCD_STATUS_OK unless the
	 * NHCD table was wiped or a PC-partitioned disk was selected.
	 *
	 * @return NHCD table status.
	 */
	inline NHCD_Status_e nhcd_status(void) const
	{
		return m_NHCD_status;
	}

	/**
	 * Get the NHCD Table Header.
	 *
	 * For any non HDD images this will always be a NULL pointer.
	 * For HDD images this will be the raw bytes of the NHCD Bank Table Header.
	 *
	 * NOTE: the NHCD table may be wiped, and it may contain invalid, or
	 * unexpected values.
	 *
	 * @return NHCD Bank Table Header.
	 */
	NHCD_BankTable_Header *nhcd_header(void) const;

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
	 * @param userdata	[in,opt] User data for progress callback.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int copyToGcm(RvtH *rvth_dest, unsigned int bank_src,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr);

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
	 * @param userdata	[in,opt] User data for progress callback.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int copyToGcm_doCrypt(RvtH *rvth_dest, unsigned int bank_src,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr);

	/**
	 * Extract a disc image from this RVT-H disk image.
	 * Compatibility wrapper; this function creates a new RvtH
	 * using the GCM constructor and then copyToGcm().
	 * @param bank		[in] Bank number. (0-7)
	 * @param filename	[in] Destination filename.
	 * @param recrypt_key	[in] Key for recryption. (-1 for default; otherwise, see RVL_CryptoType_e)
	 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
	 * @param callback	[in,opt] Progress callback.
	 * @param userdata	[in,opt] User data for progress callback.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int extract(unsigned int bank, const TCHAR *filename,
		int recrypt_key, unsigned int flags,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr);

	/**
	 * Copy a bank from this HDD or standalone disc image to an RVT-H system.
	 * @param rvth_dest	[in] Destination RvtH object.
	 * @param bank_dest	[in] Destination bank number. (0-7)
	 * @param bank_src	[in] Source bank number. (0-7)
	 * @param callback	[in,opt] Progress callback.
	 * @param userdata	[in,opt] User data for progress callback.
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int copyToHDD(RvtH *rvth_dest, unsigned int bank_dest,
		unsigned int bank_src,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr);

	/**
	 * Import a disc image into this RVT-H disk image.
	 * Compatibility wrapper; this function creates an RvtH object for the
	 * RVT-H disk image and then copyToHDD().
	 * @param bank		[in] Bank number. (0-7)
	 * @param filename	[in] Source GCM filename.
	 * @param callback	[in,opt] Progress callback.
	 * @param userdata	[in,opt] User data for progress callback.
	 * @param ios_force	[in,opt] IOS version to force. (-1 to use the existing IOS)
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int import(unsigned int bank, const TCHAR *filename,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr,
		int ios_force = -1);

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
	 * @param userdata	[in,opt] User data for progress callback.
	 * @param ios_force	[in,opt] IOS version to force. (-1 to use the existing IOS)
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int recryptWiiPartitions(unsigned int bank,
		RVL_CryptoType_e cryptoType,
		RvtH_Progress_Callback callback = nullptr,
		void *userdata = nullptr,
		int ios_force = -1);

public:
	/** Verification functions (verify.cpp) **/

	/**
	 * Verify partitions in a Wii disc image.
	 *
	 * NOTE: This function only supports encrypted Wii disc images,
	 * either retail or debug encryption.
	 *
	 * This will check all five levels of hashes:
	 * - H4: Hash of H3 table (stored in the TMD)
	 * - H3: Hash of all H2 tables (H2 table = 2 MB group)
	 * - H2: Hash of all H1 tables (H1 table = 256 KB subgroup)
	 * - H1: Hash of all H0 tables (H0 table = 32 KB block)
	 * - H0: Hash of a single KB   (H0 = 1 KB)
	 *
	 * NOTE: Assuming the TMD signature is valid, which means
	 * the H4 hash is correct.
	 *
	 * @param bank		[in] Bank number (0-7)
	 * @param errors	[out] Error counts for all 5 hash tables
	 * @param callback	[in,opt] Progress callback
	 * @param userdata	[in,opt] User data for progress callback
	 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
	 */
	int verifyWiiPartitions(unsigned int bank,
		unsigned int error_count[5] = nullptr,
		RvtH_Verify_Progress_Callback callback = nullptr,
		void *userdata = nullptr);

private:
	// Reference-counted FILE*
	RefFile *m_file;

	// Image type
	RvtH_ImageType_e m_imageType;

	// NHCD header status
	NHCD_Status_e m_NHCD_status;

	// NHCD bank table header
	// NOTE: This will be nullptr for e.g. GCM disc images.
	std::unique_ptr<NHCD_BankTable_Header> m_nhcdHeader;

	// BankEntry objects
	// - RVT-H system or disk image: 8 (usually)
	// - Standalone disc image: 1
	std::vector<RvtH_BankEntry> m_entries;
};

#endif /* __cplusplus */

#endif /* __RVTHTOOL_LIBRVTH_RVTH_HPP__ */
