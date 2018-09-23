/***************************************************************************
 * RVT-H Tool                                                              *
 * query.c: Query available RVT-H Reader devices.                          *
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

#include "query.h"

// FIXME: Better way to detect if querying is available.
#include "librvth/config.librvth.h"

#ifdef HAVE_QUERY

#include "libwiicrypto/common.h"
#include "librvth/query.h"

#include <stdio.h>

static inline int calc_frac_part(int64_t size, int64_t mask)
{
	float f = (float)(size & (mask - 1)) / (float)mask;
	int frac_part = (int)(f * 1000.0f);

	// MSVC added round() and roundf() in MSVC 2013.
	// Use our own rounding code instead.
	int round_adj = (frac_part % 10 > 5);
	frac_part /= 10;
	frac_part += round_adj;
	return frac_part;
}

/**
 * Format a block device size.
 * @param buf		[out] Output buffer.
 * @param buf_size	[in] Size of buf.
 * @param size		[in] Block device size.
 */
static void format_size(char *buf, size_t buf_size, int64_t size)
{
	const char *suffix;
	// frac_part is always 0 to 100.
	// If whole_part >= 10, frac_part is divided by 10.
	int whole_part, frac_part;

	// TODO: Optimize this?
	if (size < 0) {
		// Invalid size. Print the value as-is.
		suffix = NULL;
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 10)) {
		// tr: Bytes (< 1,024)
		suffix = (size == 1 ? "byte" : "bytes");
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 20)) {
		// tr: Kilobytes
		suffix = "KB";
		whole_part = (int)(size >> 10);
		frac_part = calc_frac_part(size, (1LL << 10));
	} else if (size < (2LL << 30)) {
		// tr: Megabytes
		suffix = "MB";
		whole_part = (int)(size >> 20);
		frac_part = calc_frac_part(size, (1LL << 20));
	} else if (size < (2LL << 40)) {
		// tr: Gigabytes
		suffix = "GB";
		whole_part = (int)(size >> 30);
		frac_part = calc_frac_part(size, (1LL << 30));
	} else if (size < (2LL << 50)) {
		// tr: Terabytes
		suffix = "TB";
		whole_part = (int)(size >> 40);
		frac_part = calc_frac_part(size, (1LL << 40));
	} else if (size < (2LL << 60)) {
		// tr: Petabytes
		suffix = "PB";
		whole_part = (int)(size >> 50);
		frac_part = calc_frac_part(size, (1LL << 50));
	} else /*if (size < (2ULL << 70))*/ {
		// tr: Exabytes
		suffix = "EB";
		whole_part = (int)(size >> 60);
		frac_part = calc_frac_part(size, (1LL << 60));
	}

	if (size < (2LL << 10)) {
		// Bytes or negative value. No fractional part.
		if (suffix) {
			snprintf(buf, buf_size, "%d %s", whole_part, suffix);
		} else {
			snprintf(buf, buf_size, "%d", whole_part);
		}
	} else {
		// TODO: Localized decimal point?
		int frac_digits = 2;
		if (whole_part >= 10) {
			int round_adj = (frac_part % 10 > 5);
			frac_part /= 10;
			frac_part += round_adj;
			frac_digits = 1;
		}
		if (suffix) {
			snprintf(buf, buf_size, "%d.%0*d %s",
				whole_part, frac_digits, frac_part, suffix);
		} else {
			snprintf(buf, buf_size, "%d.%0*d",
				whole_part, frac_digits, frac_part);
		}
	}
}

/**
 * 'query' command.
 * @return 0 on success; non-zero on error.
 */
int query(void)
{
	// TODO: Differentiate between "no devices" and "error".
	RvtH_QueryEntry *devs, *p;
	char hdd_size[32];
	bool did_one = false;

	devs = rvth_query_devices();
	if (!devs) {
		printf("No RVT-H Reader devices found.\n");
		return 0;
	}

	printf("Detected RVT-H Reader devices:\n\n");

	p = devs;
	for (; p != NULL; p = p->next) {
		if (!p->device_name) {
			// No device name. Skip it.
			continue;
		}

		if (did_one) {
			fputs("\n", stdout);
		} else {
			did_one = true;
		}

		printf("%s\n", p->device_name);
		printf("- Manufacturer:  %s\n", (p->usb_vendor ? p->usb_vendor : "(unknown)"));
		printf("- Product Name:  %s\n", (p->usb_product ? p->usb_product : "(unknown)"));
		printf("- Serial Number: %s\n", (p->serial_number ? p->serial_number : "(unknown)"));
		printf("- HDD Firmware:  %s\n", (p->fw_version ? p->fw_version : "(unknown)"));
		// TODO: Trim the vendor/model fields if necessary.
		printf("- HDD Vendor:    %s\n", (p->hdd_vendor ? p->hdd_vendor : "(unknown)"));
		printf("- HDD Model:     %s\n", (p->hdd_model ? p->hdd_model : "(unknown)"));

		// Format and print the HDD size.
		format_size(hdd_size, sizeof(hdd_size), p->size);
		printf("- HDD Size:      %s\n", hdd_size);
	}

	rvth_query_free(devs);
	return 0;
}

#else /* !HAVE_QUERY */

#include <stdio.h>
#include <errno.h>

/**
 * 'query' command.
 * @return 0 on success; non-zero on error.
 */
int query(void)
{
	fputs("*** ERROR: Querying devices is not available on this system.\n", stderr);
	return -ENOSYS;
}

#endif /* HAVE_QUERY */
