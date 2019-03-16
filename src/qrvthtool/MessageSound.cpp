/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * MessageSound.hpp: Message sound effects abstraction class.              *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#if !defined(_WIN32) && defined(HAVE_KF5WIDGETSADDONS)
static pthread_once_t kf5notify_once = PTHREAD_ONCE_INIT;
static KMessageBoxNotifyInterface *s_notifyInterface = nullptr;

/**
 * Initialize the KMessageBox notify interface.
 */
static void init_kf5notify(void)
{
	// Reference: https://github.com/KDE/kwidgetsaddons/blob/master/src/kmessagebox_p.cpp
	QPluginLoader lib(QStringLiteral("kf5/FrameworkIntegrationPlugin"));
	QObject *rootObj = lib.instance();
	if (rootObj) {
		s_notifyInterface = rootObj->property(KMESSAGEBOXNOTIFY_PROPERTY)
			.value<KMessageBoxNotifyInterface*>();
	}
}
#endif /* !defined(_WIN32) && defined(HAVE_KF5WIDGETSADDONS) */

/**
 * Play a message sound effect.
 * @param icon MessageBox icon associated with the sound effect.
 */
void MessageSound::play(QMessageBox::Icon icon)
{
#ifdef _WIN32
	// Windows: Use MessageBeep().
	UINT uType;
	switch (icon) {
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

	// If KDE is available, try FrameworkIntegrationPlugin.
	pthread_once(&kf5notify_once, init_kf5notify);
	if (s_notifyInterface) {
		// TODO: Send the message and parent too?
		s_notifyInterface->sendNotification(icon, QString(), nullptr);
	}
#endif /* _WIN32 */
}
