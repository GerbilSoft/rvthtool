/***************************************************************************
 * RVT-H Tool                                                              *
 * undelete.cpp: Delete or undelete a bank in an RVT-H disk image.         *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
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
	if (ret != 0 || !rvth->isOpen()) {
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
	if (ret != 0 || !rvth->isOpen()) {
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
