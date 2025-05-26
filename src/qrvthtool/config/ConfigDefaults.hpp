/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * ConfigDefaults.hpp: Configuration defaults.                             *
 *                                                                         *
 * Copyright (c) 2011-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

// C includes
#include <stdint.h>

// C++ STL classes
#include <vector>

// Qt includes
#include <QtCore/QString>

class ConfigDefaultsPrivate;
class ConfigDefaults
{
public:
	friend class ConfigDefaultsPrivate;
	static ConfigDefaults *Instance(void);

private:
	ConfigDefaults();
	~ConfigDefaults() = default;
	Q_DISABLE_COPY(ConfigDefaults)

private:
	ConfigDefaultsPrivate *const d_ptr;
	Q_DECLARE_PRIVATE(ConfigDefaults);

public:
	// Default configuration filename
	static const char *const DefaultConfigFilename;

	// Default settings struct
	struct DefaultSetting {
		const char *key;
		const char *value;
		uint8_t hex_digits;	// If non-zero, saves as hexadecimal with this many digits.

		// Flags
		enum DefaultSettingFlags {
			/**
			 * If true, allow a setting to be reset to the current value,
			 * which will result in property change signals being emitted
			 * regardless of whether or not the setting has actually changed.
			 *
			 * This is useful for e.g. "Savestate/saveSlot", since the user
			 * should be able to press the key corresponding to the current
			 * save slot in order to see the preview image for that savestate.
			 */
			DEF_ALLOW_SAME_VALUE	= (1 << 0),

			/**
			 * If true, don't save this setting in the configuration file.
			 * This will be used for some "hidden" settings that cannot
			 * be changed in the GUI, e.g. the option to show the
			 * VScroll Bug and Zero-Length DMA bug checkboxes in
			 * GeneralConfigWindow.
			 */
			DEF_NO_SAVE		= (1 << 1),
		};

		// Flags. Uses DefaultSettingFlags values.
		uint8_t flags;

		// Parameter validation
		enum class ValidationType : uint8_t {
			None,		// No validation
			Boolean,	// Boolean (normalize to true/false)
			Color,		// QColor
			Range,		// Integer range

			Max
		};
		ValidationType validation;

		int range_min;
		int range_max;
	};

	/**
	 * Get a vector of all DefaultSetting names.
	 * @return Vector of all DefaultSetting names.
	 */
	std::vector<QString> getAllSettingNames(void) const;

	/**
	 * Get a DefaultSetting struct.
	 * @param key Setting key
	 * @return DefaultSetting struct, or nullptr if not found.
	 */
	const DefaultSetting *get(const QString &key) const;
};
