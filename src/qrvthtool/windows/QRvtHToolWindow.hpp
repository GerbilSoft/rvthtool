/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.hpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
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
		 * @param filename Filename (using NATIVE separators)
		 * @param isDevice True if opened using SelectDeviceDialog
		 */
		void openRvtH(const QString &filename, bool isDevice);

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

#ifdef Q_OS_WIN
		// Windows message handler. Used for TaskbarButtonManager.
		bool nativeEvent(const QByteArray &eventType, void *message, long *result) final;
#endif /* Q_OS_WIN */

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
		void on_actionDelete_triggered(void);
		void on_actionUndelete_triggered(void);

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
