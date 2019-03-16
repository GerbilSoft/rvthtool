/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TaskbarButtonManagerFactory.hpp: TaskbarButtonManager factory class.    *
 *                                                                         *
 * Copyright (c) 2015-2019 by David Korth.                                 *
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

#ifndef __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGERFACTORY_HPP__
#define __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGERFACTORY_HPP__

// for Q_DISABLE_COPY()
#include <QtCore/qglobal.h>
class QObject;

class TaskbarButtonManager;
class TaskbarButtonManagerFactory
{
	private:
		TaskbarButtonManagerFactory();
		~TaskbarButtonManagerFactory();
	private:
		Q_DISABLE_COPY(TaskbarButtonManagerFactory)

	public:
		/**
		 * Create a TaskbarButtonManager.
		 * @param parent Parent object.
		 * @return System-specific TaskbarButtonManager, or nullptr on error.
		 */
		static TaskbarButtonManager *createManager(QObject *parent = nullptr);
};

#endif /* __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGERFACTORY_HPP__ */
