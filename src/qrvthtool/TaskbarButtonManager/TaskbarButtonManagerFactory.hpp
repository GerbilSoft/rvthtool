/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TaskbarButtonManagerFactory.hpp: TaskbarButtonManager factory class.    *
 *                                                                         *
 * Copyright (c) 2015-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
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
