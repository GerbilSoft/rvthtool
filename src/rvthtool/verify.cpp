/***************************************************************************
 * RVT-H Tool                                                              *
 * verify.cpp: Verify a bank in an RVT-H disk image.                       *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "verify.h"
#include "list-banks.hpp"

#include "librvth/rvth.hpp"
#include "librvth/rvth_error.h"
#include "librvth/nhcd_structs.h"

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstdlib>

// C++ includes
#include <numeric>

/**
 * RVT-H verify progress callback.
 * @param state		[in] Current progress.
 * @param userdata	[in] User data specified when calling the RVT-H function.
 * @return True to continue; false to abort.
 */
static bool progress_callback(const RvtH_Verify_Progress_State *state, void *userdata)
{
	UNUSED(userdata);
	static int prev_pt = -1;	// TODO: Userdata struct.
	int pt_current = state->pt_current;
	if (pt_current < state->pt_total) {
		pt_current++;
	}
	if (prev_pt != -1 && prev_pt != pt_current) {
		putchar('\n');
	}
	prev_pt = pt_current;

	const char *ps_pt_type = nullptr;
	char s_pt_type[8];
	const uint32_t pt_type = state->pt_type;
	switch (pt_type) {
		case 0:
			ps_pt_type = "game";
			break;
		case 1:
			ps_pt_type = "updt";
			break;
		case 2:
			ps_pt_type = "chan";
			break;
		default:
			s_pt_type[0] = ( pt_type        & 0xFF);
			s_pt_type[1] = ((pt_type >>  8) & 0xFF);
			s_pt_type[2] = ((pt_type >> 16) & 0xFF);
			s_pt_type[3] = ((pt_type >> 24) & 0xFF);
			s_pt_type[4] = 0;
			ps_pt_type = s_pt_type;
			break;
	}

	switch (state->type) {
		default:
			assert(!"Invalid verify progress state.");
			break;

		case RVTH_VERIFY_STATUS: {
			printf("\rPartition %d/%u (%s): %4u MiB / %4u MiB checked...",
				pt_current, state->pt_total, ps_pt_type,
				state->group_cur * 2, state->group_total * 2);

			if (state->pt_current == state->pt_total) {
				// Finished processing.
				putchar('\n');
			}
			break;
		}

		case RVTH_VERIFY_ERROR_REPORT: {
			// Error report!
			char s_kb[16];
			if (state->hash_level == 0) {
				snprintf(s_kb, sizeof(s_kb), ",%u", state->kb);
			} else {
				s_kb[0] = '\0';
			}

			switch (state->err_type) {
				default:
					assert(!"Invalid error type.");
					break;
				case RVTH_VERIFY_ERROR_BAD_HASH:
					printf("\n*** ERROR: Pt%u [%u,%u,%u%s]: H%u hash is invalid.\n",
						state->pt_current, state->group_cur,
						state->sector / 8, state->sector,
						s_kb, state->hash_level);
					break;
				case RVTH_VERIFY_ERROR_TABLE_COPY:
					printf("\n*** ERROR: Pt%u [%u,%u,%u%s]: H%u table copy doesn't match base sector.\n",
						state->pt_current, state->group_cur,
						state->sector / 8, state->sector,
						s_kb, state->hash_level);
					break;
			}
			if (state->is_zero) {
				printf("*** (sector is zeroed; image may be scrubbed or truncated)\n");
			}
			break;
		}
	}

	fflush(stdout);
	return true;
}

/**
 * 'verify' command.
 * @param rvth_filename	[in] RVT-H device or disk image filename.
 * @param s_bank	[in] Bank number (as a string). (If NULL, assumes bank 1.)
 * @return 0 on success; non-zero on error.
 */
int verify(const TCHAR *rvth_filename, const TCHAR *s_bank)
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

	unsigned int error_count[5] = {0, 0, 0, 0, 0};
	_tprintf(_T("Verifying Bank %u...\n"), bank+1);
	fflush(stdout);
	ret = rvth->verifyWiiPartitions(bank, error_count, progress_callback);
	if (ret == 0) {
		// Add up the errors.
		unsigned int total_errs = std::accumulate(error_count, error_count + ARRAY_SIZE(error_count), 0);
		_tprintf(_T("Bank %u verified with %u error%s.\n"), bank+1, total_errs, (total_errs != 1) ? "s" : "");
	} else {
		fprintf(stderr, "*** ERROR: rvth->verifyWiiPartitions() failed: %s\n", rvth_error(ret));
	}

	delete rvth;
	return ret;
}
