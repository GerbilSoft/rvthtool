/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TranslationManager.hpp: Qt translation manager.                         *
 *                                                                         *
 * Copyright (c) 2014-2018 by David Korth.                                 *
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

#ifndef __RVTHTOOL_QRVTHTOOL_TRANSLATIONMANAGER_HPP__
#define __RVTHTOOL_QRVTHTOOL_TRANSLATIONMANAGER_HPP__

// Qt includes.
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

class TranslationManagerPrivate;
class TranslationManager : public QObject
{
	Q_OBJECT

	private:
		TranslationManager(QObject *parent = nullptr);
		~TranslationManager();

	protected:
		typedef QObject super;
		TranslationManagerPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(TranslationManager)
	private:
		Q_DISABLE_COPY(TranslationManager)

	public:
		static TranslationManager *instance(void);

		/**
		 * Set the translation.
		 * @param locale Locale, e.g. "en_US". (Empty string is untranslated.)
		 */
		void setTranslation(const QString &locale);

		// TODO: Add a function to get the current translation?

		/**
		 * Enumerate available translations.
		 * NOTE: This only checks qrvthtool translations.
		 * If a Qt translation exists but qrvthtool doesn't have
		 * that translation, it won't show up.
		 * @return Map of available translations. (Key == locale, Value == description)
		 */
		QMap<QString, QString> enumerate(void) const;
};

#endif /* __RVTHTOOL_QRVTHTOOL_TRANSLATIONMANAGER_HPP__ */
