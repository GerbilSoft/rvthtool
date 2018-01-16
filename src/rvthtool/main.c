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

#include "byteswap.h"
#include "rvth_structs.h"
#include "gcn_structs.h"

/**
 * Print an RVT-H Bank Table.
 * @param tbl	[in] RVT-H bank table.
 * @param f_img	[in,opt] If not NULL, disk image file for loading the disc headers.
 * @return 0 on success; non-zero on error.
 */
static int print_bank_table(const RVTH_BankTable *tbl, FILE *f_img)
{
	// Verify the bank table header.
	if (tbl->header.magic != cpu_to_be32(RVTH_BANKTABLE_MAGIC)) {
		fprintf(stderr, "*** ERROR: Bank table is invalid.\n");
		return 1;
	}

	// TODO: Figure out the other entries in the bank table.
	// TODO: Calculate expected LBAs and compare them to the bank table.

	// Print the entries.
	const RVTH_BankEntry *entry = tbl->entries;
	for (unsigned int i = 0; i < ARRAY_SIZE(tbl->entries); i++, entry++) {
		const uint32_t type = be32_to_cpu(entry->type);
		const char *s_type;
		switch (type) {
			case 0:
				// TODO: Print "Deleted" if a game is still present.
				s_type = "Empty";
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
				s_type = NULL;
				break;
		}

		if (s_type) {
			printf("Bank %u: %s\n", i+1, s_type);
		} else {
			printf("Bank %u: Unknown (0x%08X)\n", i+1, type);
		}

		if (type != 0 && s_type != NULL) {
			// Print timestamp.
			printf("- Timestamp: %.4s/%.2s/%.2s %.2s:%.2s:%.2s\n",
				&entry->mdate[0], &entry->mdate[4], &entry->mdate[6],
				&entry->mtime[0], &entry->mtime[2], &entry->mtime[4]);
		}

		// LBAs.
		uint32_t lba_start = be32_to_cpu(entry->lba_start);
		uint32_t lba_len = be32_to_cpu(entry->lba_len);
		if (lba_start == 0) {
			// No LBA start address.
			// Use the default LBA start and length.
			lba_start = (uint32_t)((RVTH_BANK_1_START + (RVTH_BANK_SIZE * i)) / RVTH_BLOCK_SIZE);
			lba_len = RVTH_BANK_SIZE / RVTH_BLOCK_SIZE;
		}

		printf("- LBA start:  0x%08X\n", lba_start);
		printf("- LBA length: 0x%08X\n", lba_len);

		if (f_img) {
			int ret; size_t size; int i;
			bool is_wii = false;

			// Read the disc header.
			int64_t addr = (int64_t)lba_start * RVTH_BLOCK_SIZE;
			GCN_DiscHeader discHeader;
			ret = fseeko(f_img, addr, SEEK_SET);
			if (ret != 0) {
				printf("- *** ERROR: Cannot seek to LBA start position: %s\n", strerror(errno));
				goto skip_disc_info;
			}
			size = fread(&discHeader, 1, sizeof(discHeader), f_img);
			if (size != sizeof(discHeader)) {
				printf("- *** ERROR: Unable to read the GCN/Wii disc header.\n");
				goto skip_disc_info;
			}

			// Check for magic numbers.
			if (discHeader.magic_wii == cpu_to_be32(WII_MAGIC)) {
				printf("- Disc type:  Wii\n");
				is_wii = true;
			} else if (discHeader.magic_gcn == cpu_to_be32(GCN_MAGIC)) {
				printf("- Disc type:  GameCube\n");
				is_wii = false;
			} else {
				printf("- Disc type:  Unknown\n");
				goto skip_disc_info;
			}

			// Print the game ID.
			printf("- Game ID:    %.6s\n", discHeader.id6);

			// Print the disc title.
			// First, trim excess spaces.
			for (i = ARRAY_SIZE(discHeader.game_title)-1; i >= 0; i--) {
				if (discHeader.game_title[i] != ' ')
					break;
				discHeader.game_title[i] = 0;
			}
			printf("- Title:      %.64s\n", discHeader.game_title);

			// TODO: Print encryption status for Wii.
		}

skip_disc_info:
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
	FILE *f_img = fopen(argv[1], "rb");
	if (!f_img) {
		fprintf(stderr, "*** ERROR opening '%s': %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	// Read the bank table.
	RVTH_BankTable tbl;
	int ret = fseeko(f_img, RVTH_BANKTABLE_ADDRESS, SEEK_SET);
	if (ret != 0) {
		fprintf(stderr, "*** ERROR seeking to bank table: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	size_t size = fread(&tbl, 1, sizeof(RVTH_BankTable), f_img);
	if (size != sizeof(tbl)) {
		fprintf(stderr, "*** ERROR reading bank table.\n");
		return EXIT_FAILURE;
	}

	// Print the bank table.
	printf("RVT-H Bank Table:\n\n");
	print_bank_table(&tbl, f_img);
	fclose(f_img);

	return EXIT_SUCCESS;
}
