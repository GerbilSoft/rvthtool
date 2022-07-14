/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * main.c: Main program file.                                              *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.version.h"
#include "git.h"

// C includes
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "libwiicrypto/sig_tools.h"

#ifdef _WIN32
#  include "libwiicrypto/win32/secoptions.h"
#endif /* _WIN32 */

#include "print-info.h"
#include "resign-wad.hpp"

#ifdef __GNUC__
#  define ATTR_PRINTF(fmt, args) __attribute__ ((format (printf, (fmt), (args))))
#else
#  define ATTR_PRINTF(fmt, args)
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
	_fputts(_T("This program is licensed under the GNU GPL v2.\n")
		_T("For more information, visit: http://www.gnu.org/licenses/\n")
		_T("\n"), stdout);

	_tprintf(_T("Syntax: %s [options] [command]\n\n"), argv0);
	_fputts(_T("Supported commands:\n")
		_T("\n")
		_T("info file.wad\n")
		_T("- Print information about the specified WAD file.\n")
		_T("\n")
		_T("resign source.wad dest.wad\n")
		_T("- Resigns source.wad and creates dest.wad using the new key.\n")
		_T("  Default converts Retail/Korean WADs to Debug, and\n")
		_T("  Debug WADs to Retail. The format isn't changed unless\n")
		_T("  the --format parameter is specified.\n")
		_T("\n")
		_T("verify file.wad\n")
		_T("- Verify the content hashes.\n")
		_T("\n")
		_T("Options:\n")
		_T("\n")
		_T("  -k, --recrypt=KEY         Recrypt the WAD using the specified KEY:\n")
		_T("                            default, retail, korean, vWii, debug\n")
		_T("                            Recrypting to retail will use fakesigning.\n")
		_T("  -f, --format=FMT          Use the specified format FMT:\n")
		_T("                            default, wad, bwf\n")
		_T("  -h, --help                Display this help and exit.\n")
		_T("\n"), stdout);
}

int RVTH_CDECL _tmain(int argc, TCHAR *argv[])
{
	int ret;

	// Key to use for recryption.
	// -1 == default; no recryption, except when importing retail to RVT-H.
	// Other values are from RVL_CryptoType_e.
	int recrypt_key = -1;

	// Output format.
	// -1 == default: standard .wad format.
	// Other values are from WAD_Format_e.
	int output_format = -1;

	((void)argc);
	((void)argv);

#ifdef _WIN32
	// Set Win32 security options.
	secoptions_init();
#endif /* _WIN32 */

	// Set the C locale.
	setlocale(LC_ALL, "");

	_fputts(_T("WAD Resigner v") _T(VERSION_STRING) _T("\n")
		_T("Copyright (c) 2018-2022 by David Korth.\n"), stdout);
	fputs(
#ifdef RP_GIT_VERSION
		_T(RP_GIT_VERSION) _T("\n")
#  ifdef RP_GIT_DESCRIBE
		_T(RP_GIT_DESCRIBE) _T("\n")
#  endif
#endif
		_T("\n"), stdout);

	// Using Unicode getopt() for Windows:
	// - https://www.codeproject.com/Articles/157001/Full-getopt-Port-for-Unicode-and-Multibyte-Microso
	while (true) {
		static const struct option long_options[] = {
			{_T("recrypt"),	required_argument,	0, _T('k')},
			{_T("format"),	required_argument,	0, _T('f')},
			{_T("ndev"),	no_argument,		0, _T('N')},
			{_T("help"),	no_argument,		0, _T('h')},

			{NULL, 0, 0, 0}
		};

		int c = getopt_long(argc, argv, _T("k:f:Nh"), long_options, NULL);
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
				} else if (!_tcsicmp(optarg, _T("vWii"))) {
					recrypt_key = RVL_CryptoType_vWii;
				} else {
					print_error(argv[0], _T("unknown encryption key '%s'"), optarg);
					return EXIT_FAILURE;
				}
				break;

			case _T('f'):
				// Output format.
				if (!optarg) {
					// NULL?
					print_error(argv[0], _T("no output format specified"));
					return EXIT_FAILURE;
				}
				if (!_tcsicmp(optarg, _T("default"))) {
					output_format = -1;
				} else if (!_tcsicmp(optarg, _T("wad"))) {
					output_format = WAD_Format_Standard;
				} else if (!_tcsicmp(optarg, _T("bwf"))) {
					output_format = WAD_Format_BroadOn;
				} else {
					print_error(argv[0], _T("unknown output format '%s'"), optarg);
					return EXIT_FAILURE;
				}
				break;

			case _T('h'):
				print_help(argv[0]);
				return EXIT_SUCCESS;

			case _T('?'):
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
		int i;
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filename not specified"));
			return EXIT_FAILURE;
		}

		ret = 0;
		for (i = optind+1; i < argc; i++) {
			ret |= print_wad_info(argv[i], false);
		}
	} else if (!_tcscmp(argv[optind], _T("verify"))) {
		// Verify a WAD.
		int i;
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filename not specified"));
			return EXIT_FAILURE;
		}

		ret = 0;
		for (i = optind+1; i < argc; i++) {
			ret |= print_wad_info(argv[i], true);
		}
	} else if (!_tcscmp(argv[optind], _T("resign"))) {
		// Resign a WAD.
		if (argc < optind+2) {
			print_error(argv[0], _T("WAD filenames not specified"));
			return EXIT_FAILURE;
		} else if (argc < optind+3) {
			print_error(argv[0], _T("Output WAD filename not specified"));
			return EXIT_FAILURE;
		}
		ret = resign_wad(argv[optind+1], argv[optind+2], recrypt_key, output_format);
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
			int i;
			ret = 0;
			for (i = optind; i < argc; i++) {
				ret |= print_wad_info(argv[i], false);
			}
		} else {
			// Not a filename.
			print_error(argv[0], _T("unrecognized command '%s'"), argv[optind]);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
