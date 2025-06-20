/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * ConfigStore.cpp: Configuration store.                                   *
 *                                                                         *
 * Copyright (c) 2011-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "ConfigStore.hpp"
#include "git.h"

// Qt includes
#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QPointer>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <QtCore/QObject>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>

// Default settings
#include "ConfigDefaults.hpp"

// C includes (C++ namespace)
#include <cassert>

// C++ STL classes
#include <map>
#include <vector>
using std::map;
using std::vector;

class ConfigStorePrivate
{
public:
	explicit ConfigStorePrivate(ConfigStore *q);
	~ConfigStorePrivate();

private:
	ConfigStore *const q_ptr;
	Q_DECLARE_PUBLIC(ConfigStore)
private:
	Q_DISABLE_COPY(ConfigStorePrivate)

public:
	/**
	 * Initialize the configuration path.
	 */
	static void InitConfigPath(void);

	/**
	 * Validate a property.
	 * @param key Property name
	 * @param value Property value (May be edited for validation.)
	 * @return Property value (possibly adjusted) if validated; invalid QVariant on failure.
	 */
	static QVariant Validate(const QString &name, const QVariant &value);

	/**
	 * Look up the method index of a SIGNAL() or SLOT() in a QObject.
	 * @param object Qt object
	 * @param method Method name
	 * @return Method index, or negative on error.
	 */
	static int LookupQtMethod(const QObject *object, const char *method);

	/**
	 * Invoke a Qt method by method index, with one QVariant parameter.
	 * @param object Qt object
	 * @param method_idx Method index
	 * @param param QVariant parameter
	 */
	static void InvokeQtMethod(QObject *object, int method_idx, const QVariant &param);

	/** Internal variables **/

	// Configuration path
	static QString ConfigPath;

	// Current settings
	// TODO: Use const char* for the key instead of QString?
	map<QString, QVariant> settingsMap;

	/**
	 * Signal mappings
	 * Format:
	 * - Key: Property to watch
	 * - Value: List of SignalMaps
	 *   - SignalMap.object: Object to send signal to
	 *   - SignalMap.method: Method name
	 */
	struct SignalMap {
		QPointer<QObject> object;
		int method_idx;

		SignalMap(QPointer<QObject> object, int method_idx)
			: object(object)
			, method_idx(method_idx)
		{ }
	};
	map<QString, vector<SignalMap> > signalMaps;
	QMutex mtxSignalMaps;
};

/** ConfigStorePrivate **/

QString ConfigStorePrivate::ConfigPath;

ConfigStorePrivate::ConfigStorePrivate(ConfigStore* q)
	: q_ptr(q)
{
	// Determine the configuration path.
	// TODO: Use a mutex?
	if (ConfigPath.isEmpty()) {
		InitConfigPath();
	}
}

/**
 * Initialize the configuration path.
 */
void ConfigStorePrivate::InitConfigPath(void)
{
#ifdef Q_OS_WIN
	// Win32: Check if the application directory is writable.
	QFileInfo prgDir(QCoreApplication::applicationDirPath());
	if (prgDir.isWritable()) {
		// Application directory is writable.
		QString configPath = QCoreApplication::applicationDirPath();
		if (!configPath.endsWith(QChar(L'/')))
			configPath.append(QChar(L'/'));
		ConfigStorePrivate::ConfigPath = configPath;
		return;
	}
#endif /* Q_OS_WIN */

	// TODO: Fallback if the user directory isn't writable.
	QSettings settings(QSettings::IniFormat, QSettings::UserScope,
		QLatin1String("qrvthtool"), QLatin1String("qrvthtool"));

	// TODO: Figure out if QDir has a function to remove the filename portion of the pathname.
	QString configPath = settings.fileName();
	int slash_pos = configPath.lastIndexOf(QChar(L'/'));
#ifdef Q_OS_WIN
	int bslash_pos = configPath.lastIndexOf(QChar(L'\\'));
	if (bslash_pos > slash_pos) {
		slash_pos = bslash_pos;
	}
#endif /* Q_OS_WIN */
	if (slash_pos >= 0) {
		configPath.remove(slash_pos + 1, configPath.size());
	}

	// Make sure the directory exists.
	// If it doesn't exist, create it.
	QDir configDir(configPath);
	if (!configDir.exists()) {
		configDir.mkpath(configDir.absolutePath());
	}

	// Save the main configuration path.
	configPath = configDir.absolutePath();
	if (!configPath.endsWith(QChar(L'/'))) {
		configPath.append(QChar(L'/'));
	}
	ConfigStorePrivate::ConfigPath = configPath;
}

ConfigStorePrivate::~ConfigStorePrivate()
{
	// Delete all the signal map vectors.
	signalMaps.clear();
}

/**
 * Validate a property.
 * @param key Property name
 * @param value Property value (May be edited for validation.)
 * @return Property value (possibly adjusted) if validated; invalid QVariant on failure.
 */
QVariant ConfigStorePrivate::Validate(const QString &name, const QVariant &value)
{
	// Get the DefaultSetting entry for this property.
	// TODO: Lock the hash?
	const ConfigDefaults::DefaultSetting *def = ConfigDefaults::Instance()->get(name);
	if (!def) {
		return -1;
	}

	switch (def->validation) {
		case ConfigDefaults::DefaultSetting::ValidationType::None:
		default:
			// No validation required.
			return value;

		case ConfigDefaults::DefaultSetting::ValidationType::Boolean:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			if (!value.canConvert(QMetaType(QMetaType::Bool))) {
				return QVariant();
			}
#else /* QT_VERSION < QT_VERSION_CHECK(6, 0, 0) */
			if (!value.canConvert(QVariant::Bool)) {
				return QVariant();
			}
#endif
			return QVariant(value.toBool());

		case ConfigDefaults::DefaultSetting::ValidationType::Range: {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			if (!value.canConvert(QMetaType(QMetaType::Int))) {
				return QVariant();
			}
#else /* QT_VERSION < QT_VERSION_CHECK(6, 0, 0) */
			if (!value.canConvert(QVariant::Int)) {
				return QVariant();
			}
#endif
			int val = value.toString().toInt(nullptr, 0);
			if (val < def->range_min || val > def->range_max) {
				return QVariant();
			}
			return QVariant(val);
		}
	}

	// Should not get here...
	return QVariant();
}

/**
 * Look up the method index of a SIGNAL() or SLOT() in a QObject.
 * @param object Qt object
 * @param method Method name
 * @return Method index, or negative on error.
 */
int ConfigStorePrivate::LookupQtMethod(const QObject *object, const char *method)
{
	// Based on QMetaObject::invokeMethod().

	// NOTE: The first character of method indicates whether it's a signal or slot.
	// We don't need this information, so we'll use method+1.
	method++;

	int idx = object->metaObject()->indexOfMethod(method);
	if (idx < 0) {
		// Normalize the signature and try again.
		QByteArray norm = QMetaObject::normalizedSignature(method);
		idx = object->metaObject()->indexOfMethod(norm.constData());
	}

	if (idx < 0 || idx >= object->metaObject()->methodCount()) {
		// Method index not found.
		return -1;
	}

	// Method index found.
	return idx;
}

/**
 * Invoke a Qt method by method index, with one QVariant parameter.
 * @param object Qt object
 * @param method_idx Method index
 * @param param QVariant parameter
 */
void ConfigStorePrivate::InvokeQtMethod(QObject *object, int method_idx, const QVariant &param)
{
	// Based on QMetaObject::invokeMethod().
	QMetaMethod metaMethod = object->metaObject()->method(method_idx);
	metaMethod.invoke(object, Qt::AutoConnection, Q_ARG(QVariant, param));
}

/** ConfigStore **/

ConfigStore::ConfigStore(QObject *parent)
	: QObject(parent)
	, d_ptr(new ConfigStorePrivate(this))
{
	// Initialize defaults and load user settings.
	reset();
	load();
}

ConfigStore::~ConfigStore()
{
	// Save the configuration.
	// TODO: Handle non-default filenames.
	save();

	delete d_ptr;
}

/**
 * Reset all settings to defaults.
 */
void ConfigStore::reset(void)
{
	// Initialize settings with DefaultSettings.
	Q_D(ConfigStore);
	d->settingsMap.clear();

	const ConfigDefaults *const configDefaults = ConfigDefaults::Instance();
	vector<QString> settingNames = configDefaults->getAllSettingNames();
	for (const QString &p : settingNames) {
		const ConfigDefaults::DefaultSetting *const def = configDefaults->get(p);
		assert(def);
		if (!def) {
			continue;
		}

		d->settingsMap.emplace(
			QLatin1String(def->key),
			(def->value) ? QLatin1String(def->value) : QString()
		);
	}
}

/**
 * Set a property.
 * @param key Property name
 * @param value Property value
 */
void ConfigStore::set(const QString &key, const QVariant &value)
{
	Q_D(ConfigStore);

	// Get the current property value.
	auto settingsIter = d->settingsMap.find(key);

	// Get the default value.
	const ConfigDefaults *const configDefaults = ConfigDefaults::Instance();
	const ConfigDefaults::DefaultSetting *const def = configDefaults->get(key);
	if (!def) {
#ifndef NDEBUG
		// Property does not exist. Print a warning.
		// TODO: Make this an error, since it won't be saved?
		fprintf(stderr, "ConfigStore::set(): Property '%s' has no default value. FIX THIS!\n",
			key.toUtf8().constData());
#endif /* NDEBUG */
		return;
	}

	if (!(def->flags & ConfigDefaults::DefaultSetting::DEF_ALLOW_SAME_VALUE)) {
		// Check if the new value is the same as the old value.
		if (settingsIter != d->settingsMap.end()) {
			if (value == settingsIter->second) {
				return;
			}
		}
	}

	// Verify that this value passes validation.
	QVariant newValue = ConfigStorePrivate::Validate(key, value);
	if (!newValue.isValid()) {
		return;
	}

	// Set the new value.
	d->settingsMap[key] = newValue;

	// Invoke methods for registered objects.
	// NOTE: To prevent deadlocks, we'll need to build up a list of
	// objects and methods to invoke, then invoke them.
	QMutexLocker mtxLocker(&d->mtxSignalMaps);
	auto signalIter = d->signalMaps.find(key);
	if (signalIter == d->signalMaps.end()) {
		return;
	}
	vector<ConfigStorePrivate::SignalMap> &signalMapVector = signalIter->second;

	struct ToInvoke_t {
		QObject *object;
		int method_idx;
		QVariant value;

		ToInvoke_t(QObject *object, int method_idx, const QVariant &value)
			: object(object)
			, method_idx(method_idx)
			, value(value)
		{ }
	};
	std::vector<ToInvoke_t> toInvoke;

	// Process the signal map vector in reverse-order.
	// Reverse order makes it easier to remove deleted objects.
	for (int i = static_cast<int>(signalMapVector.size()) - 1; i >= 0; i--) {
		ConfigStorePrivate::SignalMap &smap = signalMapVector[i];
		if (smap.object.isNull()) {
			signalMapVector.erase(signalMapVector.begin() + i);
		} else {
			// Invoke this method.
			toInvoke.emplace_back(smap.object, smap.method_idx, value);
		}
	}
	mtxLocker.unlock();

	// Now invoke the methods.
	for (const auto &p : toInvoke) {
		ConfigStorePrivate::InvokeQtMethod(p.object, p.method_idx, p.value);
	}
}

/**
 * Get a property.
 * @param key Property name
 * @return Property value
 */
QVariant ConfigStore::get(const QString &key) const
{
	Q_D(const ConfigStore);

	// Get the current property value.
	auto settingsIter = d->settingsMap.find(key);
	if (settingsIter == d->settingsMap.end()) {
#ifndef NDEBUG
		// Property does not exist. Print a warning.
		// TODO: Make this an error, since it won't be saved?
		fprintf(stderr, "ConfigStore::get(): Property '%s' has no default value. FIX THIS!\n",
			key.toUtf8().constData());
#endif
		return {};
	}

	return settingsIter->second;
}

/**
 * Get a property.
 * Converts hexadecimal string values to unsigned int.
 * @param key Property name
 * @return Property value
 */
unsigned int ConfigStore::getUInt(const QString &key) const
{
	return get(key).toString().toUInt(nullptr, 0);
}

/**
 * Get a property.
 * Converts hexadecimal string values to signed int.
 * @param key Property name
 * @return Property value
 */
int ConfigStore::getInt(const QString &key) const
{
	return get(key).toString().toInt(nullptr, 0);
}

/**
 * Get the default configuration path.
 * @return Default configuration path
 */
QString ConfigStore::ConfigPath(void)
{
	if (ConfigStorePrivate::ConfigPath.isEmpty())
		ConfigStorePrivate::InitConfigPath();
	return ConfigStorePrivate::ConfigPath;
}

/**
 * Load the configuration file.
 * @param filename Configuration filename
 * @return 0 on success; non-zero on error.
 */
int ConfigStore::load(const QString &filename)
{
	Q_D(ConfigStore);
	QSettings qSettings(filename, QSettings::IniFormat);

	// NOTE: Only known settings will be loaded.
	d->settingsMap.clear();
	// TODO: Add function to get sizeof(DefaultSettings) from ConfigDefaults.
	//d->settingsMap.reserve(32);

	// Load known settings from the configuration file.
	const ConfigDefaults *const configDefaults = ConfigDefaults::Instance();
	vector<QString> settingNames = configDefaults->getAllSettingNames();
	for (const QString &p : settingNames) {
		const ConfigDefaults::DefaultSetting *const def = configDefaults->get(p);
		assert(def);
		if (!def) {
			continue;
		}

		QString key = QLatin1String(def->key);
		QVariant value = qSettings.value(key, QLatin1String(def->value));

		// Validate this value.
		value = ConfigStorePrivate::Validate(key, value);
		if (!value.isValid()) {
			// Validation failed. Use the default value.
			value = QVariant(QLatin1String(def->value));
		}

		d->settingsMap.emplace(std::move(key), std::move(value));
	}

	// Finished loading settings.
	// NOTE: Caller must call emitAll() for settings to take effect.
	return 0;
}

/**
 * Load the configuration file.
 * No filename specified; use the default filename
 * @return 0 on success; non-zero on error.
 */
int ConfigStore::load(void)
{
	const QString cfgFilename = ConfigStorePrivate::ConfigPath +
		QLatin1String(ConfigDefaults::DefaultConfigFilename);
	return load(cfgFilename);
}

/**
 * Save the configuration file.
 * @param filename Filename
 * @return 0 on success; non-zero on error.
 */
int ConfigStore::save(const QString &filename) const
{
	Q_D(const ConfigStore);
	QSettings qSettings(filename, QSettings::IniFormat);

	/** Application information **/

	// Stored in the "General" section.
	// TODO: Move "General" settings to another section?
	// ("General" is always moved to the top of the file.)
	qSettings.setValue(QLatin1String("_Application"), QCoreApplication::applicationName());
	qSettings.setValue(QLatin1String("_Version"), QCoreApplication::applicationVersion());

#ifdef MCRECOVER_GIT_VERSION
	qSettings.setValue(QLatin1String("_VersionVcs"),
				QLatin1String(RP_GIT_VERSION));
#ifdef MCRECOVER_GIT_DESCRIBE
	qSettings.setValue(QLatin1String("_VersionVcsExt"),
				QLatin1String(RP_GIT_DESCRIBE));
#else
	qSettings.remove(QLatin1String("_VersionVcsExt"));
#endif /* MCRECOVER_GIT_DESCRIBE */
#else
	qSettings.remove(QLatin1String("_VersionVcs"));
	qSettings.remove(QLatin1String("_VersionVcsExt"));
#endif /* MCRECOVER_GIT_DESCRIBE */

	// NOTE: Only known settings will be saved.
	
	// Save known settings to the configuration file.
	const ConfigDefaults *const configDefaults = ConfigDefaults::Instance();
	vector<QString> settingNames = configDefaults->getAllSettingNames();
	for (const QString &p : settingNames) {
		const ConfigDefaults::DefaultSetting *const def = configDefaults->get(p);
		assert(def);
		if (!def || (def->flags & ConfigDefaults::DefaultSetting::DEF_NO_SAVE)) {
			continue;
		}

		const QString key = QLatin1String(def->key);
		auto settingsIter = d->settingsMap.find(key);
		QVariant value = (settingsIter != d->settingsMap.end())
			? settingsIter->second
			: QLatin1String(def->value);

		if (def->hex_digits > 0) {
			// Convert to hexadecimal.
			unsigned int uint_val = value.toString().toUInt(nullptr, 0);
			QString str = QLatin1String("0x") +
					QString::number(uint_val, 16).toUpper().rightJustified(4, QChar(L'0'));
			value = str;
		}

		qSettings.setValue(key, value);
	}

	return 0;
}

/**
 * Save the configuration file.
 * No filename specified; use the default filename.
 * @return 0 on success; non-zero on error.
 */
int ConfigStore::save(void) const
{
	const QString cfgFilename = ConfigStorePrivate::ConfigPath +
		QLatin1String(ConfigDefaults::DefaultConfigFilename);
	return save(cfgFilename);
}

/**
 * Register an object for property change notification.
 * @param property Property to watch
 * @param object QObject to register
 * @param method Method name
 */
void ConfigStore::registerChangeNotification(const QString &property, QObject *object, const char *method)
{
	if (!object || !method || property.isEmpty()) {
		return;
	}

	// Get the vector of signal maps for this property.
	Q_D(ConfigStore);
	vector<ConfigStorePrivate::SignalMap> *signalMapVector;
	QMutexLocker mtxLocker(&d->mtxSignalMaps);
	auto iter = d->signalMaps.find(property);
	if (iter != d->signalMaps.end()) {
		signalMapVector = &iter->second;
	} else {
		// No vector found. Create one.
		auto ret = d->signalMaps.emplace(property, vector<ConfigStorePrivate::SignalMap>());
		signalMapVector = &ret.first->second;
	}

	// Look up the method index.
	int method_idx = ConfigStorePrivate::LookupQtMethod(object, method);
	if (method_idx < 0) {
		// NOTE: The first character of method indicates whether it's a signal or slot.
		// This is useless in error messages, so we'll use method+1.
		if (*method != 0) {
			method++;
		}
		fprintf(stderr, "ConfigStore::registerChangeNotification(): "
			"No such method %s::%s\n",
			object->metaObject()->className(), method);
		return;
	}

	// Add this object and slot to the signal maps vector.
	signalMapVector->emplace_back(object, method_idx);
}

/**
 * Unregister an object for property change notification.
 * @param property Property to watch
 * @param object QObject to register
 * @param method Method name
 */
void ConfigStore::unregisterChangeNotification(const QString &property, QObject *object, const char *method)
{
	if (!object) {
		return;
	}

	// Get the vector of signal maps for this property.
	Q_D(ConfigStore);
	QMutexLocker mtxLocker(&d->mtxSignalMaps);
	auto iter = d->signalMaps.find(property);
	if (iter == d->signalMaps.end()) {
		return;
	}
	vector<ConfigStorePrivate::SignalMap> &signalMapVector = iter->second;

	// Get the method index.
	int method_idx = -1;
	if (method != nullptr) {
		method_idx = ConfigStorePrivate::LookupQtMethod(object, method);
		if (method_idx < 0) {
			return;
		}
	}

	// Process the signal map vector in reverse-order.
	// Reverse order makes it easier to remove deleted objects.
	for (int i = static_cast<int>(signalMapVector.size()) - 1; i >= 0; i--) {
		const ConfigStorePrivate::SignalMap &smap = signalMapVector[i];
		if (smap.object.isNull()) {
			signalMapVector.erase(signalMapVector.begin() + i);
		} else if (smap.object == object) {
			// Found the object.
			if (method == nullptr || method_idx == smap.method_idx) {
				// Found a matching signal map.
				signalMapVector.erase(signalMapVector.begin() + i);
			}
		}
	}
}

/**
 * Notify all registered objects that configuration settings have changed.
 * Useful when starting the program.
 */
void ConfigStore::notifyAll(void)
{
	// Invoke methods for registered objects.
	// NOTE: To prevent deadlocks, we'll need to build up a list of
	// objects and methods to invoke, then invoke them.
	Q_D(ConfigStore);
	QMutexLocker mtxLocker(&d->mtxSignalMaps);

	struct ToInvoke_t {
		QObject *object;
		int method_idx;
		QVariant value;

		ToInvoke_t(QObject *object, int method_idx, const QVariant &value)
			: object(object)
			, method_idx(method_idx)
			, value(value)
		{ }
	};
	std::vector<ToInvoke_t> toInvoke;

	for (auto &kv : d->signalMaps) {
		const QString &property = kv.first;
		vector<ConfigStorePrivate::SignalMap> &signalMapVector = kv.second;
		if (signalMapVector.empty()) {
			continue;
		}

		// Get the property value.
		auto settingsIter = d->settingsMap.find(property);
		const QVariant &value  = (settingsIter != d->settingsMap.end())
			? settingsIter->second
			: QVariant();

		// Process the signal map vector in reverse-order.
		// Reverse order makes it easier to remove deleted objects.
		for (int i = static_cast<int>(signalMapVector.size()) - 1; i >= 0; i--) {
			ConfigStorePrivate::SignalMap &smap = signalMapVector[i];
			if (smap.object.isNull()) {
				signalMapVector.erase(signalMapVector.begin() + i);
			} else {
				// Invoke this method.
				toInvoke.emplace_back(smap.object, smap.method_idx, value);
			}
		}
	}
	mtxLocker.unlock();

	// Now invoke the methods.
	for (const auto &p : toInvoke) {
		ConfigStorePrivate::InvokeQtMethod(p.object, p.method_idx, p.value);
	}
}
