/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * QRvtHToolWindow.hpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ***************************************************************************/

#ifndef __RVTHTOOL_QRVTHTOOL_WINDOWS_QRVTHTOOLWINDOW_HPP__
#define __RVTHTOOL_QRVTHTOOL_WINDOWS_QRVTHTOOLWINDOW_HPP__

#include <QMainWindow>

class QRvtHToolWindowPrivate;
class QRvtHToolWindow : public QMainWindow
{
	Q_OBJECT
	typedef QMainWindow super;
	
	public:
		explicit QRvtHToolWindow(QWidget *parent = 0);
		virtual ~QRvtHToolWindow();

	protected:
		QRvtHToolWindowPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(QRvtHToolWindow)
	private:
		Q_DISABLE_COPY(QRvtHToolWindow)

	public:
		/**
		 * Open an RVT-H Reader disk image.
		 * @param filename Filename.
		 */
		void openRvtH(const QString &filename);

		/**
		 * Close the currently-opened RVT-H Reader disk image.
		 */
		void closeRvtH(void);

	protected:
		// State change event. (Used for switching the UI language at runtime.)
		void changeEvent(QEvent *event) final;

	protected slots:
		// Actions.
		void on_actionOpen_triggered(void);
		void on_actionClose_triggered(void);
		void on_actionExit_triggered(void);
		void on_actionAbout_triggered(void);

		// RvtHModel slots.
		void rvthModel_layoutChanged(void);
		void rvthModel_rowsInserted(void);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_QRVTHTOOLWINDOW_HPP__ */
