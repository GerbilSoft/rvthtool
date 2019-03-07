/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * main.c: Main program file.                                              *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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
#include "git.h"

// C includes.
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "libwiicrypto/sig_tools.h"

#ifdef _WIN32
# include "libwiicrypto/win32/secoptions.h"
#endif /* _WIN32 */

#include "print-info.h"
#include "resign-wad.h"

#ifdef _MSC_VER
# define RVTH_CDECL __cdecl
#else
# define RVTH_CDECL
#endif

#ifdef __GNUC__
# define ATTR_PRINTF(fmt, args) __attribute__ ((format (printf, (fmt), (args))))
#else
# define ATTR_PRINTF(fmt, args)
#endif

/**
 * Print an error message.
 * @param argv0 Program name.
 * @param fmt Format string.
 * @param ... Arguments.
 */
static void ATTR_PRINTF(2, 3) print_error(const TCHAR *argv0, const TCHAR *fmt, ...)
{
	if (fmt != NULL) {
		va_list ap;
		va_start(ap, fmt);
		_ftprintf(stderr, _T("%s: "), argv0);
		_vftprintf(stderr, fmt, ap);
		va_end(ap);

		fputc('\n', stderr);
	}

	_ftprintf(stderr, _T("Try `%s` --help` for more information.\n"), argv0);
}

/**
 * Print program help.
 * @param argv0 Program name.
 */
static void print_help(const TCHAR *argv0)
{
	fputs("This program is licensed under the GNU GPL v2.\n"
		"For more information, visit: http://www.gnu.org/licenses/\n"
		"\n", stdout);

	fputs("Syntax: ", stdout);
	_fputts(argv0, stdout);
	fputs(" [options] [command]\n"
		"\n"
		"Supported commands:\n"
		"\n"
		"info file.wad\n"
		"- Print information about the specified WAD file.\n"
		"\n"
		"resign source.wad dest.wad\n"
		" - Resigns source.wad and creates dest.wad using the new key.\n"
		"   Default converts Retail/Korean WADs to Debug, and\n"
		"   Debug WADs to Retail.\n"
		"\n"
		"verify file.wad\n"
		" - Verify the content hashes.\n"
		"\n"
		"Options:\n"
		"\n"
		"  -k, --recrypt=KEY         Recrypt the WAD using the specified KEY:\n"
		"                            default, retail, korean, debug\n"
		"                            Recrypting to retail will use fakesigning.\n"
		"  -h, --help                Display this help and exit.\n"
		"\n"
		, stdout);
}

int RVTH_CDECL _tmain(int argc, TCHAR *argv[])
{
	int ret;

	// Key to use for recryption.
	// -1 == default; no recryption, except when importing retail to RVT-H.
	// Other values are from RVL_CryptoType_e.
	int recrypt_key = -1;

	((void)argc);
	((void)argv);

#ifdef _WIN32
	// Set Win32 security options.
	secoptions_init();
#endif /* _WIN32 */

	// Set the C locale.
	setlocale(LC_ALL, "");

	puts("WAD Resigner v" VERSION_STRING "\n"
		"Copyright (c) 2018-2019 by David Korth.");
#ifdef RP_GIT_VERSION
	puts(RP_GIT_VERSION);
# ifdef RP_GIT_DESCRIBE
	puts(RP_GIT_DESCRIBE);
# endif
#endif
	putchar('\n');

	// Using Unicode getopt() for Windows:
	// - https://www.codeproject.com/Articles/157001/Full-getopt-Port-for-Unicode-and-Multibyte-Microso
	while (true) {
		static const struct option long_options[] = {
			{_T("recrypt"),	required_argument,	0, _T('k')},
			{_T("ndev"),	no_argument,		0, _T('N')},
			{_T("help"),	no_argument,		0, _T('h')},

			{NULL, 0, 0, 0}
		};

		int c = getopt_long(argc, argv, _T("k:Nh"), long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
			case _T('k'):
				// Recryption method.
				if (!optarg) {
					// NULL?
					print_error(argv[0], _T("no recryption method specified"));
					return EXIT_FAILURE;
				}
				if (!_tcsicmp(optarg, _T("default"))) {
					recrypt_key = -1;
				} else if (!_tcsicmp(optarg, _T("debug"))) {
					recrypt_key = RVL_CryptoType_Debug;
				} else if (!_tcsicmp(optarg, _T("retail"))) {
					recrypt_key = RVL_CryptoType_Retail;
				} else if (!_tcsicmp(optarg, _T("korean"))) {
					recrypt_key = RVL_CryptoType_Korean;
				} else {
					print_error(argv[0], _T("unknown encryption key '%s'"), optarg);
					return EXIT_FAILURE;
				}
				break;

			case 'h':
				print_help(argv[0]);
				return EXIT_SUCCESS;

			case '?':
			default:
				print_error(argv[0], NULL);
				return EXIT_FAILURE;
		}
	}

	// First argument after getopt-parsed arguments is set in optind.
	if (optind >= argc) {
		print_error(argv[0], _T("no parameters specified"));
		return EXIT_FAILURE;
	}

	// Check the specified command.
	// TODO: Better help if the command parameters are invalid.
	if (!_tcscmp(argv[optind], _T("help"))) {
		// Display help.
		print_help(argv[0]);
		return EXIT_FAILURE;
	} else if (!_tcscmp(argv[optind], _T("info"))) {
		// Print WAD information.
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filename not specified"));
			return EXIT_FAILURE;
		}
		ret = print_wad_info(argv[optind+1], false);
	} else if (!_tcscmp(argv[optind], _T("verify"))) {
		// Verify a WAD.
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filename not specified"));
			return EXIT_FAILURE;
		}
		ret = print_wad_info(argv[optind+1], true);
	} else if (!_tcscmp(argv[optind], _T("resign"))) {
		// Resign a WAD.
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filenames not specified"));
			return EXIT_FAILURE;
		} else if (argc < optind+3) {
			print_error(argv[0], _T("Output WAD filename not specified"));
			return EXIT_FAILURE;
		}
		ret = resign_wad(argv[optind+1], argv[optind+2], recrypt_key);
	} else {
		// If the "command" contains a slash or dot (or backslash on Windows),
		// assume it's a filename and handle it as 'info'.
		// TODO: If two filenames are specified, handle it as 'resign'.
		const TCHAR *p;
		bool isFilename = false;
		for (p = argv[optind]; *p != 0; p++) {
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
			ret = print_wad_info(argv[optind], false);
		} else {
			// Not a filename.
			print_error(argv[0], _T("unrecognized command '%s'"), argv[optind]);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
