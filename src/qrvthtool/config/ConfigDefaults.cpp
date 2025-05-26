/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * ConfigDefaults.hpp: Configuration defaults.                             *
 *                                                                         *
 * Copyright (c) 2008-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "ConfigDefaults.hpp"

// C++ STL classes
#include <array>
#include <map>
using std::array;
using std::map;
using std::vector;

class ConfigDefaultsPrivate {
public:
	ConfigDefaultsPrivate();

public:
	/**
	 * Single instance of ConfigDefaults
	 */
	static ConfigDefaults ms_Instance;

public:
	// Default settings
	static const array<ConfigDefaults::DefaultSetting, 2> defaultSettings;

	// Internal settings map
	map<QString, const ConfigDefaults::DefaultSetting*> defaultSettingsMap;
};

// Single instance of ConfigDefaults
ConfigDefaults ConfigDefaultsPrivate::ms_Instance;

/**
 * Default configuration filename
 */
const char *const ConfigDefaults::DefaultConfigFilename = "qrvthtool.conf";

// Default settings
const array<ConfigDefaults::DefaultSetting, 2> ConfigDefaultsPrivate::defaultSettings = {{
	/** General settings **/
	{"lastPath",		"", 0, 0,	ConfigDefaults::DefaultSetting::ValidationType::None, 0, 0},
	{"language",		"", 0, 0,	ConfigDefaults::DefaultSetting::ValidationType::None, 0, 0},
}};

ConfigDefaultsPrivate::ConfigDefaultsPrivate()
{
	// Populate the default settings hash.
	for (const auto &def : defaultSettings) {
		const QString key = QLatin1String(def.key);
		defaultSettingsMap.emplace(key, &def);
	}
}

ConfigDefaults::ConfigDefaults()
	: d_ptr(new ConfigDefaultsPrivate)
{ }

/**
 * Return a single instance of ConfigDefaults.
 * @return Single instance of ConfigDefaults
 */
ConfigDefaults *ConfigDefaults::Instance(void)
{
	return &ConfigDefaultsPrivate::ms_Instance;
}

/**
 * Get a vector of all DefaultSetting names.
 * @return Vector of all DefaultSetting names.
 */
vector<QString> ConfigDefaults::getAllSettingNames(void) const
{
	Q_D(const ConfigDefaults);

	vector<QString> vret;
	vret.reserve(d->defaultSettingsMap.size());

	for (const auto &p : d->defaultSettingsMap) {
		vret.push_back(p.first);
	}

	return vret;
}

/**
 * Get a DefaultSetting struct.
 * @param key Setting key
 * @return DefaultSetting struct, or nullptr if not found.
 */
const ConfigDefaults::DefaultSetting *ConfigDefaults::get(const QString &key) const
{
	Q_D(const ConfigDefaults);
	auto iter = d->defaultSettingsMap.find(key);
	return (iter != d->defaultSettingsMap.end())
		? iter->second
		: nullptr;
}
