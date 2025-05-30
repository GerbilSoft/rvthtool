/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * FormatSize.hpp: Format file sizes.                                      *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

// C includes (C++ namespace)
#include <cstdint>

// Qt includes
#include <QtCore/QString>

/**
 * Format a file size.
 * @param size	[in] File size
 * @return Formatted file size
 */
QString formatSize(off64_t size);
