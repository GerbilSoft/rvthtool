/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * UnityLauncher.cpp: Unity Launcher implementation.                       *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_UNITYLAUNCHER_HPP__
#define __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_UNITYLAUNCHER_HPP__

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
		 * @param window Window.
		 */
		void setWindow(QWidget *window) final;

	protected:
		/**
		 * Update the taskbar button.
		 */
		void update(void) final;
};

#endif /* __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_UNITYLAUNCHER_HPP__ */
