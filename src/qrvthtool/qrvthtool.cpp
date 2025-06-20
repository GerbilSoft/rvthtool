/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * qrvthtool.cpp: Main program.                                            *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.qrvthtool.h"

#include "windows/QRvtHToolWindow.hpp"

// Qt includes
#include <QApplication>
#include <QtCore/QDir>

// KDE
#ifdef HAVE_KF_CoreAddons
#  include <KAboutData>
#endif /* HAVE_KF_CoreAddons */

// TranslationManager
#include "TranslationManager.hpp"

#ifdef _WIN32
#  include <windows.h>
#  include <tchar.h>
extern UINT WM_TaskbarButtonCreated;
UINT WM_TaskbarButtonCreated;

// MSGFLT_ADD (requires _WIN32_WINNT >= 0x0600)
#ifndef MSGFLT_ADD
#  define MSGFLT_ADD 1
#endif

// VersionHelpers
#include "rp_versionhelpers.h"

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

#ifdef _WIN32
	// Windows: Set the "fusion" theme if using Windows 10 or later.
	// This is needed for proper Dark Mode support.
	// NOTE: It's recommended to set the style *before* creating the QApplication.
	if (IsWindows10OrGreater()) {
		QApplication::setStyle(QLatin1String("fusion"));
	}
#endif /* _WIN32 */

	QApplication *const app = new QApplication(argc, argv);
	app->setApplicationName(QStringLiteral("qrvthtool"));
	app->setOrganizationDomain(QStringLiteral("gerbilsoft.com"));
	app->setOrganizationName(QStringLiteral("GerbilSoft"));
	app->setApplicationVersion(QStringLiteral(VERSION_STRING));

	app->setApplicationDisplayName(QStringLiteral("RVT-H Tool"));
#if QT_VERSION >= QT_VERSION_CHECK(5,7,0)
	app->setDesktopFileName(QStringLiteral("com.gerbilsoft.qrvthtool"));
#endif /* QT_VERSION >= QT_VERSION_CHECK(5,7,0) */

#ifdef HAVE_KF_CoreAddons
	// Set up KAboutData.
	QString applicationDisplayName = QStringLiteral("RVT-H Tool");
	KAboutData aboutData(
		app->applicationName(),		// componentName
		applicationDisplayName,		// displayName
		app->applicationVersion(),	// version
		applicationDisplayName,		// shortDescription (TODO: Better value?)
		KAboutLicense::GPL_V2,		// licenseType
		QStringLiteral("Copyright (c) 2018-2025 by David Korth."),	// copyrightStatement
		QString(),			// otherText
		QLatin1String("https://github.com/GerbilSoft/rvthtool"),	// homePageAddress
		QLatin1String("https://github.com/GerbilSoft/rvthtool/issues")	// bugAddress
	);
	KAboutData::setApplicationData(aboutData);
#endif /* HAVE_KF_CoreAddons */

	// Initialize KCrash.
	// FIXME: It shows bugs.kde.org as the bug reporting address, which isn't wanted...
	//KCrash::initialize();

	// Initialize the TranslationManager.
	TranslationManager *const tsm = TranslationManager::instance();
	tsm->setTranslation(QLocale::system().name());

	// TODO: Call QApplication::setWindowIcon().

#ifdef _WIN32
	RegisterTaskbarButtonCreatedMessage();
#endif /* _WIN32 */

#if defined(_WIN32) || defined(__APPLE__)
	// Use the built-in Oxygen icon theme.
	// Reference: http://tkrotoff.blogspot.com/2010/02/qiconfromtheme-under-windows.html

	// NOTE: On Windows 10, Qt6 has a built-in monochrome icon theme,
	// but it's missing some icons.
	QIcon::setThemeName(QStringLiteral("oxygen"));
#endif /* _WIN32 || __APPLE__ */

	// Initialize the QRvtHToolWindow.
	QRvtHToolWindow *window = new QRvtHToolWindow();

	// Show the window.
	window->show();
	QCoreApplication::processEvents();

	// If a filename was specified, open it.
	QStringList args = app->arguments();
	if (args.size() >= 2) {
		// TODO: Better device file check.
		const QString &filename = args.at(1);
#ifdef _WIN32
		bool isDevice = filename.startsWith(QStringLiteral("\\\\.\\PhysicalDrive"), Qt::CaseInsensitive);
#else /* !_WIN32 */
		bool isDevice = filename.startsWith(QStringLiteral("/dev/"));
#endif /* _WIN32 */
		window->openRvtH(QDir::fromNativeSeparators(filename), isDevice);
	}

	// Run the Qt UI.
	return app->exec();
}
