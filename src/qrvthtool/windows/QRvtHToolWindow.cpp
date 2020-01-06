/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.cpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.librvth.h"
#include "QRvtHToolWindow.hpp"

// librvth
#include "librvth/rvth.hpp"
#include "librvth/nhcd_structs.h"
#include "librvth/rvth_error.h"
#include "librvth/query.h"

#include "RvtHModel.hpp"
#include "RvtHSortFilterProxyModel.hpp"
#include "MessageSound.hpp"

#include "widgets/MessageWidgetStack.hpp"
#include "windows/SelectDeviceDialog.hpp"

// C includes. (C++ namespace)
#include <cassert>

// Qt includes.
#include <QtCore/QThread>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolButton>

// RVL_CryptoType_e
#include "libwiicrypto/sig_tools.h"

// Worker object for the worker thread.
#include "WorkerObject.hpp"

// Taskbar Button Manager.
#include "TaskbarButtonManager/TaskbarButtonManager.hpp"
#include "TaskbarButtonManager/TaskbarButtonManagerFactory.hpp"
#ifdef Q_OS_WIN
# include <windows.h>
#endif /* Q_OS_WIN */

// Other windows.
#include "AboutDialog.hpp"

/** QRvtHToolWindowPrivate **/

#include "ui_QRvtHToolWindow.h"
class QRvtHToolWindowPrivate
{
	public:
		explicit QRvtHToolWindowPrivate(QRvtHToolWindow *q);
		~QRvtHToolWindowPrivate();

	protected:
		QRvtHToolWindow *const q_ptr;
		Q_DECLARE_PUBLIC(QRvtHToolWindow)
	private:
		Q_DISABLE_COPY(QRvtHToolWindowPrivate)

	public:
		Ui::QRvtHToolWindow ui;

		// RVT-H Reader disk image
		RvtH *rvth;
		RvtHModel *model;
		RvtHSortFilterProxyModel *proxyModel;

		// Filename.
		QString filename;

		// TODO: Config class like mcrecover?
		QString lastPath;

		// Initialized columns?
		bool cols_init;

		// Last icon ID.
		RvtHModel::IconID lastIconID;

		/**
		 * Update the RVT-H Reader disk image's QTreeView.
		 */
		void updateLstBankList(void);

		/**
		 * Update the "enabled" status of the QActions.
		 */
		void updateActionEnableStatus(void);

		/**
		 * Update the window title.
		 */
		void updateWindowTitle(void);

		/**
		 * Get the display filename for the given filename.
		 *
		 * This removes the directories and returns only the filename,
		 * except in the case of device files.
		 *
		 * @param filename Full filename.
		 * @return Display filename.
		 */
		static QString getDisplayFilename(const QString &filename);

	public:
		/**
		 * Initialize the toolbar.
		 */
		void initToolbar(void);

		/**
		 * Retranslate the toolbar.
		 */
		void retranslateToolbar(void);

		// Recryption Key widgets.
		QLabel *lblRecryptionKey;
		QComboBox *cboRecryptionKey;

	public:
		// Status bar widgets.
		QLabel *lblMessage;
		QToolButton *btnCancel;
		QProgressBar *progressBar;

		// Worker thread.
		QThread *workerThread;
		WorkerObject *workerObject;

		// UI busy counter
		int uiBusyCounter;

		// Taskbar Button Manager.
		TaskbarButtonManager *taskbarButtonManager;

	public:
		// Update status.
		bool updateStatus_didInitialUpdate;
		int updateStatus_bank;
};

QRvtHToolWindowPrivate::QRvtHToolWindowPrivate(QRvtHToolWindow *q)
	: q_ptr(q)
	, rvth(nullptr)
	, model(new RvtHModel(q))
	, proxyModel(new RvtHSortFilterProxyModel(q))
	, cols_init(false)
	, lastIconID(RvtHModel::ICON_MAX)
	, lblRecryptionKey(nullptr)
	, cboRecryptionKey(nullptr)
	, lblMessage(nullptr)
	, progressBar(nullptr)
	, workerThread(nullptr)
	, workerObject(nullptr)
	, uiBusyCounter(0)
	, taskbarButtonManager(nullptr)
	, updateStatus_didInitialUpdate(false)
	, updateStatus_bank(0)
{
	// Connect the RvtHModel slots.
	QObject::connect(model, &RvtHModel::layoutChanged,
			 q, &QRvtHToolWindow::rvthModel_layoutChanged);
	QObject::connect(model, &RvtHModel::rowsInserted,
			 q, &QRvtHToolWindow::rvthModel_rowsInserted);
}

QRvtHToolWindowPrivate::~QRvtHToolWindowPrivate()
{
	if (workerThread) {
		// Worker thread is still running...
		// TODO: Cancel it instead of simply quitting.
		workerThread->quit();
		do {
			workerThread->wait(250);
		} while (workerThread->isRunning());
	}
	delete workerObject;

	// NOTE: Delete the RvtHModel first to prevent issues later.
	delete model;
	if (rvth) {
		delete rvth;
	}
}

/**
 * Update the RVT-H Reader disk image's QTreeView.
 */
void QRvtHToolWindowPrivate::updateLstBankList(void)
{
	if (!rvth) {
		// Set the group box's title.
		ui.grpBankList->setTitle(QRvtHToolWindow::tr("No RVT-H Reader disk image loaded."));
	} else {
		// Show the filename and device type.
		const QString displayFilename = getDisplayFilename(filename);
		QString title = displayFilename;
		QString imageType;
		switch (rvth->imageType()) {
			// HDDs (multiple banks)
			case RVTH_ImageType_HDD_Reader: {
				// TODO: Option to hide the serial number?
				// TODO: Handle pErr.
				imageType = QRvtHToolWindow::tr("RVT-H Reader");

#ifdef HAVE_QUERY
				QString qs_full_serial;
# ifdef _WIN32
				wchar_t *const s_full_serial = rvth_get_device_serial_number(
					reinterpret_cast<const wchar_t*>(filename.utf16()), nullptr);
				if (s_full_serial) {
					qs_full_serial = QString::fromUtf16(
						reinterpret_cast<const char16_t*>(s_full_serial));
					free(s_full_serial);
				}
# else /* !_WIN32 */
				char *const s_full_serial = rvth_get_device_serial_number(
					filename.toUtf8().constData(), nullptr);
				if (s_full_serial) {
					qs_full_serial = QString::fromUtf8(s_full_serial);
					free(s_full_serial);
				}
# endif /* _WIN32 */

				if (!qs_full_serial.isEmpty()) {
					imageType += QChar(L' ');
					imageType += qs_full_serial;
				}
#endif /* HAVE_QUERY */

				break;
			}
			case RVTH_ImageType_HDD_Image:
				imageType = QRvtHToolWindow::tr("RVT-H Reader Disk Image");
				break;

			// GCMs (single banks)
			// TODO: CISO/WBFS?
			case RVTH_ImageType_GCM:
				imageType = QRvtHToolWindow::tr("Disc Image");
				break;
			case RVTH_ImageType_GCM_SDK:
				imageType = QRvtHToolWindow::tr("SDK Disc Image");
				break;

			default:
				break;
		}

		if (!imageType.isEmpty()) {
			ui.grpBankList->setTitle(
				QRvtHToolWindow::tr("%1 [%2]")
				.arg(displayFilename).arg(imageType));
		} else {
			ui.grpBankList->setTitle(displayFilename);
		}
	}

	// Show the QTreeView headers if an RVT-H Reader disk image is loaded.
	ui.lstBankList->setHeaderHidden(!rvth);

	// Resize the columns to fit the contents.
	int num_sections = model->columnCount();
	for (int i = 0; i < num_sections; i++)
		ui.lstBankList->resizeColumnToContents(i);
	ui.lstBankList->resizeColumnToContents(num_sections);
}

/**
 * Update the "enabled" status of the QActions.
 */
void QRvtHToolWindowPrivate::updateActionEnableStatus(void)
{
	if (!rvth) {
		// No RVT-H Reader image is loaded.
		ui.actionClose->setEnabled(false);
		ui.actionExtract->setEnabled(false);
		ui.actionImport->setEnabled(false);
		ui.actionDelete->setEnabled(false);
		ui.actionUndelete->setEnabled(false);
	} else {
		// RVT-H Reader image is loaded.
		// TODO: Disable open, scan, and save (all) if we're scanning.
		ui.actionClose->setEnabled(true);

		// If a bank is selected, enable the actions.
		const RvtH_BankEntry *const entry = ui.bevBankEntryView->bankEntry();
		if (entry) {
			// Enable Extract if the bank is *not* empty.
			ui.actionExtract->setEnabled(entry->type != RVTH_BankType_Empty);

			// If this is an actual RVT-H Reader, we can
			// enable the writing functions.
			if (rvth->imageType() == RVTH_ImageType_HDD_Reader) {
				if (entry->type == RVTH_BankType_Empty) {
					// Bank is empty.
					// Enable Import; disable Delete and Undelete.
					ui.actionImport->setEnabled(true);
					ui.actionDelete->setEnabled(false);
					ui.actionUndelete->setEnabled(false);
				} else {
					// Bank is not empty.
					// Enable Import and Undelete if the bank is deleted.
					// Enable Delete if the bank is not deleted.
					ui.actionImport->setEnabled(entry->is_deleted);
					ui.actionUndelete->setEnabled(entry->is_deleted);
					ui.actionDelete->setEnabled(!entry->is_deleted);
				}
			} else {
				// Not an RVT-H Reader. Disable all writing functions.
				ui.actionImport->setEnabled(false);
				ui.actionDelete->setEnabled(false);
				ui.actionUndelete->setEnabled(false);
			}
		} else {
			// No entry. Disable everything.
			ui.actionExtract->setEnabled(false);
			ui.actionImport->setEnabled(false);
			ui.actionDelete->setEnabled(false);
			ui.actionUndelete->setEnabled(false);
		}
	}
}

/**
 * Update the window title.
 */
void QRvtHToolWindowPrivate::updateWindowTitle(void)
{
	// NOTE: Since we're setting QGuiApplication::applicationDisplayName,
	// the program name is automatically appended to the title on
	// Windows and Unix/Linux. Hence, we only have to set the window
	// title to the loaded filename.
	// TODO: Mac OS X handling.
	Q_Q(QRvtHToolWindow);
	RvtHModel::IconID iconID;
	if (rvth) {
		// Set the file path. Qt automaticlaly takes the filename
		// portion and prepends it to the application title.
		q->setWindowFilePath(filename);

		// If it's an RVT-H HDD image, use the RVT-H icon.
		// Otherwise, get the icon for the first bank.
		if (rvth->isHDD()) {
			iconID = RvtHModel::ICON_RVTH;
		} else {
			// Get the icon for the first bank.
			iconID = model->iconIDForBank1();
			if (iconID < RvtHModel::ICON_GCN || iconID >= RvtHModel::ICON_MAX) {
				// Invalid icon ID. Default to RVT-H.
				iconID = RvtHModel::ICON_RVTH;
			}
		}
	} else {
		// No file is loaded.
		q->setWindowFilePath(QString());

		// Use the RVT-H icon as the default.
		iconID = RvtHModel::ICON_RVTH;
	}

#ifdef Q_OS_MAC
	// If there's no image loaded, remove the window icon.
	// This is a "proxy icon" on Mac OS X.
	// TODO: Associate with the image file if the file is loaded?
	if (!rvth) {
		q->setWindowIcon(QIcon());
		return;
	} else if (q->windowIcon().isNull()) {
		// Force an icon update.
		lastIconID = RvtHModel::ICON_MAX;
	}
#endif /* Q_OS_MAC */

	if (iconID != lastIconID) {
		q->setWindowIcon(model->getIcon(iconID));
		lastIconID = iconID;
	}
}

/**
 * Get the display filename for the given filename.
 *
 * This removes the directories and returns only the filename,
 * except in the case of device files.
 *
 * @param filename Full filename.
 * @return Display filename.
 */
QString QRvtHToolWindowPrivate::getDisplayFilename(const QString &filename)
{
	QString rfn = filename;
	bool removeDir = true;

#ifdef _WIN32
	// Does the path start with "\\\\.\\PhysicalDriveN"?
	// FIXME: How does Qt's native slashes interact with this?
	if (rfn.startsWith(QLatin1String("\\\\.\\PhysicalDrive")) ||
	    rfn.startsWith(QLatin1String("//./PhysicalDrive")))
	{
		// Physical drive.
		// TODO: Make sure it's all backslashes.
		removeDir = false;
	}
#else /* !_WIN32 */
	// Does the path start with "/dev/"?
	if (rfn.startsWith(QLatin1String("/dev/"))) {
		// Physical drive.
		removeDir = false;
	}
#endif

	if (removeDir) {
		int lastSlash = rfn.lastIndexOf(QChar(L'/'));
		if (lastSlash >= 0) {
			rfn.remove(0, lastSlash + 1);
		}
	}

	return rfn;
}

/**
 * Initialize the toolbar.
 */
void QRvtHToolWindowPrivate::initToolbar(void)
{
	Q_Q(QRvtHToolWindow);

	// Disable per-bank actions by default.
	ui.actionExtract->setEnabled(false);
	ui.actionImport->setEnabled(false);
	ui.actionDelete->setEnabled(false);
	ui.actionUndelete->setEnabled(false);

	// Recryption key.
	ui.toolBar->insertSeparator(ui.actionAbout);
	lblRecryptionKey = new QLabel(QRvtHToolWindow::tr("Recryption Key:"), q);
	lblRecryptionKey->setObjectName(QLatin1String("lblRecryptionKey"));
	lblRecryptionKey->setToolTip(QRvtHToolWindow::tr(
		"Set the encryption key to use when extracting disc images.\n"
		"Default is None, which retains the original key."));
	lblRecryptionKey->setContentsMargins(4, 0, 4, 0);
	ui.toolBar->insertWidget(ui.actionAbout, lblRecryptionKey);

	cboRecryptionKey = new QComboBox(q);
	cboRecryptionKey->addItem(QString(), -1);			// None
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Retail);	// Retail (fakesigned)
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Korean);	// Korean (fakesigned)
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Debug);	// Debug (fakesigned)
	cboRecryptionKey->setCurrentIndex(0);
	ui.toolBar->insertWidget(ui.actionAbout, cboRecryptionKey);

	// Make sure the "About" button is right-aligned.
	QWidget *spacer = new QWidget(q);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	ui.toolBar->insertWidget(ui.actionAbout, spacer);

	// Retranslate the toolbar.
	retranslateToolbar();
}

/**
 * Retranslate the toolbar.
 */
void QRvtHToolWindowPrivate::retranslateToolbar(void)
{
	lblRecryptionKey->setText(QRvtHToolWindow::tr("Recryption Key:"));
	lblRecryptionKey->setToolTip(QRvtHToolWindow::tr(
		"Set the encryption key to use when extracting disc images.\n"
		"Default is None, which retains the original key."));

	cboRecryptionKey->setItemText(0, QRvtHToolWindow::tr("None"));
	cboRecryptionKey->setItemText(1, QRvtHToolWindow::tr("Retail (fakesigned)"));
	cboRecryptionKey->setItemText(2, QRvtHToolWindow::tr("Korean (fakesigned)"));
	cboRecryptionKey->setItemText(3, QRvtHToolWindow::tr("Debug (realsigned)"));
	cboRecryptionKey->setCurrentIndex(0);
}

/** QRvtHToolWindow **/

QRvtHToolWindow::QRvtHToolWindow(QWidget *parent)
	: super(parent)
	, d_ptr(new QRvtHToolWindowPrivate(this))
{
	Q_D(QRvtHToolWindow);
	d->ui.setupUi(this);

	// Make sure the window is deleted on close.
	this->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_OS_MAC
	// Remove the window icon. (Mac "proxy icon")
	// TODO: Use the memory card file?
	this->setWindowIcon(QIcon());
#endif /* Q_OS_MAC */

#ifdef Q_OS_WIN
	// Hide the QMenuBar border on Win32.
	// FIXME: This causes the menu bar to be "truncated" when using
	// the Aero theme on Windows Vista and 7.
#if 0
	this->Ui_QRvtHToolWindow::menuBar->setStyleSheet(
		QLatin1String("QMenuBar { border: none }"));
#endif
#endif

	// Initialize the Language Menu.
	// TODO: Load/save the language setting somewhere?
	d->ui.menuLanguage->setLanguage(QString());

	// Set up the main splitter sizes.
	// We want the card info panel to be 160px wide at startup.
	// TODO: Save positioning settings somewhere?
	static const int BankInfoPanelWidth = 256;
	QList<int> sizes;
	sizes.append(this->width() - BankInfoPanelWidth);
	sizes.append(BankInfoPanelWidth);
	d->ui.splitterMain->setSizes(sizes);

	// Set the main splitter stretch factors.
	// We want the QTreeView to stretch, but not the card info panel.
	d->ui.splitterMain->setStretchFactor(0, 1);
	d->ui.splitterMain->setStretchFactor(1, 0);

	// Set the models.
	d->proxyModel->setSourceModel(d->model);
	d->ui.lstBankList->setModel(d->proxyModel);

	// Sort by COL_BANKNUM by default.
	// TODO: Disable sorting on specific columns.
	//d->proxyModel->setDynamicSortFilter(true);
	//d->ui.lstBankList->sortByColumn(RvtHModel::COL_BANKNUM, Qt::AscendingOrder);

	// Initialize the UI.
	d->updateLstBankList();
	d->initToolbar();
	d->updateWindowTitle();

	/** Progress bar widgets **/
	d->lblMessage = new QLabel();
	d->lblMessage->setTextFormat(Qt::PlainText);
	d->ui.statusBar->addWidget(d->lblMessage, 1);

	// TODO: Handle "Escape" keypresses as cancel?
	// Disabling focus to prevent it from being highlighted.
	d->btnCancel = new QToolButton();
	d->btnCancel->setVisible(false);
	d->btnCancel->setFocusPolicy(Qt::NoFocus);
	d->btnCancel->setToolTip(tr("Cancel the current operation."));
	d->btnCancel->setStyleSheet(QLatin1String("margin: 0px; padding: 0px;"));
	if (QIcon::hasThemeIcon(QLatin1String("dialog-close"))) {
		d->btnCancel->setIcon(QIcon::fromTheme(QLatin1String("dialog-close")));
	} else {
		d->btnCancel->setIcon(style()->standardIcon(
			QStyle::SP_DialogCloseButton, nullptr, d->btnCancel));
	}
	connect(d->btnCancel, &QToolButton::clicked,
		this, &QRvtHToolWindow::btnCancel_clicked);
	d->ui.statusBar->addPermanentWidget(d->btnCancel, 1);

	d->progressBar = new QProgressBar();
	d->progressBar->setVisible(false);
	d->ui.statusBar->addPermanentWidget(d->progressBar, 1);

	// Set the progress bar's size so it doesn't randomly resize.
	d->progressBar->setMinimumWidth(320);
	d->progressBar->setMaximumWidth(320);

	// Connect the lstBankList selection signal.
	connect(d->ui.lstBankList->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &QRvtHToolWindow::lstBankList_selectionModel_selectionChanged);

#ifndef Q_OS_WIN
	// Initialize the Taskbar Button Manager. (Non-Windows systems)
	d->taskbarButtonManager = TaskbarButtonManagerFactory::createManager(this);
#endif /* Q_OS_WIN */
}

QRvtHToolWindow::~QRvtHToolWindow()
{
	delete d_ptr;
}

/**
 * Open an RVT-H Reader disk image.
 * @param filename Filename.
 */
void QRvtHToolWindow::openRvtH(const QString &filename)
{
	Q_D(QRvtHToolWindow);

	if (d->rvth) {
		d->model->setRvtH(nullptr);
		delete d->rvth;
	}

	// Open the specified RVT-H Reader disk image.
	int err = 0;
#ifdef _WIN32
	RvtH *const rvth_tmp = new RvtH(reinterpret_cast<const wchar_t*>(filename.utf16()), &err);
#else /* !_WIN32 */
	RvtH *const rvth_tmp = new RvtH(filename.toUtf8().constData(), &err);
#endif
	if (!rvth_tmp->isOpen() || err != 0) {
		// Unable to open the RVT-H Reader disk image.
		const QString errMsg = tr("An error occurred while opening '%1': %2")
			.arg(d->getDisplayFilename(filename))
			.arg(QString::fromUtf8(rvth_error(err)));
		d->ui.msgWidget->showMessage(errMsg, MessageWidget::ICON_CRITICAL);
		delete d->rvth;
		return;
	}

	d->rvth = rvth_tmp;
	d->filename = filename;
	d->model->setRvtH(d->rvth);

	// Check the NHCD table status.
	bool checkNHCD = false;
	switch (d->rvth->imageType()) {
		case RVTH_ImageType_HDD_Reader:
		case RVTH_ImageType_HDD_Image:
			// NHCD table should be present.
			checkNHCD = true;
			break;

		default:
			// No NHCD table here.
			break;
	}

	if (checkNHCD) {
		QString message;
		switch (d->rvth->nhcd_status()) {
			case NHCD_STATUS_OK:
				break;

			default:
			case NHCD_STATUS_UNKNOWN:
			case NHCD_STATUS_MISSING:
				message = tr("NHCD table is missing.");
				break;

			case NHCD_STATUS_HAS_MBR:
				message = tr("This appears to be a PC MBR-partitioned HDD.");
				break;

			case NHCD_STATUS_HAS_GPT:
				message = tr("This appears to be a PC GPT-partitioned HDD.");
				break;
		}

		if (!message.isEmpty()) {
			message += QChar(L'\n') + tr("Using defaults. Writing will be disabled.");
			d->ui.msgWidget->showMessage(message, MessageWidget::ICON_CRITICAL);
		}
	}

	// Update the UI.
	d->updateLstBankList();
	d->updateWindowTitle();

	// FIXME: If a file is opened from the command line,
	// QTreeView sort-of selects the first file.
	// (Signal is emitted, but nothing is highlighted.)
}

/**
 * Close the currently-opened RVT-H Reader disk image.
 */
void QRvtHToolWindow::closeRvtH(void)
{
	Q_D(QRvtHToolWindow);
	if (!d->rvth) {
		// Not open...
		return;
	}

	d->model->setRvtH(nullptr);
	delete d->rvth;
	d->rvth = nullptr;

	// Clear the filenames.
	d->filename.clear();

	// Update the UI.
	d->updateLstBankList();
	d->updateWindowTitle();
}

/**
 * Widget state has changed.
 * @param event State change event.
 */
void QRvtHToolWindow::changeEvent(QEvent *event)
{
	Q_D(QRvtHToolWindow);

	switch (event->type()) {
		case QEvent::LanguageChange:
			// Retranslate the UI.
			d->ui.retranslateUi(this);
			d->updateLstBankList();
			d->updateWindowTitle();
			d->retranslateToolbar();
			break;

		default:
			break;
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}

/**
 * Window show event.
 * @param event Window show event.
 */
void QRvtHToolWindow::showEvent(QShowEvent *event)
{
	Q_UNUSED(event);
	Q_D(QRvtHToolWindow);

	// Show all columns except signature status by default.
	// TODO: Allow the user to customize the columns, and save the
	// customized columns somewhere.
	if (!d->cols_init) {
		d->cols_init = true;
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_BANKNUM, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_TYPE, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_TITLE, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_DISCNUM, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_REVISION, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_REGION, false);
		d->ui.lstBankList->setColumnHidden(RvtHModel::COL_IOS_VERSION, false);
		static_assert(RvtHModel::COL_IOS_VERSION + 1 == RvtHModel::COL_MAX,
			"Default column visibility status needs to be updated!");
	}

	// Pass the event to the base class.
	super::showEvent(event);
}

/**
 * Window close event.
 * @param event Window close event.
 */
void QRvtHToolWindow::closeEvent(QCloseEvent *event)
{
	Q_D(QRvtHToolWindow);
	if (d->uiBusyCounter > 0 || d->workerObject ||
	    (d->workerThread && d->workerThread->isRunning())) {
		// UI is busy, or the worker thread is running.
		// Ignore the close event.
		event->ignore();
		return;
	}

	// Pass the event to the base class.
	super::closeEvent(event);
}

#ifdef Q_OS_WIN
/**
 * Windows message handler.
 * Used for TaskbarButtonManager.
 * @param eventType
 * @param message
 * @param result
 * @return
 */
bool QRvtHToolWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
	// Reference: http://nicug.blogspot.com/2011/03/windows-7-taskbar-extensions-in-qt.html
	Q_D(QRvtHToolWindow);
	const MSG *const msg = reinterpret_cast<const MSG*>(message);
	extern UINT WM_TaskbarButtonCreated;
	if (msg->message == WM_TaskbarButtonCreated) {
		// Initialize the Taskbar Button Manager.
		d->taskbarButtonManager = TaskbarButtonManagerFactory::createManager(this);
		if (d->taskbarButtonManager) {
			d->taskbarButtonManager->setWindow(this);
		}
	}
	return false;
}
#endif /* Q_OS_WIN */

/** UI busy functions **/

/**
 * Mark the UI as busy.
 * Calls to this function stack, so if markUiBusy()
 * was called 3 times, markUiNotBusy() must be called 3 times.
 */
void QRvtHToolWindow::markUiBusy(void)
{
	Q_D(QRvtHToolWindow);
	d->uiBusyCounter++;
	if (d->uiBusyCounter == 1) {
		// UI is now busy.
		// TODO: Prevent the window from being closed.
		this->setCursor(Qt::WaitCursor);
		d->ui.menuBar->setEnabled(false);
		d->ui.toolBar->setEnabled(false);
		this->centralWidget()->setEnabled(false);

		// TODO: Disable the close button?
	}
}

/**
 * Mark the UI as not busy.
 * Calls to this function stack, so if markUiBusy()
 * was called 3 times, markUiNotBusy() must be called 3 times.
 */
void QRvtHToolWindow::markUiNotBusy(void)
{
	Q_D(QRvtHToolWindow);
	if (d->uiBusyCounter <= 0) {
		// We're already not busy.
		// Don't decrement the counter, though.
		return;
	}

	d->uiBusyCounter--;
	if (d->uiBusyCounter == 0) {
		// UI is no longer busy.
		d->ui.menuBar->setEnabled(true);
		d->ui.toolBar->setEnabled(true);
		this->centralWidget()->setEnabled(true);
		this->unsetCursor();
	}
}

/** UI widget slots **/

/** Actions **/

/**
 * Open an RVT-H Reader disk image or standalone GCM disc image.
 */
void QRvtHToolWindow::on_actionOpenDiskImage_triggered(void)
{
	Q_D(QRvtHToolWindow);

	// TODO: Remove the space before the "*.raw"?
	// On Linux, Qt shows an extra space after the filter name, since
	// it doesn't show the extension. Not sure about Windows...
	const QString allSupportedFilter = tr("All Supported Files") +
		QLatin1String(" (*.img *.bin *.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString hddFilter = tr("RVT-H Reader Disk Image Files") +
		QLatin1String(" (*.img *.bin)");
	const QString gcmFilter = tr("GameCube/Wii Disc Image Files") +
		QLatin1String(" (*.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString allFilter = tr("All Files") + QLatin1String(" (*)");

	// NOTE: Using a QFileDialog instead of QFileDialog::getOpenFileName()
	// causes a non-native appearance on Windows. Hence, we should use
	// QFileDialog::getOpenFileName().
	const QString filters =
		allSupportedFilter + QLatin1String(";;") +
		hddFilter + QLatin1String(";;") +
		gcmFilter + QLatin1String(";;") +
		allFilter;

	// Get the filename.
	// TODO: d->lastPath()
	QString filename = QFileDialog::getOpenFileName(this,
			tr("Open RVT-H Reader Disk Image"),	// Dialog title
			d->lastPath,				// Default filename
			filters);				// Filters

	if (!filename.isEmpty()) {
		// Filename is selected.

		// Save the last path.
		d->lastPath = QFileInfo(filename).absolutePath();

		// Open the RVT-H Reader disk image.
		openRvtH(filename);
	}
}

/**
 * Open an RVT-H Reader disk image or standalone GCM disc image.
 */
void QRvtHToolWindow::on_actionOpenDevice_triggered(void)
{
	Q_D(QRvtHToolWindow);

	// Prompt the user to select a device.
	SelectDeviceDialog *const dialog = new SelectDeviceDialog();
	int ret = dialog->exec();
	if (ret == QDialog::Accepted) {
		QString deviceName = dialog->deviceName();
		if (!deviceName.isEmpty()) {
			// Filename is selected.
			// TODO: Show the serial number?
			openRvtH(deviceName);
		}
	}
}

/**
 * Close the currently-opened RVT-H Reader disk image or device.
 */
void QRvtHToolWindow::on_actionClose_triggered(void)
{
	Q_D(QRvtHToolWindow);
	if (!d->rvth)
		return;

	closeRvtH();
}

/**
 * Exit the program.
 * TODO: Separate close/exit for Mac OS X?
 */
void QRvtHToolWindow::on_actionExit_triggered(void)
{
	this->closeRvtH();
	this->close();
}

/**
 * Show the About dialog.
 */
void QRvtHToolWindow::on_actionAbout_triggered(void)
{
	AboutDialog::ShowSingle(this);
}

/** RVT-H disk image actions **/

/**
 * Extract the selected bank.
 */
void QRvtHToolWindow::on_actionExtract_triggered(void)
{
	Q_D(QRvtHToolWindow);

	if (d->workerObject || (d->workerThread && d->workerThread->isRunning())) {
		// Worker thread is already running.
		return;
	}

	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection())
		return;

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;

	// TODO: Sort proxy model like in mcrecover.
	const unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Prompt the user for a save location.
	QString filename = QFileDialog::getSaveFileName(this,
		tr("Extract Disc Image"),
		QString(),	// Default filename (TODO)
		// TODO: Remove extra space from the filename filter?
		tr("GameCube/Wii Disc Images") + QLatin1String(" (*.gcm);;") +
		tr("All Files") + QLatin1String(" (*)"));
	if (filename.isEmpty())
		return;

	// Disable the main UI widgets.
	markUiBusy();

	// Show the progress bar and cancel button.
	d->btnCancel->setVisible(true);
	d->progressBar->setVisible(true);
	d->progressBar->setMaximum(100);
	d->progressBar->setValue(0);

	// Initial message.
	const QString filenameOnly = d->getDisplayFilename(filename);
	d->lblMessage->setText(tr("Extracting Bank %1 to %2:")
		.arg(bank+1).arg(filenameOnly));

	// Recryption key.
	const int recryption_key = d->cboRecryptionKey->currentData().toInt();

	// TODO: NDEV flag?
	const unsigned int flags = 0;

	// Update status.
	// NOTE: We don't need to do an initial update for extract.
	d->updateStatus_didInitialUpdate = true;
	d->updateStatus_bank = bank;

	// Create the worker thread and object.
	d->workerThread = new QThread(this);
	d->workerObject = new WorkerObject();
	d->workerObject->moveToThread(d->workerThread);
	d->workerObject->setRvtH(d->rvth);
	d->workerObject->setBank(bank);
	d->workerObject->setGcmFilename(filename);
	d->workerObject->setRecryptionKey(recryption_key);
	d->workerObject->setFlags(flags);

	connect(d->workerThread, &QThread::started,
		d->workerObject, &WorkerObject::doExtract);
	connect(d->workerObject, &WorkerObject::updateStatus,
		this, &QRvtHToolWindow::workerObject_updateStatus);
	connect(d->workerObject, &WorkerObject::finished,
		this, &QRvtHToolWindow::workerObject_finished);

	// Start the thread.
	// Progress will be updated using callback signals.
	// TODO: Watchdog timer in case the thread fails?
	d->workerThread->start();
}

/**
 * Import an image into the selected bank.
 */
void QRvtHToolWindow::on_actionImport_triggered(void)
{
	Q_D(QRvtHToolWindow);

	if (d->workerObject || (d->workerThread && d->workerThread->isRunning())) {
		// Worker thread is already running.
		return;
	}

	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection())
		return;

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;

	// TODO: Sort proxy model like in mcrecover.
	const unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Prompt the user to select a disc image.
	QString filename = QFileDialog::getOpenFileName(this,
		tr("Import Disc Image"),
		QString(),	// Default filename (TODO)
		// TODO: Remove extra space from the filename filter?
		tr("GameCube/Wii Disc Images") + QLatin1String(" (*.gcm *.wbfs *.ciso);;") +
		tr("All Files") + QLatin1String(" (*)"));
	if (filename.isEmpty())
		return;

	// Disable the main UI widgets.
	markUiBusy();

	// Show the progress bar and cancel button.
	d->btnCancel->setVisible(true);
	d->progressBar->setVisible(true);
	d->progressBar->setMaximum(100);
	d->progressBar->setValue(0);

	// Initial message.
	const QString filenameOnly = d->getDisplayFilename(filename);
	d->lblMessage->setText(tr("Importing %1 to Bank %2:")
		.arg(filenameOnly).arg(bank+1));

	// Update status.
	// NOTE: We *do* need to do an initial update for import.
	d->updateStatus_didInitialUpdate = false;
	d->updateStatus_bank = bank;

	// Create the worker thread and object.
	d->workerThread = new QThread(this);
	d->workerObject = new WorkerObject();
	d->workerObject->moveToThread(d->workerThread);
	d->workerObject->setRvtH(d->rvth);
	d->workerObject->setBank(bank);
	d->workerObject->setGcmFilename(filename);

	connect(d->workerThread, &QThread::started,
		d->workerObject, &WorkerObject::doImport);
	connect(d->workerObject, &WorkerObject::updateStatus,
		this, &QRvtHToolWindow::workerObject_updateStatus);
	connect(d->workerObject, &WorkerObject::finished,
		this, &QRvtHToolWindow::workerObject_finished);

	// Start the thread.
	// Progress will be updated using callback signals.
	// TODO: Watchdog timer in case the thread fails?
	d->workerThread->start();
}

/**
 * Delete the selected bank.
 */
void QRvtHToolWindow::on_actionDelete_triggered(void)
{
	Q_D(QRvtHToolWindow);

	if (d->workerObject || (d->workerThread && d->workerThread->isRunning())) {
		// Worker thread is already running.
		return;
	}

	// TODO: Prompt the user to confirm?

	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection())
		return;

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;

	// TODO: Sort proxy model like in mcrecover.
	const unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Delete the selected bank.
	int ret = d->rvth->deleteBank(bank);

	QString text;
	QMessageBox::Icon notificationType;
	if (ret == 0) {
		// Successfully deleted.
		text = tr("Bank %1 deleted.").arg(bank+1);
		notificationType = QMessageBox::Information;
	} else {
		// An error occurred...
		text = tr("ERROR deleting Bank %1: %2")
			.arg(bank+1).arg(ret);
		notificationType = QMessageBox::Warning;
	}
	d->lblMessage->setText(text);
	MessageSound::play(notificationType, text, this);

	// Update the RVT-H model.
	d->model->forceBankUpdate(bank);
	// Update the BankEntryView.
	d->ui.bevBankEntryView->update();
	// Update the action enable status.
	d->updateActionEnableStatus();
}

/**
 * Undelete the selected bank.
 */
void QRvtHToolWindow::on_actionUndelete_triggered(void)
{
	Q_D(QRvtHToolWindow);

	if (d->workerObject || (d->workerThread && d->workerThread->isRunning())) {
		// Worker thread is already running.
		return;
	}

	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection())
		return;

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;

	// TODO: Sort proxy model like in mcrecover.
	const unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Undelete the selected bank.
	int ret = d->rvth->undeleteBank(bank);

	QString text;
	QMessageBox::Icon notificationType;
	if (ret == 0) {
		// Successfully deleted.
		text = tr("Bank %1 undeleted.").arg(bank+1);
		notificationType = QMessageBox::Information;
	} else {
		// An error occurred...
		text = tr("ERROR undeleting Bank %1: %2")
			.arg(bank+1).arg(ret);
		notificationType = QMessageBox::Warning;
	}
	d->lblMessage->setText(text);
	MessageSound::play(notificationType, text, this);

	// Update the RVT-H model.
	d->model->forceBankUpdate(bank);
	// Update the BankEntryView.
	d->ui.bevBankEntryView->update();
	// Update the action enable status.
	d->updateActionEnableStatus();
}

/** RvtHModel slots **/

void QRvtHToolWindow::rvthModel_layoutChanged(void)
{
	// Update the QTreeView columns, etc.
	// FIXME: This doesn't work the first time a file is added...
	// (possibly needs a dataChanged() signal)
	Q_D(QRvtHToolWindow);
	d->updateLstBankList();
}

void QRvtHToolWindow::rvthModel_rowsInserted(void)
{
	// A new file entry was added to the GcnCard.
	// Update the QTreeView columns.
	// FIXME: This doesn't work the first time a file is added...
	Q_D(QRvtHToolWindow);
	d->updateLstBankList();
}

/** lstBankList slots **/

void QRvtHToolWindow::lstBankList_selectionModel_selectionChanged(
	const QItemSelection& selected, const QItemSelection& deselected)
{
	Q_UNUSED(deselected)
	Q_D(QRvtHToolWindow);

	if (!d->rvth) {
		// No RVT-H Reader disk image.
		d->ui.bevBankEntryView->setBankEntry(nullptr);
		return;
	}

	// Determine the selected bank.
	// Note that selected.indexes() contains all columns,
	// so we just need to get the bank number from the first column.
	int bank = -1;
	const RvtH_BankEntry *entry = nullptr;
	QModelIndexList selList = selected.indexes();
	if (!selList.isEmpty()) {
		// TODO: Sort proxy model like in mcrecover.
		bank = d->proxyModel->mapToSource(selList[0]).row();
		entry = d->rvth->bankEntry(bank);
	}

	// Set the BankView's BankEntry to the selected bank.
	// NOTE: Only handles the first selected bank.
	d->ui.bevBankEntryView->setBankEntry(entry);

	// Update the action enable status.
	d->updateActionEnableStatus();
}

/** Worker object slots **/

/**
 * Update the status bar.
 * @param text Status bar text.
 * @param progress_value Progress bar value. (If -1, ignore this.)
 * @param progress_max Progress bar maximum. (If -1, ignore this.)
 */
void QRvtHToolWindow::workerObject_updateStatus(const QString &text, int progress_value, int progress_max)
{
	Q_D(QRvtHToolWindow);
	d->lblMessage->setText(text);

	// TODO: "Error" state.
	if (progress_value >= 0 && progress_max >= 0) {
		if (!d->updateStatus_didInitialUpdate) {
			// Update the RVT-H model.
			d->model->forceBankUpdate(d->updateStatus_bank);
			d->updateStatus_didInitialUpdate = true;
		}

		if (d->progressBar->maximum() != progress_max) {
			d->progressBar->setMaximum(progress_max);
		}
		d->progressBar->setValue(progress_value);

		if (d->taskbarButtonManager) {
			if (d->taskbarButtonManager->progressBarMax() != progress_max) {
				d->taskbarButtonManager->setProgressBarMax(progress_max);
			}
			d->taskbarButtonManager->setProgressBarValue(progress_value);
		}
	}
}

/**
 * Process is finished.
 * @param text Status bar text.
 * @param err Error code. (0 on success)
 */
void QRvtHToolWindow::workerObject_finished(const QString &text, int err)
{
	Q_D(QRvtHToolWindow);
	d->lblMessage->setText(text);

	// Hide the Cancel button.
	d->btnCancel->setVisible(false);

	if (err == 0) {
		// Process completed.
		d->progressBar->setValue(d->progressBar->maximum());
		MessageSound::play(QMessageBox::Information, text, this);
	} else {
		// Process failed.
		// TODO: Change progress bar to red?
		// TOOD: Critical vs. warning.
		MessageSound::play(QMessageBox::Warning, text, this);
	}

	// TODO: Hide the progress bar on success after 5 seconds.
	// TODO: Same with taskbar button, but for now, just clear it.
	if (d->taskbarButtonManager) {
		d->taskbarButtonManager->clearProgressBar();
	}

	// Make sure the thread exits.
	// NOTE: Connecting WorkerObject::finished() to QThread::quit()
	// might not work if the slots get run in the wrong order.
	// NOTE 2: QThread::wait() doesn't return if the thread has
	// finished before it's called, even though the documentation
	// says it does...
	d->workerThread->quit();
	do {
		d->workerThread->wait(250);
	} while (d->workerThread->isRunning());
	d->workerThread->deleteLater();
	d->workerThread = nullptr;

	// Delete the worker object. It'll be created again for the next process.
	// NOTE: Need to use deleteLater() to prevent race conditions.
	d->workerObject->deleteLater();
	d->workerObject = nullptr;

	// Enable the main UI widgets.
	markUiNotBusy();
}

/**
 * Cancel button was pressed.
 */
void QRvtHToolWindow::btnCancel_clicked(void)
{
	Q_D(QRvtHToolWindow);
	// TODO: Make sure there's no race conditions.
	if (d->workerObject) {
		// TODO: Delete the destination .gcm if necessary?
		// TODO: If importing, restore the old bank entry?
		// (may need librvth changes)
		d->workerObject->cancel();
	}
}
