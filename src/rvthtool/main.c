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
#include "undelete.h"

#ifdef _MSC_VER
# define RVTH_CDECL __cdecl
#else
# define RVTH_CDECL
#endif

#ifdef _WIN32
# define DEVICE_NAME_EXAMPLE "\\\\.\\PhysicalDriveN"
#else
// TODO: Non-Linux systems.
# define DEVICE_NAME_EXAMPLE "/dev/sdX"
#endif

#include "librvth/cert.h"
int RVTH_CDECL _tmain(int argc, TCHAR *argv[])
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
			"import rvth.img bank# disc.gcm\n"
			"- Import disc.gcm into rvth.img at the specified bank number.\n"
			"  The destination bank must be either empty or deleted.\n"
			"\n"
			"delete " DEVICE_NAME_EXAMPLE " bank#\n"
			"- Delete the specified bank number from the specified RVT-H device.\n"
			"  This does NOT wipe the disc image.\n"
			"  [This command only works with RVT-H Readers, not disk images.]\n"
			"\n"
			"undelete " DEVICE_NAME_EXAMPLE " bank#\n"
			"- Undelete the specified bank number from the specified RVT-H device.\n"
			"  [This command only works with RVT-H Readers, not disk images.]\n"
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
		// Extract a bank.
		if (argc < 5) {
			fputs("*** ERROR: Missing parameters for 'extract'.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = extract(argv[2], argv[3], argv[4]);
	} else if (!_tcscmp(argv[1], _T("import"))) {
		// Import a bank.
		if (argc < 5) {
			fputs("*** ERROR: Missing parameters for 'import'.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = import(argv[2], argv[3], argv[4]);
	} else if (!_tcscmp(argv[1], _T("delete"))) {
		// Delete a bank.
		if (argc < 4) {
			fputs("*** ERROR: Missing parameters for 'delete'.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = delete_bank(argv[2], argv[3]);
	} else if (!_tcscmp(argv[1], _T("undelete"))) {
		// Undelete a bank.
		if (argc < 4) {
			fputs("*** ERROR: Missing parameters for 'undelete'.\n", stderr);
			return EXIT_FAILURE;
		}
		ret = undelete_bank(argv[2], argv[3]);
	} else {
		// If the "command" contains a slash or dot (or backslash on Windows),
		// assume it's a filename and handle it as 'list'.
		const TCHAR *p = argv[1];
		bool isFilename = false;
		while (*p != 0) {
			if (*p == _T('/') || *p == _T('.')) {
				// Probably a filename.
				isFilename = true;
				break;
			}
#ifdef _WIN32
			else if (*p == '\\') {
				// Probably a filename on Windows.
				isFilename = true;
				break;
			}
#endif /* _WIN32 */
		}

		if (isFilename) {
			// Probably a filename.
			ret = list_banks(argv[1]);
		} else {
			// Not a filename.
			fputs("*** ERROR: Unrecognized command: '", stderr);
			_fputts(argv[1], stderr);
			fputs("'\n", stderr);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
