/***************************************************************************
 * RVT-H Tool                                                              *
 * list-banks.c: List banks in an RVT-H disk image.                        *
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

#include "list-banks.h"
#include "librvth/rvth.h"
#include "librvth/gcn_structs.h"

#include <errno.h>

/**
 * Print information for the specified bank.
 * @param rvth	[in] RVT-H disk image.
 * @param bank	[in] Bank number. (0-7)
 * @return 0 on success; non-zero on error.
 */
int print_bank(const RvtH *rvth, unsigned int bank)
{
	const RvtH_BankEntry *entry;
	const char *s_type;
	bool is_hdd;
	int ret = 0;

	// Region codes.
	static const char region_code_tbl[5][4] = {
		"JPN", "USA", "EUR", "ALL", "KOR"
	};

	const unsigned int bank_count = rvth_get_BankCount(rvth);
	if (bank >= bank_count) {
		// Out of range...
		return -ERANGE;
	}

	// Check if this is an HDD image.
	is_hdd = rvth_is_hdd(rvth);

	// TODO: Check the error code.
	entry = rvth_get_BankEntry(rvth, bank, &ret);
	if (!entry) {
		// NOTE: Should not return NULL for empty banks anymore...
		if (ret != 0 && ret != RVTH_ERROR_BANK_EMPTY) {
			// Error...
			return ret;
		}
		if (is_hdd) {
			printf("Bank %u: ", bank+1);
		} else {
			fputs("Disc image: ", stdout);
		}
		if (ret == RVTH_ERROR_BANK_EMPTY) {
			fputs("Empty\n\n", stdout);
		} else {
			fputs("Unknown\n\n", stdout);
		}
		return 0;
	} else if (entry->type == RVTH_BankType_Wii_DL_Bank2) {
		// Bank 2 for DL images.
		// We don't need to print this.
		return 0;
	}

	switch (entry->type) {
		case RVTH_BankType_Empty:
			s_type = "Empty";
			break;
		case RVTH_BankType_Unknown:
			// TODO: Print the bank type magic?
			s_type = "Unknown";
			break;
		case RVTH_BankType_GCN:
			s_type = "GameCube";
			break;
		case RVTH_BankType_Wii_SL:
			s_type = "Wii (Single-Layer)";
			break;
		case RVTH_BankType_Wii_DL:
			// TODO: Invalid for Bank 8; need to skip the next bank.
			s_type = "Wii (Dual-Layer)";
			break;
		case RVTH_BankType_Wii_DL_Bank2:
			// NOTE: Should not happen...
			s_type = "Wii (Dual-Layer) (Bank 2)";
			break;
		default:
			s_type = "Unknown";
			break;
	}

	if (is_hdd) {
		printf("Bank %u: ", bank+1);
	} else {
		fputs("Disc image: ", stdout);
	}
	printf("%s%s\n", s_type, (entry->is_deleted ? " [DELETED]" : ""));

	// LBAs.
	printf("- LBA start:   0x%08X\n", entry->lba_start);
	printf("- LBA length:  0x%08X\n", entry->lba_len);

	if (entry->type <= RVTH_BankType_Unknown ||
	    entry->type >= RVTH_BankType_MAX)
	{
		// Cannot display any more information for this type.
		return 0;
	}

	// Print the timestamp.
	// TODO: Reentrant gmtime() if available.
	if (entry->timestamp != -1) {
		struct tm timestamp = *gmtime(&entry->timestamp);
		printf("- Timestamp:   %04d/%02d/%02d %02d:%02d:%02d\n",
			timestamp.tm_year + 1900, timestamp.tm_mon + 1, timestamp.tm_mday,
			timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
	} else {
		printf("- Timestamp:   Unknown\n");
	}

	// Game ID.
	printf("- Game ID:     %.6s\n", entry->id6);

	// Game title.
	// rvth_open() has already trimmed the title.
	printf("- Title:       %.64s\n", entry->game_title);
	printf("- Disc #:      %u\n", entry->disc_number);
	printf("- Revision:    %u\n", entry->revision);

	// Region code.
	fputs("- Region code: ", stdout);
	if (entry->region_code < ARRAY_SIZE(region_code_tbl)) {
		fputs(region_code_tbl[entry->region_code], stdout);
	} else {
		printf("0x%02X", entry->region_code);
	}
	putchar('\n');

	// Wii encryption status.
	if (entry->type == RVTH_BankType_Wii_SL ||
	    entry->type == RVTH_BankType_Wii_DL)
	{
		const char *s_crypto_type;
		const char *s_sig_type, *s_sig_status;

		static const char *const crypto_type_tbl[] = {
			// tr: RVTH_CryptoType_Unknown
			"Unknown",
			// tr: RVTH_CryptoType_None
			"None",
			// tr: RVTH_CryptoType_Debug
			"Debug",
			// tr: RVTH_CryptoType_Retail
			"Retail",
			// tr: RVTH_CryptoType_Korean
			"Korean",
		};

		static const char *const sig_type_tbl[] = {
			// tr: RVTH_SigType_Unknown
			"Unknown",
			// tr: RVTH_SigType_Debug
			"Debug",
			// tr: RVTH_SigType_Retail
			"Retail",
		};

		static const char *const sig_status_tbl[] = {
			// tr: RVTH_SigStatus_Unknown
			" (unknown)",
			// tr: RVTH_SigStatus_OK
			"",
			// tr: RVTH_SigStatus_Invalid
			" (INVALID)",
			// tr: RVTH_SigStatus_Fake
			" (fakesigned)",
		};

		// IOS version.
		// TODO: If 0, print an error message.
		printf("- IOS version: %u\n", entry->ios_version);

		// Encryption type.
		// TODO: static_assert() implementation.
		//static_assert(ARRAY_SIZE(crypto_type_tbl) == RVTH_CryptoType_MAX, "Update crypto_type_tbl[]");
		s_crypto_type = (entry->crypto_type < ARRAY_SIZE(crypto_type_tbl)
			? crypto_type_tbl[entry->crypto_type]
			: crypto_type_tbl[RVTH_CryptoType_Unknown]);
		printf("- Encryption:  %s\n", s_crypto_type);

		// Ticket signature.
		// TODO: static_assert() implementation.
		//static_assert(ARRAY_SIZE(sig_type_tbl) == RVTH_SigType_MAX, "Update sig_type_tbl[]");
		s_sig_type = (entry->ticket.sig_type < ARRAY_SIZE(sig_type_tbl)
			? sig_type_tbl[entry->ticket.sig_type]
			: sig_type_tbl[RVTH_SigType_Unknown]);
		// TODO: static_assert() implementation.
		//static_assert(ARRAY_SIZE(sig_status_tbl) == RVTH_SigStatus_MAX, "Update sig_status_tbl[]");
		s_sig_status = (entry->ticket.sig_status < ARRAY_SIZE(sig_status_tbl)
			? sig_status_tbl[entry->ticket.sig_status]
			: sig_status_tbl[RVTH_SigStatus_Unknown]);
		printf("- Ticket Signature: %s%s\n", s_sig_type, s_sig_status);

		// TMD signature.
		// TODO: static_assert() implementation.
		//static_assert(ARRAY_SIZE(sig_type_tbl) == RVTH_SigType_MAX, "Update sig_type_tbl[]");
		s_sig_type = (entry->tmd.sig_type < ARRAY_SIZE(sig_type_tbl)
			? sig_type_tbl[entry->tmd.sig_type]
			: sig_type_tbl[RVTH_SigType_Unknown]);
		// TODO: static_assert() implementation.
		//static_assert(ARRAY_SIZE(sig_status_tbl) == RVTH_SigStatus_MAX, "Update sig_status_tbl[]");
		s_sig_status = (entry->tmd.sig_status < ARRAY_SIZE(sig_status_tbl)
			? sig_status_tbl[entry->tmd.sig_status]
			: sig_status_tbl[RVTH_SigStatus_Unknown]);
		printf("- TMD Signature:    %s%s\n", s_sig_type, s_sig_status);
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
	unsigned int bank;
	const unsigned int bank_count = rvth_get_BankCount(rvth);

	// Print the entries.
	for (bank = 0; bank < bank_count; bank++) {
		// TODO: Check for errors?
		const RvtH_BankEntry *const entry = rvth_get_BankEntry(rvth, bank, NULL);
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
	RvtH *rvth;
	int ret;

	// Open the disk image.
	rvth = rvth_open(rvth_filename, &ret);
	if (!rvth) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fprintf(stderr, "': %s\n", rvth_error(ret));
		return ret;
	}

	// Print the bank table.
	if (rvth_is_hdd(rvth)) {
		// HDD image and/or device.
		const unsigned int bank_count = rvth_get_BankCount(rvth);
		fputs("RVT-H Bank Table: [", stdout);
		if (bank_count > RVTH_BANK_COUNT) {
			// Bank table is larger than standard.
			fputs("EXTENDED: ", stdout);
		} else if (bank_count < RVTH_BANK_COUNT) {
			// Bank table is smaller than standard.
			fputs("SHRUNKEN: ", stdout);
		}
		printf("%u bank%s]\n", bank_count, (bank_count != 1 ? "s" : ""));
	}

	putchar('\n');
	print_bank_table(rvth);
	rvth_close(rvth);
	return 0;
}
