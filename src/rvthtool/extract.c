/***************************************************************************
 * RVT-H Tool                                                              *
 * extract.c: Extract a bank from an RVT-H disk image.                     *
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

#include <errno.h>
#include <stdlib.h>

/**
 * RVT-H progress callback.
 * @param lba_processed LBAs processed.
 * @param lba_total LBAs total.
 * @return True to continue; false to abort.
 */
static bool progress_callback(uint32_t lba_processed, uint32_t lba_total)
{
	#define MEGABYTE (1048576 / RVTH_BLOCK_SIZE)
	printf("\r%4u MB / %4u MB processed...",
		lba_processed / MEGABYTE,
		lba_total / MEGABYTE);
	fflush(stdout);
	return true;
}

/**
 * 'extract' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @param gcm_filename	Filename for the extracted GCM image.
 * @return 0 on success; non-zero on error.
 */
int extract(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename)
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
	ret = rvth_extract(rvth, bank, gcm_filename, progress_callback);
	putchar('\n');
	if (ret == 0) {
		printf("Bank %u extracted to '", bank+1);
		_fputts(gcm_filename, stdout);
		fputs("' successfully.\n\n", stdout);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_extract() failed: %s", rvth_error(ret));
	}

	rvth_close(rvth);
	return ret;
}
