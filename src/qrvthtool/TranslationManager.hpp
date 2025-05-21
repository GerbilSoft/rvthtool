/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TranslationManager.hpp: Qt translation manager.                         *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

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
