/***************************************************************************
 * RVT-H Tool                                                              *
 * extract.cpp: Extract/import a bank from/to an RVT-H disk image.         *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "extract.h"
#include "list-banks.hpp"

#include "librvth/rvth.hpp"
#include "librvth/rvth_error.h"
#include "librvth/nhcd_structs.h"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstdlib>

/**
 * RVT-H progress callback.
 * @param state		[in] Current progress.
 * @param userdata	[in] User data specified when calling the RVT-H function.
 * @return True to continue; false to abort.
 */
static bool progress_callback(const RvtH_Progress_State *state, void *userdata)
{
	UNUSED(userdata);

	#define MEGABYTE (1048576 / LBA_SIZE)
	switch (state->type) {
		case RVTH_PROGRESS_EXTRACT:
			printf("\rExtracting: %4u MiB / %4u MiB copied...",
				state->lba_processed / MEGABYTE,
				state->lba_total / MEGABYTE);
			break;
		case RVTH_PROGRESS_IMPORT:
			printf("\rImporting: %4u MiB / %4u MiB copied...",
				state->lba_processed / MEGABYTE,
				state->lba_total / MEGABYTE);
			break;
		case RVTH_PROGRESS_RECRYPT:
			if (state->lba_total <= 1) {
				// TODO: Encryption types?
				if (state->lba_processed == 0) {
					printf("\rRecrypting the ticket(s) and TMD(s)...");
				}
			} else {
				// TODO: This doesn't seem to be used yet...
				printf("\rRecrypting: %4u MiB / %4u MiB processed...",
					state->lba_processed / MEGABYTE,
					state->lba_total / MEGABYTE);
			}
			break;
		default:
			// FIXME
			assert(false);
			return false;
	}

	if (state->lba_processed == state->lba_total) {
		// Finished processing.
		putchar('\n');
	}
	fflush(stdout);
	return true;
}

/**
 * 'extract' command.
 * @param rvth_filename	[in] RVT-H device or disk image filename.
 * @param s_bank	[in] Bank number (as a string). (If NULL, assumes bank 1.)
 * @param gcm_filename	[in] Filename for the extracted GCM image.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @param flags		[in] Flags. (See RvtH_Extract_Flags.)
 * @return 0 on success; non-zero on error.
 */
int extract(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename, int recrypt_key, unsigned int flags)
{
	// Open the RVT-H device or disk image.
	int ret;
	RvtH *const rvth = new RvtH(rvth_filename, &ret);
	if (ret != 0 || !rvth->isOpen()) {
		_ftprintf(stderr, _T("*** ERROR opening RVT-H device '%s': "), rvth_filename);
		fputs(rvth_error(ret), stderr);
		_fputtc(_T('\n'), stderr);
		delete rvth;
		return ret;
	}

	unsigned int bank;
	if (s_bank) {
		// Validate the bank number.
		TCHAR *endptr;
		bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
		if (*endptr != 0 || bank > rvth->bankCount()) {
			_ftprintf(stderr, _T("*** ERROR: Invalid bank number '%s'.\n"), s_bank);
			delete rvth;
			return -EINVAL;
		}
	} else {
		// No bank number specified.
		// Assume 1 bank if this is a standalone disc image.
		// For HDD images or RVT-H Readers, this is an error.
		if (rvth->bankCount() != 1) {
			_ftprintf(stderr, _T("*** ERROR: Must specify a bank number for this RVT-H Reader%s.\n"),
				rvth->isHDD() ? _T("") : _T(" disk image"));
			delete rvth;
			return -EINVAL;
		}
		bank = 0;
	}

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	_tprintf(_T("Extracting Bank %u into '%s'...\n"), bank+1, gcm_filename);
	ret = rvth->extract(bank, gcm_filename, recrypt_key, flags, progress_callback);
	if (ret == 0) {
		_tprintf(_T("Bank %u extracted to '%s' successfully.\n\n"), bank+1, gcm_filename);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_extract() failed: %s\n", rvth_error(ret));
	}

	delete rvth;
	return ret;
}

/**
 * 'import' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @param gcm_filename	Filename of the GCM image to import.
 * @param ios_force	IOS version to force. (-1 to use the existing IOS)
 * @return 0 on success; non-zero on error.
 */
int import(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename, int ios_force)
{
	// TODO: Verification for overwriting images.

	// Open the RVT-H device or disk image.
	int ret;
	RvtH *const rvth = new RvtH(rvth_filename, &ret);
	if (ret != 0 || !rvth->isOpen()) {
		_ftprintf(stderr, _T("*** ERROR opening RVT-H device '%s': "), rvth_filename);
		fputs(rvth_error(ret), stderr);
		_fputtc(_T('\n'), stderr);
		delete rvth;
		return ret;
	}

	// Validate the bank number.
	TCHAR *endptr;
	unsigned int bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
	if (*endptr != 0 || bank > rvth->bankCount()) {
		_ftprintf(stderr, _T("*** ERROR: Invalid bank number '%s'.\n"), s_bank);
		delete rvth;
		return -EINVAL;
	}

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	// Print the source disc information.
	// This requires temporarily opening the source disc here.
	RvtH *const rvth_src_tmp = new RvtH(gcm_filename, &ret);
	if (ret != 0 || !rvth_src_tmp->isOpen()) {
		_ftprintf(stderr, _T("*** ERROR opening disc image '%s': "), gcm_filename);
		fputs(rvth_error(ret), stderr);
		_fputtc(_T('\n'), stderr);
		delete rvth_src_tmp;
		delete rvth;
		return ret;
	}
	_fputts(_T("Source disc image:\n"), stdout);
	print_bank(rvth_src_tmp, 0);
	putchar('\n');

	// If this is a Wii image and the IOS version doesn't match
	// the forced version, print a notice.
	if (ios_force >= 3) {
		const RvtH_BankEntry *const entry = rvth_src_tmp->bankEntry(0);
		if (entry &&
			(entry->type == RVTH_BankType_Wii_SL ||
			 entry->type == RVTH_BankType_Wii_DL))
		{
			if (entry->ios_version != ios_force) {
				_tprintf(_T("*** IOS version will be changed from %u to %d.\n\n"),
					entry->ios_version, ios_force);
			}
		}
	}
	delete rvth_src_tmp;

	_tprintf(_T("Importing '%s' into Bank %u...\n"), gcm_filename, bank+1);
	ret = rvth->import(bank, gcm_filename, progress_callback, nullptr, ios_force);
	if (ret == 0) {
		_tprintf(_T("'%s' imported to Bank %u successfully.\n"), gcm_filename, bank+1);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_import() failed: %s\n", rvth_error(ret));
	}

	delete rvth;
	return ret;
}
