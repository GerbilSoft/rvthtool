/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * MessageSound.hpp: Message sound effects abstraction class.              *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "MessageSound.hpp"

#include "config.qrvthtool.h"

#if defined(_WIN32)
# include <windows.h>
#elif defined(HAVE_KF5WIDGETSADDONS)
# include <kmessageboxnotifyinterface.h>
# include <QtCore/QPluginLoader>
# include <QtCore/QVariant>
# include <pthread.h>
#endif

/**
 * Play a message sound effect.
 * @param notificationType Notification type.
 * @param message Message for logging. (not supported on all systems)
 * @param parent Parent window. (not supported on all systems)
 */
void MessageSound::play(QMessageBox::Icon notificationType, const QString &message, QWidget *parent)
{
#ifdef _WIN32
	// Windows: Use MessageBeep().
	Q_UNUSED(message)
	Q_UNUSED(parent)
	UINT uType;
	switch (notificationType) {
		case QMessageBox::Information:
			uType = MB_ICONINFORMATION;
			break;
		case QMessageBox::Warning:
			uType = MB_ICONWARNING;
			break;
		case QMessageBox::Critical:
			uType = MB_ICONERROR;
			break;
		case QMessageBox::Question:
			uType = MB_ICONQUESTION;
			break;
		default:
			uType = 0;
			break;
	}

	if (uType != 0) {
		MessageBeep(uType);
	}
#else /* !_WIN32 */
	// Linux: Try a few different methods.
	// TODO: Check XDG_DESKTOP_SESSION.
	// TODO: More methods.

#ifdef HAVE_KF5WIDGETSADDONS
	// If KDE is available, try FrameworkIntegrationPlugin.
	// NOTE: Not unloading the plugin here, so QPluginLoader's lookup
	// should be fast, and we won't have to maintain the QPluginLoader
	// instance. (Keeping the lib.instance() pointer in memory is likely
	// to be undefined behavior.)
	QPluginLoader lib(QStringLiteral("kf5/FrameworkIntegrationPlugin"));
	QObject *rootObj = lib.instance();
	if (rootObj) {
		KMessageBoxNotifyInterface *iface =
			rootObj->property(KMESSAGEBOXNOTIFY_PROPERTY)
				.value<KMessageBoxNotifyInterface*>();
		if (iface) {
			iface->sendNotification(notificationType, message, parent);
		}
	}
#endif /* HAVE_KF5WIDGETSADDONS */

#endif /* _WIN32 */
}
