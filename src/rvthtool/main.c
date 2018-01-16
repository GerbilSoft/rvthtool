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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librvth/byteswap.h"
#include "librvth/rvth.h"

/**
 * Print an RVT-H Bank Table.
 * @param rvth	[in] RVT-H disk image.
 * @return 0 on success; non-zero on error.
 */
static int print_bank_table(const RvtH *rvth)
{
	struct tm timestamp;

	// Print the entries.
	for (unsigned int i = 0; i < RVTH_BANK_COUNT; i++) {
		const RvtH_BankEntry *const entry = rvth_get_BankEntry(rvth, i);
		if (!entry) {
			printf("Bank %u: Empty\n\n", i+1);
			continue;
		}

		const char *s_type;
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

		// TODO: Print encryption status for Wii.
		printf("\n");
	}

	return 0;
}

int main(int argc, char *argv[])
{
	((void)argc);
	((void)argv);

	printf("RVT-H Tool v" VERSION_STRING "\n"
		"Copyright (c) 2018 by David Korth.\n\n");

	if (argc != 2) {
		printf("Syntax: %s diskimage.img\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Open the disk image.
	RvtH *rvth = rvth_open(argv[1]);
	if (!rvth) {
		fprintf(stderr, "*** ERROR opening '%s': %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	// Print the bank table.
	printf("RVT-H Bank Table:\n\n");
	print_bank_table(rvth);
	rvth_close(rvth);

	return EXIT_SUCCESS;
}
