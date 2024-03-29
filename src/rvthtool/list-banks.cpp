/***************************************************************************
 * RVT-H Tool                                                              *
 * list-banks.cpp: List banks in an RVT-H disk image.                      *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.librvth.h"
#include "list-banks.hpp"

#include "librvth/rvth.hpp"
#include "librvth/rvth_error.h"
#include "librvth/query.h"

#include "libwiicrypto/gcn_structs.h"
#include "libwiicrypto/sig_tools.h"

#include "time_r.h"

// C includes
#include <stdlib.h>

// C includes (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

/**
 * Trim a game title.
 * @param title Game title.
 * @param size Size of game title.
 */
static void trim_title(char *title, int size)
{
	size--;
	for (; size >= 0; size--) {
		if (title[size] != ' ')
			break;
		title[size] = 0;
	}
}

/**
 * Print information for the specified bank.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @return 0 on success; non-zero on error.
 */
int print_bank(const RvtH *rvth, unsigned int bank)
{
	// Region codes.
	static const TCHAR region_code_tbl[7][4] = {
		_T("JPN"), _T("USA"), _T("EUR"), _T("ALL"),
		_T("KOR"), _T("CHN"), _T("TWN")
	};

	const unsigned int bank_count = rvth->bankCount();
	if (bank >= bank_count) {
		// Out of range...
		return -ERANGE;
	}

	// Check if this is an HDD image.
	const bool is_hdd = rvth->isHDD();

	// TODO: Check the error code.
	int ret = 0;
	const RvtH_BankEntry *const entry = rvth->bankEntry(bank, &ret);
	if (!entry) {
		// NOTE: Should not return NULL for empty banks anymore...
		if (ret != 0 && ret != RVTH_ERROR_BANK_EMPTY) {
			// Error...
			return ret;
		}
		if (is_hdd) {
			_tprintf(_T("Bank %u: "), bank+1);
		} else {
			_fputts(_T("Disc image: "), stdout);
		}
		if (ret == RVTH_ERROR_BANK_EMPTY) {
			_fputts(_T("Empty\n\n"), stdout);
		} else {
			_fputts(_T("Unknown\n\n"), stdout);
		}
		return 0;
	} else if (entry->type == RVTH_BankType_Wii_DL_Bank2) {
		// Bank 2 for DL images.
		// We don't need to print this.
		return 0;
	}

	const TCHAR *s_type;
	switch (entry->type) {
		case RVTH_BankType_Empty:
			s_type = _T("Empty");
			break;
		case RVTH_BankType_Unknown:
			// TODO: Print the bank type magic?
			s_type = _T("Unknown");
			break;
		case RVTH_BankType_GCN:
			s_type = _T("GameCube");
			break;
		case RVTH_BankType_Wii_SL:
			s_type = _T("Wii (Single-Layer)");
			break;
		case RVTH_BankType_Wii_DL:
			// TODO: Invalid for Bank 8; need to skip the next bank.
			s_type = _T("Wii (Dual-Layer)");
			break;
		case RVTH_BankType_Wii_DL_Bank2:
			// NOTE: Should not happen...
			s_type = _T("Wii (Dual-Layer) (Bank 2)");
			break;
		default:
			s_type = _T("Unknown");
			break;
	}

	if (is_hdd) {
		_tprintf(_T("Bank %u: "), bank+1);
	} else {
		_fputts(_T("Disc image: "), stdout);
	}
	_tprintf(_T("%s%s\n"), s_type, (entry->is_deleted ? _T(" [DELETED]") : _T("")));

	// LBAs
	_tprintf(_T("- LBA start:   0x%08X\n"), entry->lba_start);
	_tprintf(_T("- LBA length:  0x%08X\n"), entry->lba_len);

	if (entry->type <= RVTH_BankType_Unknown ||
	    entry->type >= RVTH_BankType_MAX)
	{
		// Cannot display any more information for this type.
		return 0;
	}

	// Print the timestamp.
	if (entry->timestamp != -1) {
		struct tm timestamp;
		gmtime_r(&entry->timestamp, &timestamp);
		_tprintf(_T("- Timestamp:   %04d/%02d/%02d %02d:%02d:%02d\n"),
			timestamp.tm_year + 1900, timestamp.tm_mon + 1, timestamp.tm_mday,
			timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
	} else {
		_fputts(_T("- Timestamp:   Unknown\n"), stdout);
	}

	// Game ID
	printf("- Game ID:     %.6s\n", entry->discHeader.id6);

	// Game title
	char game_title[65];
	memcpy(game_title, entry->discHeader.game_title, 64);
	game_title[64] = 0;
	trim_title(game_title, 64);
	printf("- Title:       %.64s\n", game_title);
	printf("- Disc #:      %u\n", entry->discHeader.disc_number);
	printf("- Revision:    %u\n", entry->discHeader.revision);

	// Region code
	_fputts(_T("- Region code: "), stdout);
	if (entry->region_code < ARRAY_SIZE(region_code_tbl)) {
		_fputts(region_code_tbl[entry->region_code], stdout);
	} else {
		_tprintf(_T("0x%02X"), entry->region_code);
	}
	_fputtc(_T('\n'), stdout);

	// Wii encryption status.
	if (entry->type == RVTH_BankType_Wii_SL ||
	    entry->type == RVTH_BankType_Wii_DL)
	{
		// IOS version.
		// TODO: If 0, print an error message.
		printf("- IOS version: %u\n", entry->ios_version);

		// Encryption type.
		printf("- Encryption:  %s\n", RVL_CryptoType_toString((RVL_CryptoType_e)entry->crypto_type));

		// Ticket signature.
		printf("- Ticket Signature: %s%s\n",
			RVL_SigType_toString((RVL_SigType_e)entry->ticket.sig_type),
			RVL_SigStatus_toString_stsAppend((RVL_SigStatus_e)entry->ticket.sig_status));

		// TMD signature.
		printf("- TMD Signature:    %s%s\n",
			RVL_SigType_toString((RVL_SigType_e)entry->tmd.sig_type),
			RVL_SigStatus_toString_stsAppend((RVL_SigStatus_e)entry->tmd.sig_status));
	}

	// Check the AppLoader status.
	// TODO: Move strings to librvth?
	if (entry->aplerr > APLERR_OK) {
		_fputts(_T("\nAPPLOADER ERROR >>> "), stdout);
		switch (entry->aplerr) {
			default:
				_tprintf(_T("Unknown (%u)\n"), entry->aplerr);
				break;

			// TODO: Get values for these errors.
			case APLERR_FSTLENGTH:
				_tprintf(_T("FSTLength(%u) in BB2 is greater than FSTMaxLength(%u).\n"),
					entry->aplerr_val[0], entry->aplerr_val[1]);
				break;
			case APLERR_DEBUGMONSIZE_UNALIGNED:
				_tprintf(_T("Debug monitor size (%u) should be a multiple of 32.\n"),
					entry->aplerr_val[0]);
				break;
			case APLERR_SIMMEMSIZE_UNALIGNED:
				_tprintf(_T("Simulated memory size (%u) should be a multiple of 32.\n"),
				       entry->aplerr_val[0]);
				break;
			case APLERR_PHYSMEMSIZE_MINUS_SIMMEMSIZE_NOT_GT_DEBUGMONSIZE:
				_tprintf(_T("[Physical memory size(0x%x)] - [Console simulated memory size(0x%x)]\n")
					_T("APPLOADER ERROR >>> must be greater than debug monitor size(0x%x).\n"),
					entry->aplerr_val[0], entry->aplerr_val[1], entry->aplerr_val[2]);
				break;
			case APLERR_SIMMEMSIZE_NOT_LE_PHYSMEMSIZE:
				_tprintf(_T("Physical memory size is 0x%x bytes.\n")
					_T("APPLOADER ERROR >>> Console simulated memory size (0x%x) must be ")
					_T("smaller than or equal to the Physical memory size.\n"),
					entry->aplerr_val[0], entry->aplerr_val[1]);
				break;
			case APLERR_ILLEGAL_FST_ADDRESS:
				_tprintf(_T("Illegal FST destination address! (0x%x)\n"),
					entry->aplerr_val[0]);
				break;
			case APLERR_DOL_EXCEEDS_SIZE_LIMIT:
				_tprintf(_T("Total size of text/data sections of the dol file are too big (%d(0x%08x) bytes).\n")
					_T("APPLOADER ERROR >>> Currently the limit is set as %d(0x%08x) bytes.\n"),
					static_cast<int>(entry->aplerr_val[0]), entry->aplerr_val[0],
					static_cast<int>(entry->aplerr_val[1]), entry->aplerr_val[1]);
				break;
			case APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED:
				_tprintf(_T("One of the sections in the dol file exceeded its boundary.\n")
					_T("APPLOADER ERROR >>> All the sections should not exceed 0x%08x (production mode).\n")
					_T("APPLOADER WARNING >>> NOTE: This disc will still boot on devkits.\n"),
					entry->aplerr_val[0]);
				break;
			case APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED:
				_tprintf(_T("One of the sections in the dol file exceeded its boundary.\n")
					_T("APPLOADER ERROR >>> All the sections should not exceed 0x%08x (development mode).\n"),
					entry->aplerr_val[0]);
				break;
			case APLERR_DOL_TEXTSEG2BIG:
				_tprintf(_T("Too big text segment! (0x%x - 0x%x)\n"),
					entry->aplerr_val[0], entry->aplerr_val[1]);
				break;
			case APLERR_DOL_DATASEG2BIG:
				_tprintf(_T("Too big data segment! (0x%x - 0x%x)\n"),
					entry->aplerr_val[0], entry->aplerr_val[1]);
				break;
		}
	}

	return 0;
}

/**
 * Print an RVT-H Bank Table.
 * @param rvth	[in] RVT-H disk image.
 * @return 0 on success; non-zero on error.
 */
static int print_bank_table(const RvtH *rvth)
{
	const unsigned int bank_count = rvth->bankCount();

	// Print the entries.
	for (unsigned int bank = 0; bank < bank_count; bank++) {
		// TODO: Check for errors?
		const RvtH_BankEntry *const entry = rvth->bankEntry(bank);
		print_bank(rvth, bank);

		// Nothing is printed for the second bank of dual-layer Wii discs,
		// so don't print a newline in that case..
		if (entry && entry->type != RVTH_BankType_Wii_DL_Bank2) {
			putchar('\n');
		}
	}

	return 0;
}

/**
 * 'list-banks' command.
 * @param rvth_filename RVT-H device or disk image filename.
 * @return 0 on success; non-zero on error.
 */
int list_banks(const TCHAR *rvth_filename)
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

	_tprintf(_T("File: %s\n"), rvth_filename);

	// Check if this is an HDD image.
	bool checkNHCD = false;
	switch (rvth->imageType()) {
		case RVTH_ImageType_HDD_Reader: {
			checkNHCD = true;
			_fputts(_T("Type: RVT-H Reader System"), stdout);
#ifdef HAVE_QUERY
			// Get the serial number.
			TCHAR *const s_full_serial = rvth_get_device_serial_number(rvth_filename, nullptr);
			if (s_full_serial) {
				_tprintf(_T(" [%s]"), s_full_serial);
				free(s_full_serial);
			}
#endif /* HAVE_QUERY */
			_fputtc(_T('\n'), stdout);
			break;
		}

		case RVTH_ImageType_HDD_Image:
			checkNHCD = true;
			_fputts(_T("Type: RVT-H Reader Disk Image\n"), stdout);
			break;
		case RVTH_ImageType_GCM:
			// TODO: CISO/WBFS.
			_fputts(_T("Type: GCM Disc Image\n"), stdout);
			break;
		case RVTH_ImageType_GCM_SDK:
			_fputts(_T("Type: GCM Disc Image (with SDK headers)\n"), stdout);
			break;
		default:
			// Should not get here...
			assert(!"Should not get here...");
			delete rvth;
			return -EIO;
	}

	if (checkNHCD) {
		bool isDefaultNHCD = false;
		switch (rvth->nhcd_status()) {
			case NHCD_STATUS_OK:
				break;

			default:
			case NHCD_STATUS_UNKNOWN:
			case NHCD_STATUS_MISSING:
				isDefaultNHCD = true;
				_fputts(_T("*** WARNING: NHCD table is missing.\n"), stdout);
				break;

			case NHCD_STATUS_HAS_MBR:
				isDefaultNHCD = true;
				_fputts(_T("*** WARNING: This appears to be a PC MBR-partitioned HDD.\n"), stdout);
				break;

			case NHCD_STATUS_HAS_GPT:
				isDefaultNHCD = true;
				_fputts(_T("*** WARNING: This appears to be a PC GPT-partitioned HDD.\n"), stdout);
				break;
		}

		if (isDefaultNHCD) {
			_fputts(_T("*** Using defaults. Writing will be disabled.\n"), stdout);
		}
	}

	putchar('\n');

	// Print the bank table.
	if (rvth->isHDD()) {
		// HDD image and/or device.
		const unsigned int bank_count = rvth->bankCount();

		// Check for extended or shrunken bank tables.
		const TCHAR *extshr = _T("");
		if (bank_count > RVTH_BANK_COUNT) {
			// Bank table is larger than standard.
			extshr = _T("EXTENDED: ");
		} else if (bank_count < RVTH_BANK_COUNT) {
			// Bank table is smaller than standard.
			extshr = _T("SHRUNKEN: ");
		}
		_tprintf(_T("RVT-H Bank Table: [%s%u bank%s]\n\n"),
			extshr, bank_count, (bank_count != 1 ? _T("s") : _T("")));
	}

	print_bank_table(rvth);
	delete rvth;
	return 0;
}
