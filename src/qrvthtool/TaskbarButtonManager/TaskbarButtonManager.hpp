/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * TaskbarButtonManager.hpp: Taskbar button manager base class.            *
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

#ifndef __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_HPP__
#define __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_HPP__

#include <QtCore/QObject>
class QWidget;

class TaskbarButtonManagerPrivate;
class TaskbarButtonManager : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QWidget* window READ window WRITE setWindow)
	Q_PROPERTY(int progressBarValue READ progressBarValue WRITE setProgressBarValue)
	Q_PROPERTY(int progressBarMax READ progressBarMax WRITE setProgressBarMax)

	protected:
		TaskbarButtonManager(TaskbarButtonManagerPrivate *d, QObject *parent = nullptr);
	public:
		virtual ~TaskbarButtonManager();

	protected:
		TaskbarButtonManagerPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(TaskbarButtonManager)
	private:
		typedef QObject super;
		Q_DISABLE_COPY(TaskbarButtonManager)

	public:
		/**
		 * Get the window this TaskbarButtonManager is managing.
		 * @return Window.
		 */
		QWidget *window(void) const;

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
		virtual void setWindow(QWidget *window);

		/**
		 * Clear the progress bar.
		 */
		void clearProgressBar(void);

		/**
		 * Get the progress bar value.
		 * @return Value.
		 */
		int progressBarValue(void) const;

		/**
		 * Set the progress bar value.
		 * @param current Value.
		 */
		void setProgressBarValue(int value);

		/**
		 * Get the progress bar's maximum value.
		 * @return Maximum value.
		 */
		int progressBarMax(void) const;

		/**
		 * Set the progress bar's maximum value.
		 * @param max Maximum value.
		 */
		void setProgressBarMax(int max);

	protected:
		/**
		 * Update the taskbar button.
		 */
		virtual void update(void) = 0;

	private slots:
		/**
		 * Window we're managing was destroyed.
		 * @param obj QObject that was destroyed.
		 */
		void windowDestroyed_slot(QObject *obj);
};

#endif /* __RVTHTOOL_QRVTHTOOL_TASKBARBUTTONMANAGER_TASKBARBUTTONMANAGER_HPP__ */
