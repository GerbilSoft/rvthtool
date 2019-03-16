/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.hpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

class QItemSelection;
#include <QMainWindow>

class QRvtHToolWindowPrivate;
class QRvtHToolWindow : public QMainWindow
{
	Q_OBJECT
	typedef QMainWindow super;
	
	public:
		explicit QRvtHToolWindow(QWidget *parent = nullptr);
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

		// Window show event.
		void showEvent(QShowEvent *event) final;

		// Window close event.
		void closeEvent(QCloseEvent *event) final;

	protected slots:
		// UI busy functions
		void markUiBusy(void);
		void markUiNotBusy(void);

	protected slots:
		/** UI widget slots **/

		// Actions
		void on_actionOpenDiskImage_triggered(void);
		void on_actionOpenDevice_triggered(void);
		void on_actionClose_triggered(void);
		void on_actionExit_triggered(void);
		void on_actionAbout_triggered(void);

		// RVT-H disk image actions
		void on_actionExtract_triggered(void);
		void on_actionImport_triggered(void);

		// RvtHModel slots
		void rvthModel_layoutChanged(void);
		void rvthModel_rowsInserted(void);

		// lstBankList slots
		void lstBankList_selectionModel_selectionChanged(
			const QItemSelection& selected, const QItemSelection& deselected);

	protected slots:
		/** Worker object slots **/

		/**
		 * Update the status bar.
		 * @param text Status bar text.
		 * @param progress_value Progress bar value. (If -1, ignore this.)
		 * @param progress_max Progress bar maximum. (If -1, ignore this.)
		 */
		void workerObject_updateStatus(const QString &text, int progress_value, int progress_max);

		/**
		 * Process is finished.
		 * @param text Status bar text.
		 * @param err Error code. (0 on success)
		 */
		void workerObject_finished(const QString &text, int err);

		/**
		 * Cancel button was pressed.
		 */
		void btnCancel_clicked(void);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_QRVTHTOOLWINDOW_HPP__ */
