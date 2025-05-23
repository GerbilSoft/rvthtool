/***************************************************************************
 * RVT-H Tool                                                              *
 * show-table.cpp: Print out the NHCD Bank Table for debugging an invalid  *
 * bank.                                                                   *
 *                                                                         *
 * Copyright (c) 2023-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.librvth.h"
#include "show-table.hpp"

#include "librvth/rvth.hpp"
#include "librvth/rvth_error.h"
#include "librvth/nhcd_structs.h"

#include "libwiicrypto/byteswap.h"

static void print_table(NHCD_BankTable_Header* header) {
	_tprintf(_T("- Table Magic: 0x%08X (expected: 0x%08X, \"NHCD\")\n"), le32_to_cpu(header->magic), be32_to_cpu(NHCD_BANKTABLE_MAGIC));
	_tprintf(_T("-        x004: 0x%08X (expected: 0x00000001)\n"), le32_to_cpu(header->x004));
	_tprintf(_T("-  Bank Count: 0x%08X (expected: 0x00000008)\n"), le32_to_cpu(header->bank_count));
	_tprintf(_T("-        x00C: 0x%08X (expected: 0x00000000)\n"), le32_to_cpu(header->x00C));
	_tprintf(_T("-        x010: 0x%08X (expected: 0x002FF000)\n"), le32_to_cpu(header->x010));

	_fputts(_T("- unk table:\n"), stdout);
	for (uint8_t row = 0; row < 41; ++row) {
		uint16_t idx = row * 12;
		_tprintf(_T("  %#02x"), header->unk[idx]);
		idx += 1;
		for (; idx < (row + 1) * 12; ++idx) {
			_tprintf(_T(" %#02x"), header->unk[idx]);
		}
		_fputtc(_T('\n'), stdout);
	}
	_fputtc(_T('\n'), stdout);
}

/**
 * 'show-table' command.
 * @param rvth_filename RVT-H device or disk image filename.
 * @return 0 on success; non-zero on error.
 */
int show_table(const TCHAR *rvth_filename)
{
	// Open the disk image.
	int ret;
	RvtH *const rvth = new RvtH(rvth_filename, &ret);
	if (ret != 0 || !rvth->isOpen()) {
		_ftprintf(stderr, _T("*** ERROR opening RVT-H device '%s': "), rvth_filename);
		fputs(rvth_error(ret), stderr);
		_fputtc(_T('\n'), stderr);
		delete rvth;
		return ret;
	}

	// Validate we're talking to an actual HDD image.
	if (!rvth->isHDD()) {
		_fputts(_T("*** ERROR can only print table information on an HDD.\n"), stderr);
		delete rvth;
		return 1;
	}

	// Print out the raw NHCD status.
	const TCHAR *s_nhcd_status;
	switch (rvth->nhcd_status()) {
		case NHCD_STATUS_OK:
			s_nhcd_status = _T("OK");
			break;
		case NHCD_STATUS_UNKNOWN:
			s_nhcd_status = _T("UNKNOWN");
			break;
		case NHCD_STATUS_MISSING:
			s_nhcd_status = _T("MISSING");
			break;
		case NHCD_STATUS_HAS_MBR:
			s_nhcd_status = _T("HAS_MBR");
			break;
		case NHCD_STATUS_HAS_GPT:
			s_nhcd_status = _T("HAS_GPT");
			break;
		default:
			s_nhcd_status = _T("???");
			break;
	}
	_tprintf(_T("[%s] NHCD Bank Table:\n"), rvth_filename);
	_tprintf(_T("-      Status: %s\n"), s_nhcd_status);

	NHCD_BankTable_Header* header = rvth->nhcd_header();
	if (header == nullptr) {
		_fputts(_T("*** ERROR failed to read NHCD header image.\n"), stderr);
		delete rvth;
		return 1;
	}
	print_table(header);

	delete rvth;
	return 0;
}
