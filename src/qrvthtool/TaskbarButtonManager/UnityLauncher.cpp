/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * UnityLauncher.cpp: Unity Launcher implementation.                       *
 *                                                                         *
 * Copyright (c) 2013-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// References:
// - https://github.com/AppImage/AppImageKit/issues/256
// - https://daniel.molkentin.net/2012/09/23/badge-and-progress-support-in-qt-creator/
// - https://codereview.qt-project.org/#/c/33051/2/src/plugins/coreplugin/progressmanager/progressmanager_x11.cpp

#include "UnityLauncher.hpp"

// Qt includes.
#include <QtWidgets/QWidget>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

#define DESKTOP_FILENAME "com.gerbilsoft.qrvthtool.desktop"

/** UnityLauncherPrivate **/

#include "TaskbarButtonManager_p.hpp"
class UnityLauncherPrivate : public TaskbarButtonManagerPrivate
{
public:
	explicit UnityLauncherPrivate(UnityLauncher *const q)
		: super(q)
	{ }

private:
	typedef TaskbarButtonManagerPrivate super;
	Q_DECLARE_PUBLIC(UnityLauncher)
	Q_DISABLE_COPY(UnityLauncherPrivate)

public:
	static inline void sendMessage(const QVariantMap &map)
	{
		QDBusMessage signal = QDBusMessage::createSignal(
			QStringLiteral("/"),
			QStringLiteral("com.canonical.Unity.LauncherEntry"),
			QStringLiteral("Update"));

		// Set the application ID
		signal << QStringLiteral("application://" DESKTOP_FILENAME);

		// Set the properties
		signal << map;

		// Send the signal
		if (!QDBusConnection::sessionBus().send(signal)) {
			qWarning("Unable to send message");
		}
	}

	template<typename T>
	static void sendMessage(const char *name, const T &value)
	{
		QVariantMap map;
		map.insert(QLatin1String(name), value);
		sendMessage(map);
	}
};

/** UnityLauncher **/

UnityLauncher::UnityLauncher(QObject* parent)
	: super(new UnityLauncherPrivate(this), parent)
{ }

/**
 * Is this TaskbarButtonManager usable?
 * @return True if usable; false if not.
 */
bool UnityLauncher::IsUsable(void)
{
	QDBusConnection connection = QDBusConnection::sessionBus();
	QStringList services = connection.interface()->registeredServiceNames().value();
	return services.contains(QStringLiteral("com.canonical.Unity"));
}

/**
 * Set the window this TaskbarButtonManager should manage.
 * This must be a top-level window in order to work properly.
 *
 * Subclasses should reimplement this function if special
 * initialization is required to set up the taskbar button.
 *
 * TODO: Make a separate protected function that setWindow() calls?
 *
 * @param window Window
 */
void UnityLauncher::setWindow(QWidget *window)
{
	// FIXME: Ignored; launcher entries are per-application.
	// Maybe use this to set the desktop entry filename?
	Q_UNUSED(window)
}

/**
 * Update the taskbar button.
 */
void UnityLauncher::update(void)
{
	// Update the Unity Launcher Entry.
	// TODO: Optimize DockManager to use progressBar* directly?
	// FIXME: Broken on Qt6 for some reason... (QtDBus crashes)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	Q_D(UnityLauncher);
	if (d->progressBarValue < 0) {
		// Progress bar is hidden.
		d->sendMessage("progress-visible", false);
	} else {
		// Progress bar is visible.
		const double value = (double)d->progressBarValue / (double)d->progressBarMax;
		QVariantMap map;
		map.insert(QStringLiteral("progress"), value);
		map.insert(QStringLiteral("progress-visible"), true);
		d->sendMessage(map);
	}
#endif /* QT_VERSION < QT_VERSION_CHECK(6, 0, 0) */
}
