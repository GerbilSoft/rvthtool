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
#include <getopt.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// libwiicrypto
#include "libwiicrypto/sig_tools.h"

// stdboolx
#include "stdboolx.h"

#ifdef _WIN32
#  include "libwiicrypto/win32/secoptions.h"
#  include "libwiicrypto/win32/Win32_sdk.h"
#endif /* _WIN32 */

#include "resign-nus.hpp"
#include "print-info.hpp"

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
	fputs("This program is licensed under the GNU GPL v2.\n"
		"For more information, visit: http://www.gnu.org/licenses/\n"
		"\n", stdout);

	fputs("Syntax: ", stdout);
	_fputts(argv0, stdout);
	fputs(" [options] [command]\n"
		"\n"
		"Supported commands:\n"
		"\n"
		"info nusdir/\n"
		"- Print information about the specified NUS directory.\n"
		"\n"
		"resign nusdir/\n"
		" - Resigns the specified NUS directory in place.\n"
		"   Default converts Retail NUS to Debug, and Debug NUS to retail.\n"
		"\n"
		"verify nusdir/\n"
		" - Verify the content hashes.\n"
		"\n"
		"Options:\n"
		"\n"
		"  -k, --recrypt=KEY         Recrypt the WAD using the specified KEY:\n"
		"                            default, retail, debug\n"
		"                            Recrypting to retail will blank out the signatures.\n"
		"  -h, --help                Display this help and exit.\n"
		"\n"
		, stdout);
}

/**
 * Is the specified filename a directory?
 * @param filename Filename to check
 * @return True if it's a directory; false if not.
 */
static bool is_directory(const TCHAR *filename)
{
#ifdef _WIN32
	const DWORD dwAttrs = GetFileAttributes(filename);
	return (dwAttrs != INVALID_FILE_ATTRIBUTES && (dwAttrs & FILE_ATTRIBUTE_DIRECTORY));
#else /* !_WIN32 */
	// TODO: statx() if available?
	struct stat sb;
	int sret = stat(filename, &sb);
	return (sret == 0 && (sb.st_mode & S_IFDIR));
#endif /* _WIN32 */
}

int RVTH_CDECL _tmain(int argc, TCHAR *argv[])
{
	int ret;

	// Key to use for recryption.
	// -1 == default; convert from Retail to Debug or vice-versa.
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

	puts("NUS Resigner v" VERSION_STRING "\n"
		"Copyright (c) 2018-2020 by David Korth.");
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
			{_T("help"),	no_argument,		0, _T('h')},

			{NULL, 0, 0, 0}
		};

		int c = getopt_long(argc, argv, _T("k:h"), long_options, NULL);
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
				// FIXME: Add separate WUP_CryptoType_* enumerations?
				if (!_tcsicmp(optarg, _T("default"))) {
					recrypt_key = -1;
				} else if (!_tcsicmp(optarg, _T("debug"))) {
					recrypt_key = RVL_CryptoType_Debug;
				} else if (!_tcsicmp(optarg, _T("retail"))) {
					recrypt_key = RVL_CryptoType_Retail;
				} else {
					print_error(argv[0], _T("unknown encryption key '%s'"), optarg);
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
			print_error(argv[0], _T("NUS directory not specified"));
			return EXIT_FAILURE;
		}

		ret = 0;
		for (i = optind+1; i < argc; i++) {
			ret |= print_nus_info(argv[i], false);
		}
	} else if (!_tcscmp(argv[optind], _T("verify"))) {
		// Verify a WAD.
		int i;
		if (argc < optind+2) {
			print_error(argv[0], _T("NUS directory not specified"));
			return EXIT_FAILURE;
		}

		ret = 0;
		for (i = optind+1; i < argc; i++) {
			ret |= print_nus_info(argv[i], true);
		}
	} else if (!_tcscmp(argv[optind], _T("resign"))) {
		// Resign an NUS directory.
		if (argc < optind+2) {
			print_error(argv[0], _T("NUS directory not specified"));
			return EXIT_FAILURE;
		}
		ret = resign_nus(argv[optind+1], recrypt_key);
	} else {
		// If the "command" corresponds to a valid directory,
		// assume it's a filename and handle it as 'info'.
		if (is_directory(argv[optind])) {
			// It's a directory.
			int i;
			ret = 0;
			for (i = optind; i < argc; i++) {
				ret |= print_nus_info(argv[i], false);
			}
		} else {
			// Not a filename.
			print_error(argv[0], _T("unrecognized command '%s'"), argv[optind]);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
