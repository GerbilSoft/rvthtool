/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TranslationManager.cpp: Qt translation manager.                         *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.qrvthtool.h"
#include "TranslationManager.hpp"
#include "config/ConfigStore.hpp"

// Qt includes
#include <QtCore/QTranslator>
#include <QtCore/QCoreApplication>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>

// C++ STL classes
#include <array>
using std::array;

/** TranslationManagerPrivate **/

class TranslationManagerPrivate
{
public:
	explicit TranslationManagerPrivate(TranslationManager *q);
	~TranslationManagerPrivate();

protected:
	TranslationManager *const q_ptr;
	Q_DECLARE_PUBLIC(TranslationManager)
private:
	Q_DISABLE_COPY(TranslationManagerPrivate)

public:
	static TranslationManager *instance;

	// QTranslators
	array<QTranslator*, 2> qtTranslator;	// 0 == qt, 1 == qtbase
	QTranslator *prgTranslator;

	// List of paths to check for translations.
	// NOTE: qtTranslator also checks QLibraryInfo::TranslationsPath.
	QVector<QString> pathList;
};

// Singleton instance.
TranslationManager *TranslationManagerPrivate::instance = nullptr;

TranslationManagerPrivate::TranslationManagerPrivate(TranslationManager *q)
	: q_ptr(q)
	, qtTranslator({new QTranslator(), new QTranslator()})
	, prgTranslator(new QTranslator())
{
	// Determine which paths to check for translations.
	pathList.clear();

#ifdef Q_OS_WIN
	// Win32: Search the program's /translations/ and main directories.
	pathList.append(QCoreApplication::applicationDirPath() + QStringLiteral("/translations"));
	pathList.append(QCoreApplication::applicationDirPath());
#else /* !Q_OS_WIN */
	// Check if the program's directory is within the user's home directory.
	bool isPrgDirInHomeDir = false;
	QDir prgDir = QDir(QCoreApplication::applicationDirPath());
	QDir homeDir = QDir::home();

	do {
		if (prgDir == homeDir) {
			isPrgDirInHomeDir = true;
			break;
		}

		prgDir.cdUp();
	} while (!prgDir.isRoot());

	if (isPrgDirInHomeDir) {
		// Program is in the user's home directory.
		// This usually means they're working on it themselves.

		// Search the program's /translations/ and main directories.
		pathList.append(QCoreApplication::applicationDirPath() + QStringLiteral("/translations"));
		pathList.append(QCoreApplication::applicationDirPath());
	}

	// Search the installed translations directory.
	pathList.append(QStringLiteral(QRVTHTOOL_TRANSLATIONS_DIRECTORY));
#endif /* Q_OS_WIN */

	// Search the user's configuration directory.
	QDir configDir(ConfigStore::ConfigPath());
	if (configDir != QDir(QCoreApplication::applicationDirPath())) {
		pathList.append(configDir.absoluteFilePath(QStringLiteral("translations")));
		pathList.append(configDir.absolutePath());
	}
}

TranslationManagerPrivate::~TranslationManagerPrivate()
{
	delete qtTranslator[0];
	delete qtTranslator[1];
	delete prgTranslator;
}

/** TranslationManager **/

TranslationManager::TranslationManager(QObject *parent)
	: super(parent)
	, d_ptr(new TranslationManagerPrivate(this))
{ }

TranslationManager::~TranslationManager()
{
	delete d_ptr;
}

TranslationManager* TranslationManager::instance(void)
{
	if (!TranslationManagerPrivate::instance)
		TranslationManagerPrivate::instance = new TranslationManager();
	return TranslationManagerPrivate::instance;
}

/**
 * Set the translation.
 * @param locale Locale, e.g. "en_US". (Empty string is untranslated.)
 */
void TranslationManager::setTranslation(const QString &locale)
{
	Q_D(TranslationManager);

	// Remove the QTranslators, if they were installed.
	qApp->removeTranslator(d->qtTranslator[0]);
	qApp->removeTranslator(d->qtTranslator[1]);
	qApp->removeTranslator(d->prgTranslator);

	// Initialize the Qt translation system.
	const array<QString, 2> qtLocales = {
		QStringLiteral("qt_") + locale,
		QStringLiteral("qtbase_") + locale,
	};
	array<bool, 2> isLoaded = {false, false};

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
	// Qt on Unix (but not Mac) is usually installed system-wide.
	// Check the Qt library path first.
	for (size_t i = 0; i < qtLocales.size(); i++) {
#  if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
		QString path = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#  else /* QT_VERSION < QT_VERSION_CHECK(6,0,0) */
		QString path = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#  endif /* QT_VERSION >= QT_VERSION_CHECK(6,0,0) */
		isLoaded[i] = d->qtTranslator[i]->load(qtLocales[i], path);
	}
#endif

	if (!isLoaded[0] || !isLoaded[1]) {
		// System-wide translations aren't installed.
		// Check other paths.
		for (const QString &path : d->pathList) {
			for (size_t i = 0; i < qtLocales.size(); i++) {
				if (isLoaded[i]) {
					continue;
				}

				isLoaded[i] = d->qtTranslator[i]->load(qtLocales[i], path);
			}
		}
	}

	// Initialize the application translator.
	QString prgLocale = QStringLiteral("rvthtool_") + locale;
	for (const QString &path : d->pathList) {
		if (d->prgTranslator->load(prgLocale, path)) {
			break;
		}
	}

	/** Translation file information **/

	//: Translation file author. Put your name here.
	QString tsAuthor = d->prgTranslator->translate("TranslationManager", "David Korth", "ts-author");
	Q_UNUSED(tsAuthor)
	//: Language this translation provides, e.g. "English (US)".
	QString tsLanguage = d->prgTranslator->translate("TranslationManager", "Default", "ts-language");
	Q_UNUSED(tsLanguage)
	//: Locale name, e.g. "en_US".
	QString tsLocale = d->prgTranslator->translate("TranslationManager", "C", "ts-locale");
	if (tsLocale.isEmpty() || tsLocale == QLatin1String("1337")) {
		tsLocale = QLatin1String("C");
	}

#ifdef __linux__
	// Setting LANG and LC_ALL may help with Qt's base translations...
	tsLocale += QLatin1String(".UTF-8");
	QByteArray tsLocale_utf8 = tsLocale.toUtf8();
	setenv("LANG", tsLocale_utf8.constData(), 1);
	setenv("LC_ALL", tsLocale_utf8.constData(), 1);
#endif /* __linux__ */

	// Add the new QTranslators.
	qApp->installTranslator(d->qtTranslator[0]);
	qApp->installTranslator(d->qtTranslator[1]);
	qApp->installTranslator(d->prgTranslator);
}

/**
 * Enumerate available translations.
 * NOTE: This only checks rvthtool translations.
 * If a Qt translation exists but rvthtool doesn't have
 * that translation, it won't show up.
 * @return Map of available translations. (Key == locale, Value == description)
 */
QMap<QString, QString> TranslationManager::enumerate(void) const
{
	// Name filters.
	// Remember that compiled translations have the
	// extension *.qm, not *.ts.
	const QStringList nameFilters = {
		QLatin1String("*.qm"),
		QLatin1String("*.qM"),
		QLatin1String("*.Qm"),
		QLatin1String("*.QM"),
	};

	// Search the paths for TS files.
	static const QDir::Filters filters = (QDir::Files | QDir::Readable);

	Q_D(const TranslationManager);
	QMap<QString, QString> tsMap;
	QTranslator tmpTs;
	for (const QString &path : d->pathList) {
		QDir dir(path);
		QFileInfoList files = dir.entryInfoList(nameFilters, filters);
		for (const QFileInfo &file : files) {
			// Get the locale information.
			// TODO: Also get the author information?
			if (tmpTs.load(file.absoluteFilePath())) {
				QString locale = tmpTs.translate("TranslationManager", "C", "ts-locale");
				if (tsMap.contains(locale)) {
					// FIXME: Duplicate translation?
					continue;
				}

				// Add the translation to the map.
				QString language = tmpTs.translate("TranslationManager", "Default", "ts-language");
				tsMap.insert(locale, language);
			}
		}
	}

	// Translations enumerated.
	return tsMap;
}
