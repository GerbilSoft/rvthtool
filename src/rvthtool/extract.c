/***************************************************************************
 * RVT-H Tool                                                              *
 * extract.c: Extract/import a bank from/to an RVT-H disk image.           *
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

#include "extract.h"
#include "list-banks.h"
#include "librvth/rvth.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

/**
 * RVT-H progress callback.
 * @param state RvtH_Progress_State
 * @return True to continue; false to abort.
 */
static bool progress_callback(const RvtH_Progress_State *state)
{
	#define MEGABYTE (1048576 / RVTH_BLOCK_SIZE)
	switch (state->type) {
		case RVTH_PROGRESS_EXTRACT:
			printf("\rExtracting: %4u MB / %4u MB copied...",
				state->lba_processed / MEGABYTE,
				state->lba_total / MEGABYTE);
			break;
		case RVTH_PROGRESS_IMPORT:
			printf("\rImporting: %4u MB / %4u MB copied...",
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
				printf("\rRecrypting: %4u MB / %4u MB processed...",
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
 * @param s_bank	[in] Bank number (as a string).
 * @param gcm_filename	[in] Filename for the extracted GCM image.
 * @param recrypt_key	[in] Key for recryption. (-1 for default)
 * @return 0 on success; non-zero on error.
 */
int extract(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename, int recrypt_key)
{
	RvtH *rvth;
	TCHAR *endptr;
	unsigned int bank;
	int ret;

	// Open the disk image.
	rvth = rvth_open(rvth_filename, &ret);
	if (!rvth) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fprintf(stderr, "': %s\n", rvth_error(ret));
		return ret;
	}

	// Validate the bank number.
	bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
	if (*endptr != 0 || bank > rvth_get_BankCount(rvth)) {
		fputs("*** ERROR: Invalid bank number '", stderr);
		_fputts(s_bank, stderr);
		fputs("'.\n", stderr);
		rvth_close(rvth);
		return -EINVAL;
	}

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	printf("Extracting Bank %u into '", bank+1);
	_fputts(gcm_filename, stdout);
	fputs("'...\n", stdout);
	ret = rvth_extract(rvth, bank, gcm_filename, recrypt_key, progress_callback);
	if (ret == 0) {
		printf("Bank %u extracted to '", bank+1);
		_fputts(gcm_filename, stdout);
		fputs("' successfully.\n\n", stdout);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_extract() failed: %s\n", rvth_error(ret));
	}

	rvth_close(rvth);
	return ret;
}

/**
 * 'import' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @param gcm_filename	Filename of the GCM image to import.
 * @return 0 on success; non-zero on error.
 */
int import(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename)
{
	RvtH *rvth;
	TCHAR *endptr;
	unsigned int bank;
	int ret;

	// TODO: Verification for overwriting images.
	// TODO: Special identification.

	// Open the disk image.
	rvth = rvth_open(rvth_filename, &ret);
	if (!rvth) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fprintf(stderr, "': %s\n", rvth_error(ret));
		return ret;
	}

	// Validate the bank number.
	bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
	if (*endptr != 0 || bank > rvth_get_BankCount(rvth)) {
		fputs("*** ERROR: Invalid bank number '", stderr);
		_fputts(s_bank, stderr);
		fputs("'.\n", stderr);
		rvth_close(rvth);
		return -EINVAL;
	}

	// TODO: Print source disc information.

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	fputs("Importing '", stdout);
	_fputts(gcm_filename, stdout);
	printf("' into Bank %u...\n", bank+1);
	ret = rvth_import(rvth, bank, gcm_filename, progress_callback);
	if (ret == 0) {
		fputc('\'', stdout);
		_fputts(gcm_filename, stdout);
		printf("' imported to Bank %u successfully.\n", bank+1);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_import() failed: %s\n", rvth_error(ret));
	}

	rvth_close(rvth);
	return ret;
}
