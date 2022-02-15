/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * rvth_time.c: RVT-H timestamp functions.                                 *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "rvth_time.h"
#include "libwiicrypto/common.h"

#include "time_r.h"

// C includes
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Parse an RVT-H timestamp.
 * @param buf	[in] Pointer to NHCD timestamp string. (Must be 14 characters; NOT NULL-terminated.)
 * @return Timestamp, or -1 if invalid.
 */
time_t rvth_timestamp_parse(const char *buf)
{
	// Date format: "YYYYMMDD"
	// Time format: "HHMMSS"
	unsigned int ymd = 0;
	unsigned int hms = 0;
	unsigned int i;
	struct tm ymdtime;

	// Convert the date to an unsigned integer.
	for (i = 0; i < 8; i++) {
		if (unlikely(!isdigit(buf[i]))) {
			// Invalid digit.
			return -1;
		}
		ymd *= 10;
		ymd += (buf[i] & 0xF);
	}

	// Sanity checks:
	// - Must be higher than 19000101.
	// - Must be lower than 99991231.
	if (unlikely(ymd < 19000101 || ymd > 99991231)) {
		// Invalid date.
		return -1;
	}

	// Convert the time to an unsigned integer.
	for (i = 8; i < 14; i++) {
		if (unlikely(!isdigit(buf[i]))) {
			// Invalid digit.
			return -1;
		}
		hms *= 10;
		hms += (buf[i] & 0xF);
	}

	// Sanity checks:
	// - Must be lower than 235959.
	if (unlikely(hms > 235959)) {
		// Invalid time.
		return -1;
	}

	// Convert to Unix time.
	// NOTE: struct tm has some oddities:
	// - tm_year: year - 1900
	// - tm_mon: 0 == January
	ymdtime.tm_year = (ymd / 10000) - 1900;
	ymdtime.tm_mon  = ((ymd / 100) % 100) - 1;
	ymdtime.tm_mday = ymd % 100;

	ymdtime.tm_hour = (hms / 10000);
	ymdtime.tm_min  = (hms / 100) % 100;
	ymdtime.tm_sec  = hms % 100;

	// tm_wday and tm_yday are output variables.
	ymdtime.tm_wday = 0;
	ymdtime.tm_yday = 0;
	ymdtime.tm_isdst = 0;

	// If conversion fails, this will return -1.
	return timegm(&ymdtime);
}

/**
 * Create an RVT-H timestamp.
 * @param buf	[out] Pointer to NHCD timestamp buffer. (Must be 14 characters; will NOT be NULL-terminated.)
 * @param size	[in] Size of nhcd_timestamp.
 * @param now	[in] Time to set.
 * @return 0 on success; negative POSIX error code on error.
 */
int rvth_timestamp_create(char *buf, size_t size, time_t now)
{
	struct tm tm_now;

	// Timestamp buffer.
	// We can't snprintf() directly to nhcd_entry.timestamp because
	// gcc will complain about buffer overflows, and will crash at
	// runtime on Gentoo Hardened.
	char tsbuf[16];

	// Validate the parameters.
	if (!buf || size == 0) {
		return -EINVAL;
	} else if (size > 14) {
		size = 14;
	}

	localtime_r(&now, &tm_now);
	snprintf(tsbuf, sizeof(tsbuf), "%04d%02d%02d%02d%02d%02d",
		tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday,
		tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
	memcpy(buf, tsbuf, size);
	return 0;
}
