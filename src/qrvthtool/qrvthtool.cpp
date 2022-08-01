/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * qrvthtool.cpp: Main program.                                            *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.qrvthtool.h"

#include "windows/QRvtHToolWindow.hpp"

// Qt includes.
#include <QApplication>
#include <QtCore/QDir>

// TranslationManager
#include "TranslationManager.hpp"

#ifdef _WIN32
# include <windows.h>
# include <tchar.h>
extern UINT WM_TaskbarButtonCreated;
UINT WM_TaskbarButtonCreated;

// MSGFLT_ADD (requires _WIN32_WINNT >= 0x0600)
#ifndef MSGFLT_ADD
# define MSGFLT_ADD 1
#endif

/**
 * Register the TaskbarButtonCreated message.
 */
static void RegisterTaskbarButtonCreatedMessage(void)
{
	WM_TaskbarButtonCreated = RegisterWindowMessage(_T("TaskbarButtonCreated"));

	// Enable taskbar messages even if running elevated.
	// Elevated privileges are necessary for raw device
	// access in most cases.
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dd388202(v=vs.85).aspx
	typedef BOOL (WINAPI *PFNCHANGEWINDOWMESSAGEFILTER)(__in UINT message, __in DWORD dwFlag);
	PFNCHANGEWINDOWMESSAGEFILTER pfn = nullptr;
	HMODULE hUser32 = GetModuleHandle(_T("user32"));
	if (hUser32) {
		pfn = reinterpret_cast<PFNCHANGEWINDOWMESSAGEFILTER>(GetProcAddress(
			hUser32, "ChangeWindowMessageFilter"));
		if (pfn) {
			// Found the function.
			// Enable taskbar messages.
			pfn(WM_TaskbarButtonCreated, MSGFLT_ADD);
			pfn(WM_COMMAND, MSGFLT_ADD);
			pfn(WM_SYSCOMMAND, MSGFLT_ADD);
			pfn(WM_ACTIVATE, MSGFLT_ADD);
		}
	}
}
#endif /* _WIN32 */

/**
 * Main entry point.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return Return value.
 */
int main(int argc, char *argv[])
{
	// Set high-DPI mode on Qt 5. (not needed on Qt 6)
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0) && QT_VERSION < QT_VERSION_CHECK(6,0,0)
	// Enable High DPI.
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#  if QT_VERSION >= 0x050600
	// Enable High DPI pixmaps.
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
#  else
	// Hardcode the value in case the user upgrades to Qt 5.6 later.
	// http://doc.qt.io/qt-5/qt.html#ApplicationAttribute-enum
	QApplication::setAttribute((Qt::ApplicationAttribute)13, true);
#  endif /* QT_VERSION >= 0x050600 */
#endif /* QT_VERSION >= QT_VERSION_CHECK(5,0,0) && QT_VERSION < QT_VERSION_CHECK(6,0,0) */

	QApplication *app = new QApplication(argc, argv);
	app->setApplicationName(QLatin1String("qrvthtool"));
	app->setOrganizationDomain(QLatin1String("gerbilsoft.com"));
	app->setOrganizationName(QLatin1String("GerbilSoft"));
	app->setApplicationVersion(QLatin1String(VERSION_STRING));

	app->setApplicationDisplayName(QLatin1String("RVT-H Tool"));
#if QT_VERSION >= QT_VERSION_CHECK(5,7,0)
	app->setDesktopFileName(QLatin1String("qrvthtool.desktop"));
#endif /* QT_VERSION >= QT_VERSION_CHECK(5,7,0) */

	// Initialize the TranslationManager.
	TranslationManager *const tsm = TranslationManager::instance();
	tsm->setTranslation(QLocale::system().name());

	// TODO: Call QApplication::setWindowIcon().

#ifdef _WIN32
	RegisterTaskbarButtonCreatedMessage();
#endif /* _WIN32 */

#if defined(_WIN32) || defined(__APPLE__)
	// Check if an icon theme is available.
	if (!QIcon::hasThemeIcon(QLatin1String("application-exit"))) {
		// Icon theme is not available.
		// Use the built-in Oxygen icon theme.
		// Reference: http://tkrotoff.blogspot.com/2010/02/qiconfromtheme-under-windows.html
		QIcon::setThemeName(QLatin1String("oxygen"));
	}
#endif /* _WIN32 || __APPLE__ */

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
