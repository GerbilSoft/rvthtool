/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * bank_init.h: RvtH_BankEntry initialization functions.                   *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_LIBRVTH_BANK_INIT_H__
#define __RVTHTOOL_LIBRVTH_BANK_INIT_H__

#include "rvth.hpp"

// RefFile class
#ifdef __cplusplus
class RefFile;
#else
struct RefFile;
typedef struct RefFile RefFile;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set the region field in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_region(RvtH_BankEntry *entry);

/**
 * Set the crypto_type and sig_type fields in an RvtH_BankEntry.
 * The reader and discHeader fields must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_crypto(RvtH_BankEntry *entry);

/**
 * Set the apploader_error field in an RvtH_BankEntry.
 * The reader field must have already been set.
 * @param entry		[in,out] RvtH_BankEntry
 * @return Error code. (If negative, POSIX error; otherwise, see RvtH_Errors.)
 */
int rvth_init_BankEntry_AppLoader(RvtH_BankEntry *entry);

/**
 * Initialize an RVT-H bank entry from an opened HDD image.
 * @param entry			[out] RvtH_BankEntry
 * @param f_img			[in] RefFile*
 * @param type			[in] Bank type. (See RvtH_BankType_e.)
 * @param lba_start		[in] Starting LBA.
 * @param lba_len		[in] Length, in LBAs.
 * @param nhcd_timestamp	[in] Timestamp string pointer from the bank table.
 * @return 0 on success; negative POSIX error code on error.
 */
int rvth_init_BankEntry(RvtH_BankEntry *entry, RefFile *f_img,
	uint8_t type, uint32_t lba_start, uint32_t lba_len,
	const char *nhcd_timestamp);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_BANK_INIT_H__ */
