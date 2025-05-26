/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * PathFuncs.hpp: Path functions.                                          *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

// Qt includes.
#include <QtCore/qglobal.h>
#include <QtCore/QString>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

namespace PathFuncs {

/**
 * Make a path relative to a base path it's contained in.
 * @param basePath Base path.
 * @param filePath Path to make relative to basePath.
 * @param prefix Prefix to use for relative paths.
 * @return Relative path, or absolute path if filePath is not contained within basePath.
 */
QString makeRelative(const QString &basePath, const QString &filePath,
	const QString &prefix = QLatin1String("./"));

/**
 * Make a path relative to the application directory.
 * @param filePath Path to make relative to the application directory. (MUST BE ABSOLUTE!)
 * @return Relative path, or absolute path if filePath is not contained within the application directory.
 */
static inline QString makeRelativeToApplication(const QString &filePath)
{
	return makeRelative(QCoreApplication::applicationDirPath(), filePath);
}

/**
 * Make a path relative to the user's home directory.
 * @param filePath Path to make relative to the user's home directory. (MUST BE ABSOLUTE!)
 * @return Relative path, or absolute path if filePath is not contained within the user's home directory.
 */
static inline QString makeRelativeToHome(const QString &filePath)
{
	return makeRelative(QDir::home().absolutePath(),
			    filePath, QLatin1String("~/"));
}

} // namespace PathFuncs
