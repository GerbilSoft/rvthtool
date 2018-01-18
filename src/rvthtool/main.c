/***************************************************************************
 * RVT-H Tool                                                              *
 * main.c: Main program file.                                              *
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

#include "config.version.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librvth/common.h"
#include "librvth/byteswap.h"
#include "librvth/rvth.h"

#ifdef _MSC_VER
# define CDECL __cdecl
#else
# define CDECL
#endif

/**
 * Print an RVT-H Bank Table.
 * @param rvth	[in] RVT-H disk image.
 * @return 0 on success; non-zero on error.
 */
static int print_bank_table(const RvtH *rvth)
{
	struct tm timestamp;
	unsigned int i;
	const unsigned int banks = rvth_get_BankCount(rvth);

	// Print the entries.
	for (i = 0; i < banks; i++) {
		const char *s_type;

		// TODO: Check the error code.
		const RvtH_BankEntry *const entry = rvth_get_BankEntry(rvth, i, NULL);
		if (!entry) {
			printf("Bank %u: Empty\n\n", i+1);
			continue;
		} else if (entry->type == RVTH_BankType_Wii_DL_Bank2) {
			// Bank 2 for DL images.
			// We don't need to print this.
			continue;
		}

		switch (entry->type) {
			case RVTH_BankType_Empty:
				// NOTE: Should not happen...
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

		printf("Bank %u: %s%s\n", i+1, s_type, (entry->is_deleted ? " [DELETED]" : ""));
		if (entry->type <= RVTH_BankType_Unknown ||
		    entry->type >= RVTH_BankType_MAX)
		{
			// Cannot display any information for this type.
			continue;
		}

		// Print the timestamp.
		// TODO: Reentrant gmtime() if available.
		if (entry->timestamp != -1) {
			timestamp = *gmtime(&entry->timestamp);
			printf("- Timestamp:  %04d/%02d/%02d %02d:%02d:%02d\n",
				timestamp.tm_year + 1900, timestamp.tm_mon + 1, timestamp.tm_mday,
				timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);
		} else {
			printf("- Timestamp:  Unknown\n");
		}

		// LBAs.
		printf("- LBA start:  0x%08X\n", entry->lba_start);
		printf("- LBA length: 0x%08X\n", entry->lba_len);

		// Game ID.
		printf("- Game ID:    %.6s\n", entry->id6);

		// Game title.
		// rvth_open() has already trimmed the title.
		printf("- Title:      %.64s\n", entry->game_title);

		// Wii encryption status.
		if (entry->type == RVTH_BankType_Wii_SL ||
		    entry->type == RVTH_BankType_Wii_DL)
		{
			const char *crypto_type;
			const char *sig_type;

			switch (entry->crypto_type) {
				default:
				case RVTH_CryptoType_Unknown:
					crypto_type = "Unknown";
					break;
				case RVTH_CryptoType_None:
					crypto_type = "None";
					break;
				case RVTH_CryptoType_Debug:
					crypto_type = "Debug";
					break;
				case RVTH_CryptoType_Retail:
					crypto_type = "Retail";
					break;
				case RVTH_CryptoType_Korean:
					crypto_type = "Korean";
					break;
			}

			switch (entry->sig_type) {
				default:
				case RVTH_SigType_Unknown:
					sig_type = "Unknown";
					break;
				case RVTH_SigType_Debug:
					sig_type = "Debug";
					break;
				case RVTH_SigType_Retail:
					sig_type = "Retail";
					break;
			}

			printf("- Encryption: %s\n", crypto_type);
			printf("- Signature:  %s\n", sig_type);
		}

		printf("\n");
	}

	return 0;
}

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
 * 'list-banks' command.
 * @param rvth_filename RVT-H device or disk image filename.
 * @return 0 on success; non-zero on error.
 */
static int list_banks(const TCHAR *rvth_filename)
{
	RvtH *rvth;
	int ret;

	// Open the disk image.
	rvth = rvth_open(rvth_filename, &ret);
	if (!rvth) {
		fputs("*** ERROR opening RVT-H device '", stderr);
		_fputts(rvth_filename, stderr);
		fputs("': ", stderr);
		if (ret < 0) {
			fprintf(stderr, "%s\n", strerror(-ret));
		} else {
			fprintf(stderr, "RVT-H error %d\n", ret);
		}
		return ret;
	}

	// Print the bank table.
	printf("RVT-H Bank Table:\n\n");
	print_bank_table(rvth);
	rvth_close(rvth);
	return 0;
}

/**
 * 'extract' command.
 * @param rvth_filename	RVT-H device or disk image filename.
 * @param s_bank	Bank number (as a string).
 * @param gcm_filename	Filename for the extracted GCM image.
 * @return 0 on success; non-zero on error.
 */
static int extract(const TCHAR *rvth_filename, const TCHAR *s_bank, const TCHAR *gcm_filename)
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
		fputs("': ", stderr);
		if (ret < 0) {
			fprintf(stderr, "%s\n", strerror(-ret));
		} else {
			fprintf(stderr, "RVT-H error %d\n", ret);
		}
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

	printf("Extracting Bank %u into '", bank+1);
	_fputts(gcm_filename, stdout);
	fputs("'...\n", stdout);
	ret = rvth_extract(rvth, bank, gcm_filename, progress_callback);
	printf("\n");
	if (ret == 0) {
		printf("Bank %u extracted to '", bank+1);
		_fputts(gcm_filename, stdout);
		fputs("' successfully.\n\n", stdout);
	} else {
		// TODO: Delete the gcm file?
		fputs("*** ERROR: rvth_extract() failed: ", stderr);
		if (ret < 0) {
			fprintf(stderr, "%s\n", strerror(-ret));
		} else {
			fprintf(stderr, "RVT-H error %d\n", ret);
		}
	}

	rvth_close(rvth);
	return 0;
}

int CDECL _tmain(int argc, TCHAR *argv[])
{
	int ret;

	((void)argc);
	((void)argv);

	printf("RVT-H Tool v" VERSION_STRING "\n"
		"Copyright (c) 2018 by David Korth.\n\n");

	// TODO: getopt().
	// Unicode getopt() for Windows:
	// - https://www.codeproject.com/Articles/157001/Full-getopt-Port-for-Unicode-and-Multibyte-Microso

	if (argc == 1) {
		fputs("Syntax: ", stdout);
		_fputts(argv[0], stdout);
		fputs(" [options] [command]\n"
			"\n"
			"Supported commands:\n"
			"\n"
			"list rvth.img\n"
			"- List banks in the specified RVT-H device or disk image.\n"
			"\n"
			"extract rvth.img bank# disc.gcm\n"
			"- Extract the specified bank number from rvth.img to disc.gcm.\n"
			"\n"
			, stdout);
		return EXIT_FAILURE;
	}

	// Check the specified command.
	// TODO: Better help if the command parameters are invalid.
	if (!_tcscmp(argv[1], _T("list")) || !_tcscmp(argv[1], _T("list-banks"))) {
		// List banks.
		if (argc < 3) {
			fputs("*** ERROR: RVT-H device or disk image not specified.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = list_banks(argv[2]);
	} else if (!_tcscmp(argv[1], _T("extract"))) {
		// Extract banks.
		if (argc < 4) {
			fputs("*** ERROR: Missing parameters for 'extract'.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = extract(argv[2], argv[3], argv[4]);
	} else {
		// TODO: If it's a filename, handle it as "list".
		fputs("*** ERROR: Unrecognized command: '", stderr);
		_fputts(argv[1], stderr);
		fputs("'\n", stderr);
		return EXIT_FAILURE;
	}

	return ret;
}
