/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * FormatSize.hpp: Format file sizes.                                      *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "FormatSize.hpp"

// Qt includes
#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>

static inline int calc_frac_part(off64_t size, off64_t mask)
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
 * Format a file size.
 * @param size	[in] File size
 * @return Formatted file size
 */
QString formatSize(off64_t size)
{
	// frac_part is always 0 to 100.
	// If whole_part >= 10, frac_part is divided by 10.
	int whole_part, frac_part;

	// TODO: Optimize this?
	const QLocale sysLocale = QLocale::system();
	QString suffix;
	if (size < 0) {
		// Invalid size. Print the value as-is.
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 10)) {
		// tr: Bytes (< 1,024)
		suffix = QCoreApplication::translate("FormatSize", "byte(s)", nullptr, size);
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 20)) {
		// tr: Kilobytes
		suffix = QCoreApplication::translate("FormatSize", "KiB");
		whole_part = (int)(size >> 10);
		frac_part = calc_frac_part(size, (1LL << 10));
	} else if (size < (2LL << 30)) {
		// tr: Megabytes
		suffix = QCoreApplication::translate("FormatSize", "MiB");
		whole_part = (int)(size >> 20);
		frac_part = calc_frac_part(size, (1LL << 20));
	} else if (size < (2LL << 40)) {
		// tr: Gigabytes
		suffix = QCoreApplication::translate("FormatSize", "GiB");
		whole_part = (int)(size >> 30);
		frac_part = calc_frac_part(size, (1LL << 30));
	} else if (size < (2LL << 50)) {
		// tr: Terabytes
		suffix = QCoreApplication::translate("FormatSize", "TiB");
		whole_part = (int)(size >> 40);
		frac_part = calc_frac_part(size, (1LL << 40));
	} else if (size < (2LL << 60)) {
		// tr: Petabytes
		suffix = QCoreApplication::translate("FormatSize", "PiB");
		whole_part = (int)(size >> 50);
		frac_part = calc_frac_part(size, (1LL << 50));
	} else /*if (size < (2ULL << 70))*/ {
		// tr: Exabytes
		suffix = QCoreApplication::translate("FormatSize", "EiB");
		whole_part = (int)(size >> 60);
		frac_part = calc_frac_part(size, (1LL << 60));
	}

	QString s_value = sysLocale.toString(whole_part);
	if (size >= (2LL << 10)) {
		// KiB or larger. There is a fractional part.
		int frac_digits = 2;
		if (whole_part >= 10) {
			int round_adj = (frac_part % 10 > 5);
			frac_part /= 10;
			frac_part += round_adj;
			frac_digits = 1;
		}

		char fdigit[12];
		snprintf(fdigit, sizeof(fdigit), "%0*d", frac_digits, frac_part);
		s_value += sysLocale.decimalPoint();
		s_value += QLatin1String(fdigit);
	}

	if (!suffix.isEmpty()) {
		// Suffix is present.
		//: %1 == localized value, %2 == suffix (e.g. MiB)
		return QCoreApplication::translate("FormatSize", "%1 %2")
			.arg(s_value, suffix);
	}

	// No suffix.
	return s_value;
}
