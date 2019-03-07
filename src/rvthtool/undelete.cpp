/***************************************************************************
 * RVT-H Tool                                                              *
 * undelete.cpp: Delete or undelete a bank in an RVT-H disk image.         *
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

#include "undelete.h"
#include "list-banks.hpp"
#include "librvth/rvth.hpp"
#include "librvth/rvth_error.h"

#include <errno.h>
#include <stdlib.h>

/**
 * 'delete' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @return 0 on success; non-zero on error.
 */
int delete_bank(const TCHAR *rvth_filename, const TCHAR *s_bank)
{
	// Open the disk image.
	int ret;
	RvtH *const rvth = new RvtH(rvth_filename, &ret);
	if (!rvth->isOpen()) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fprintf(stderr, "': %s\n", rvth_error(ret));
		delete rvth;
		return ret;
	}

	// Validate the bank number.
	TCHAR *endptr;
	unsigned int bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
	if (*endptr != 0 || bank > rvth->bankCount()) {
		fputs("*** ERROR: Invalid bank number '", stderr);
		_fputts(s_bank, stderr);
		fputs("'.\n", stderr);
		delete rvth;
		return -EINVAL;
	}

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	// Delete the bank.
	ret = rvth->deleteBank(bank);
	if (ret == 0) {
		printf("Bank %u deleted.\n", bank+1);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_delete() failed: %s\n", rvth_error(ret));
	}

	delete rvth;
	return ret;
}

/**
 * 'undelete' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @return 0 on success; non-zero on error.
 */
int undelete_bank(const TCHAR *rvth_filename, const TCHAR *s_bank)
{
	// Open the disk image.
	int ret;
	RvtH *const rvth = new RvtH(rvth_filename, &ret);
	if (!rvth) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fprintf(stderr, "': %s\n", rvth_error(ret));
		return ret;
	}

	// Validate the bank number.
	TCHAR *endptr;
	unsigned int bank = (unsigned int)_tcstoul(s_bank, &endptr, 10) - 1;
	if (*endptr != 0 || bank > rvth->bankCount()) {
		fputs("*** ERROR: Invalid bank number '", stderr);
		_fputts(s_bank, stderr);
		fputs("'.\n", stderr);
		delete rvth;
		return -EINVAL;
	}

	// Print the bank information.
	// TODO: Make sure the bank type is valid before printing the newline.
	print_bank(rvth, bank);
	putchar('\n');

	// Undelete the bank.
	ret = rvth->undeleteBank(bank);
	if (ret == 0) {
		printf("Bank %u undeleted.\n", bank+1);
	} else {
		// TODO: Delete the gcm file?
		fprintf(stderr, "*** ERROR: rvth_undelete() failed: %s\n", rvth_error(ret));
	}

	delete rvth;
	return ret;
}
