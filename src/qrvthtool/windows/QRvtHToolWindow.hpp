/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.hpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

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
	 * @param filename Filename
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
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
	typedef qintptr native_event_result_t;
#else /* QT_VERSION < QT_VERSION_CHECK(6,0,0) */
	typedef long native_event_result_t;
#endif /* QT_VERSION >= QT_VERSION_CHECK(6,0,0) */

	/**
	 * Windows native event handler.
	 * Used for TaskbarButtonManager.
	 * @param eventType
	 * @param message
	 * @param result
	 * @return
	 */
	bool nativeEvent(const QByteArray &eventType, void *message, native_event_result_t *result) final;
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
	 * @param text Status bar text
	 * @param progress_value Progress bar value (If -1, ignore this.)
	 * @param progress_max Progress bar maximum (If -1, ignore this.)
	 */
	void workerObject_updateStatus(const QString &text, int progress_value, int progress_max);

	/**
	 * Process is finished.
	 * @param text Status bar text
	 * @param err Error code (0 on success)
	 */
	void workerObject_finished(const QString &text, int err);

	/**
	 * Cancel button was pressed.
	 */
	void btnCancel_clicked(void);

protected slots:
	/** Configuration slots **/

	/**
	 * "Mask Device Serial Numbers" option was changed by the user.
	 * @param mask If true, mask device serial numbers.
	 */
	void on_actionMaskDeviceSerialNumbers_triggered(bool mask);

	/**
	 * "Mask Device Serial Numbers" option was changed by the configuration.
	 * @param mask If true, mask device serial numbers.
	 */
	void maskDeviceSerialNumbers_cfg_slot(const QVariant &mask);

	/**
	 * UI language was changed by the user.
	 * @param locale Locale tag, e.g. "en_US".
	 */
	void on_menuLanguage_languageChanged(const QString &locale);

	/**
	 * UI language was changed by the configuration.
	 * @param locale Locale tag, e.g. "en_US".
	 */
	void setTranslation_cfg_slot(const QVariant &locale);
};
