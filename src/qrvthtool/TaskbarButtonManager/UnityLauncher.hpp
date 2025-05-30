/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * UnityLauncher.cpp: Unity Launcher implementation.                       *
 *                                                                         *
 * Copyright (c) 2013-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "TaskbarButtonManager.hpp"

class UnityLauncherPrivate;
class UnityLauncher : public TaskbarButtonManager
{
Q_OBJECT

public:
	explicit UnityLauncher(QObject *parent = nullptr);

private:
	typedef TaskbarButtonManager super;
	Q_DECLARE_PRIVATE(UnityLauncher)
	Q_DISABLE_COPY(UnityLauncher)

public:
	/**
	 * Is this TaskbarButtonManager usable?
	 * @return True if usable; false if not.
	 */
	static bool IsUsable(void);

public:
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
	void setWindow(QWidget *window) final;

protected:
	/**
	 * Update the taskbar button.
	 */
	void update(void) final;
};
