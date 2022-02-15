/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TaskbarButtonManager_p.hpp: Taskbar button manager base class. (PRIVATE)*
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_P_HPP__
#define __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_P_HPP__

#include "TaskbarButtonManager.hpp"

class TaskbarButtonManagerPrivate
{
	public:
		explicit TaskbarButtonManagerPrivate(TaskbarButtonManager *const q);
		virtual ~TaskbarButtonManagerPrivate();

	protected:
		TaskbarButtonManager *const q_ptr;
		Q_DECLARE_PUBLIC(TaskbarButtonManager)
	private:
		Q_DISABLE_COPY(TaskbarButtonManagerPrivate)

	public:
		// Window.
		QWidget *window;

		// Status elements.
		int progressBarValue;	// Current progress. (-1 for no bar)
		int progressBarMax;	// Maximum progress.
};

#endif /* __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_P_HPP__ */
