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

int CDECL main(int argc, char *argv[])
{
	RvtH *rvth;
	int ret;

	((void)argc);
	((void)argv);

	printf("RVT-H Tool v" VERSION_STRING "\n"
		"Copyright (c) 2018 by David Korth.\n\n");

	if (argc != 2 && argc != 3) {
		printf("Syntax: %s diskimage.img [bank]\n"
			"If a bank number (1-8) is specified, that image will be extracted into BankX.gcm."
			, argv[0]);
		return EXIT_FAILURE;
	}

	// Open the disk image.
	rvth = rvth_open(argv[1], &ret);
	if (!rvth) {
		fprintf(stderr, "*** ERROR opening '%s': ", argv[1]);
		if (ret < 0) {
			fprintf(stderr, "%s\n", strerror(-ret));
		} else {
			fprintf(stderr, "RVT-H error %d\n", ret);
		}
		return EXIT_FAILURE;
	}

	// Print the bank table.
	printf("RVT-H Bank Table:\n\n");
	print_bank_table(rvth);

	if (argc == 3) {
		// Validate the bank number.
		char gcm_filename[16];
		char *endptr;

		unsigned int bank = (unsigned int)strtoul(argv[2], &endptr, 10) - 1;
		if (*endptr != 0 || bank > rvth_get_BankCount(rvth)) {
			fprintf(stderr, "*** ERROR: Invalid bank number '%s'.\n", argv[2]);
			rvth_close(rvth);
			return EXIT_FAILURE;
		}

		printf("Extracting Bank %u into Bank%u.gcm...\n", bank+1, bank+1);
		snprintf(gcm_filename, sizeof(gcm_filename), "Bank%u.gcm", bank+1);
		ret = rvth_extract(rvth, bank, gcm_filename, progress_callback);
		printf("\n");
		if (ret == 0) {
			printf("Bank extracted to '%s' successfully.\n\n", gcm_filename);
		} else {
			// TODO: Delete the gcm file?
			fprintf(stderr, "*** ERROR: rvth_extract() failed: ");
			if (ret < 0) {
				fprintf(stderr, "%s\n", strerror(-ret));
			} else {
				fprintf(stderr, "RVT-H error %d\n", ret);
			}
		}
	}

	rvth_close(rvth);
	return EXIT_SUCCESS;
}
