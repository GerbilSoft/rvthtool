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

#include <stdlib.h>
#include <string.h>

#include "librvth/common.h"
#include "librvth/byteswap.h"
#include "librvth/rvth.h"

#include "list-banks.h"
#include "extract.h"

#ifdef _MSC_VER
# define CDECL __cdecl
#else
# define CDECL
#endif

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
