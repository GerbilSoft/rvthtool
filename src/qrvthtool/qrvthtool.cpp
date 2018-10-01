/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * qrvthtool.cpp: Main program.                                            *
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

#include "windows/QRvtHToolWindow.hpp"

// Qt includes.
#include <QApplication>
#include <QtCore/QDir>

// TranslationManager
#include "TranslationManager.hpp"

/**
 * Main entry point.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return Return value.
 */
int main(int argc, char *argv[])
{
	// Enable High DPI.
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#if QT_VERSION >= 0x050600
	// Enable High DPI pixmaps.
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
#else
	// Hardcode the value in case the user upgrades to Qt 5.6 later.
	// http://doc.qt.io/qt-5/qt.html#ApplicationAttribute-enum
	QApplication::setAttribute((Qt::ApplicationAttribute)13, true);
#endif /* QT_VERSION >= 0x050600 */

	QApplication *app = new QApplication(argc, argv);

	// Initialize the TranslationManager.
	TranslationManager *const tsm = TranslationManager::instance();
	tsm->setTranslation(QLocale::system().name());

	// Initialize the QRvtHToolWindow.
	QRvtHToolWindow *window = new QRvtHToolWindow();

	// If a filename was specified, open it.
	QStringList args = app->arguments();
	if (args.size() >= 2) {
		window->openRvtH(QDir::fromNativeSeparators(args.at(1)));
	}

	// Show the window.
	window->show();

	// Run the Qt UI.
	return app->exec();
}
