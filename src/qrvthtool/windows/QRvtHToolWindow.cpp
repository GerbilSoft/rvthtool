/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.cpp: Main window.                                       *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
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

// Taskbar Button Manager
#include "TaskbarButtonManager/TaskbarButtonManager.hpp"
#include "TaskbarButtonManager/TaskbarButtonManagerFactory.hpp"
#ifdef Q_OS_WIN
#  include <windows.h>
#endif /* Q_OS_WIN */

// Other windows
#include "AboutDialog.hpp"

// Configuration
#include "config/ConfigStore.hpp"
#include "PathFuncs.hpp"

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

	// About dialog
	AboutDialog *aboutDialog;

	// RVT-H Reader disk image
	RvtH *rvth;
	RvtHModel *model;
	RvtHSortFilterProxyModel *proxyModel;

	// Filename
	QString filename;

	// NHCD table status for the frame title
	QString nhcd_status;

	// Last icon ID
	RvtHModel::IconID lastIconID;

	// Initialized columns?
	bool cols_init;

	// Enable write operations?
	// Should be enabled for RVT-H Readers with valid
	// NHCD tables and disabled otherwise.
	bool write_enabled;

	/**
	 * Update the RVT-H Reader disk image's QGroupBox title.
	 */
	void updateGrpBankListTitle(void);

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

	// Recryption Key widgets
	QLabel *lblRecryptionKey;
	QComboBox *cboRecryptionKey;

public:
	// Status bar widgets
	QLabel *lblMessage;
	QToolButton *btnCancel;
	QProgressBar *progressBar;

	// Worker thread
	QThread *workerThread;
	WorkerObject *workerObject;

	// UI busy counter
	int uiBusyCounter;

	// Taskbar Button Manager
	TaskbarButtonManager *taskbarButtonManager;

public:
	// Configuration
	ConfigStore *cfg;

	/**
	 * Get the last path.
	 * @return Last path
	 */
	QString lastPath(void) const;

	/**
	 * Set the last path.
	 * @param path Last path
	 */
	void setLastPath(const QString &path);

public:
	// Update status
	bool updateStatus_didInitialUpdate;
	int updateStatus_bank;

public:
	/**
	 * Get the selected bank number.
	 * @return Bank number, or -1 if no banks are selected.
	 */
	int selectedBankNumber(void) const;

	/**
	 * Get the selected bank entry.
	 * @return Bank entry, or nullptr if no banks are selected.
	 */
	const RvtH_BankEntry *selectedBankEntry(void) const;
};

QRvtHToolWindowPrivate::QRvtHToolWindowPrivate(QRvtHToolWindow *q)
	: q_ptr(q)
	, aboutDialog(nullptr)
	, rvth(nullptr)
	, model(new RvtHModel(q))
	, proxyModel(new RvtHSortFilterProxyModel(q))
	, lastIconID(RvtHModel::ICON_MAX)
	, cols_init(false)
	, write_enabled(false)
	, lblRecryptionKey(nullptr)
	, cboRecryptionKey(nullptr)
	, lblMessage(nullptr)
	, btnCancel(nullptr)
	, progressBar(nullptr)
	, workerThread(nullptr)
	, workerObject(nullptr)
	, uiBusyCounter(0)
	, taskbarButtonManager(nullptr)
	, cfg(new ConfigStore(q))
	, updateStatus_didInitialUpdate(false)
	, updateStatus_bank(0)
{
	// Connect the RvtHModel slots.
	QObject::connect(model, &RvtHModel::layoutChanged,
			 q, &QRvtHToolWindow::rvthModel_layoutChanged);
	QObject::connect(model, &RvtHModel::rowsInserted,
			 q, &QRvtHToolWindow::rvthModel_rowsInserted);

	// Configuration signals
	cfg->registerChangeNotification(QLatin1String("language"),
		q, SLOT(setTranslation_cfg_slot(QVariant)));
	cfg->registerChangeNotification(QLatin1String("maskDeviceSerialNumbers"),
		q, SLOT(maskDeviceSerialNumbers_cfg_slot(QVariant)));
}

QRvtHToolWindowPrivate::~QRvtHToolWindowPrivate()
{
	// Save the configuration.
	cfg->save();

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
	delete rvth;
}

/**
 * Update the RVT-H Reader disk image's QGroupBox title.
 */
void QRvtHToolWindowPrivate::updateGrpBankListTitle(void)
{
	if (!rvth) {
		// No RVT-H Reader is loaded.
		ui.grpBankList->setTitle(QRvtHToolWindow::tr("No RVT-H Reader disk image loaded."));
		return;
	}

	// Show the filename and device type.
	const QString displayFilename = getDisplayFilename(filename);
	QString imageType;
	switch (rvth->imageType()) {
		// HDDs (multiple banks)
		case RVTH_ImageType_HDD_Reader: {
			// TODO: Handle pErr.
			imageType = QRvtHToolWindow::tr("RVT-H Reader");

#ifdef HAVE_QUERY
			QString qs_full_serial;
#  ifdef _WIN32
			wchar_t *const s_full_serial = rvth_get_device_serial_number(
				reinterpret_cast<const wchar_t*>(filename.utf16()), nullptr);
			if (s_full_serial) {
				qs_full_serial = QString::fromUtf16(
					reinterpret_cast<const char16_t*>(s_full_serial));
				free(s_full_serial);
			}
#  else /* !_WIN32 */
			char *const s_full_serial = rvth_get_device_serial_number(
				filename.toUtf8().constData(), nullptr);
			if (s_full_serial) {
				qs_full_serial = QString::fromUtf8(s_full_serial);
				free(s_full_serial);
			}
#  endif /* _WIN32 */
#endif /* HAVE_QUERY */

			if (!qs_full_serial.isEmpty()) {
				const bool mask = cfg->get(QLatin1String("maskDeviceSerialNumbers")).toBool();
				if (mask) {
					// Mask the last 5 digits.
					// TODO: qsizetype?
					const int size = static_cast<int>(qs_full_serial.size());
					if (size > 5) {
						for (int i = size - 5; i < size; i++) {
							qs_full_serial[i] = QChar(L'x');
						}
					} else {
						// Mask the entire thing?
						qs_full_serial = QString(size, QChar(L'x'));
					}
				}

				imageType += QChar(L' ');
				imageType += qs_full_serial;
			}

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
		if (!nhcd_status.isEmpty()) {
			ui.grpBankList->setTitle(
				QRvtHToolWindow::tr("%1 [%2] [%3]")
					.arg(displayFilename, imageType, nhcd_status));
		} else {
			ui.grpBankList->setTitle(
				QRvtHToolWindow::tr("%1 [%2]")
					.arg(displayFilename, imageType));
		}
	} else {
		ui.grpBankList->setTitle(displayFilename);
	}
}

/**
 * Update the RVT-H Reader disk image's QTreeView.
 */
void QRvtHToolWindowPrivate::updateLstBankList(void)
{
	// Update grpBankList's title.
	updateGrpBankListTitle();

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
		return;
	}

	// RVT-H Reader image is loaded.
	// TODO: Disable open, scan, and save (all) if we're scanning.
	ui.actionClose->setEnabled(true);

	// If a bank is selected, enable the actions.
	const RvtH_BankEntry *const entry = ui.bevBankEntryView->bankEntry();
	if (!entry) {
		// No entry. Disable everything.
		ui.actionExtract->setEnabled(false);
		ui.actionImport->setEnabled(false);
		ui.actionDelete->setEnabled(false);
		ui.actionUndelete->setEnabled(false);
		return;
	}

	// Enable Extract if the bank is *not* empty.
	ui.actionExtract->setEnabled(entry->type != RVTH_BankType_Empty);

	// d->write_enabled indicates if we can use writing functions.
	// True if using an actual RVT-H Reader with valid NHCD table;
	// false otherwise.
	if (this->write_enabled) {
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
		// Set the file path. Qt automatically takes the filename
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
	if (rfn.startsWith(QStringLiteral("\\\\.\\PhysicalDrive"), Qt::CaseInsensitive) ||
	    rfn.startsWith(QStringLiteral("//./PhysicalDrive"), Qt::CaseInsensitive))
	{
		// Physical drive.
		// TODO: Make sure it's all backslashes.
		removeDir = false;
	}
#else /* !_WIN32 */
	// Does the path start with "/dev/"?
	if (rfn.startsWith(QStringLiteral("/dev/"))) {
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
	lblRecryptionKey = new QLabel(q);
	lblRecryptionKey->setObjectName(QStringLiteral("lblRecryptionKey"));
	lblRecryptionKey->setContentsMargins(4, 0, 4, 0);
	ui.toolBar->insertWidget(ui.actionAbout, lblRecryptionKey);

	cboRecryptionKey = new QComboBox(q);
	cboRecryptionKey->setObjectName(QStringLiteral("cboRecryptionKey"));
	cboRecryptionKey->addItem(QString(), -1);			// None
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Retail);	// Retail (fakesigned)
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Korean);	// Korean (fakesigned)
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_Debug);	// Debug (fakesigned)
	//cboRecryptionKey->addItem(QString(), RVL_CryptoType_vWii);	// vWii (fakesigned) [FIXME: Not usable for discs...}
	//cboRecryptionKey->addItem(QString(), RVL_CryptoType_vWii_Debug);	// vWii (debug) (fakesigned) [FIXME: Not usable for discs...}
	cboRecryptionKey->setCurrentIndex(0);
	ui.toolBar->insertWidget(ui.actionAbout, cboRecryptionKey);

	// Make sure the "About" button is right-aligned.
	QWidget *const spacer = new QWidget(q);
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
	//cboRecryptionKey->setItemText(4, "vWii (fakesigned)");	// FIXME: Not usable for discs...
	cboRecryptionKey->setCurrentIndex(0);
}

/**
 * Get the last path.
 * @return Last path
 */
QString QRvtHToolWindowPrivate::lastPath(void) const
{
	// TODO: Cache the last absolute path?

	// NOTE: Path is stored using native separators.
	QString path = QDir::fromNativeSeparators(
		cfg->get(QLatin1String("lastPath")).toString());

	// If this is a relative path, convert to absolute.
	if (path == QLatin1String(".")) {
		// Application directory
		path = QCoreApplication::applicationDirPath();
	} else if (path == QLatin1String("~")) {
		// Home directory
		path = QDir::home().absolutePath();
	} else if (path.startsWith(QLatin1String("./"))) {
		// Path is relative to the application directory.
		QDir applicationDir(QCoreApplication::applicationDirPath());
		path.remove(0, 2);
		path = applicationDir.absoluteFilePath(path);
	} else if (path.startsWith(QLatin1String("~/"))) {
		// Path is relative to the user's home directory.
		path.remove(0, 2);
		path = QDir::home().absoluteFilePath(path);
	}

	return path;
}

/**
 * Set the last path.
 * @param path Last path
 */
void QRvtHToolWindowPrivate::setLastPath(const QString &path)
{
	// TODO: Relative to application directory on Windows?
	QFileInfo fileInfo(path);
	QString lastPath;
	if (fileInfo.isDir())
		lastPath = path;
	else
		lastPath = fileInfo.dir().absolutePath();

	// Make this path relative to the application directory.
	lastPath = PathFuncs::makeRelativeToApplication(lastPath);
#ifndef Q_OS_WIN
	// Make this path relative to the user's home directory.
	lastPath = PathFuncs::makeRelativeToHome(lastPath);
#endif /* !Q_OS_WIN */

	cfg->set(QLatin1String("lastPath"),
		 QDir::toNativeSeparators(lastPath));
}

/**
 * Get the selected bank number.
 * @return Bank number, or -1 if no banks are selected.
 */
int QRvtHToolWindowPrivate::selectedBankNumber(void) const
{
	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection()) {
		return -1;
	}

	QModelIndex index = selectionModel->currentIndex();
	if (!index.isValid()) {
		return -1;
	}

	return proxyModel->mapToSource(index).row();
}

/**
 * Get the selected bank entry.
 * @return Bank entry, or nullptr if no banks are selected.
 */
const RvtH_BankEntry *QRvtHToolWindowPrivate::selectedBankEntry(void) const
{
	const int bank = selectedBankNumber();
	return (bank >= 0) ? rvth->bankEntry(static_cast<unsigned int>(bank)) : nullptr;
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
	// Mac OS: Remove the window icon initially.
	// It will be set when a filename is selected.
	this->setWindowIcon(QIcon());
#endif /* Q_OS_MAC */

#ifdef Q_OS_WIN
	// Hide the QMenuBar border on Win32.
	// FIXME: This causes the menu bar to be "truncated" when using
	// the Aero theme on Windows Vista and 7.
#if 0
	this->Ui_QRvtHToolWindow::menuBar->setStyleSheet(
		QStringLiteral("QMenuBar { border: none }"));
#endif
#endif

	// Set up the main splitter sizes.
	// We want the card info panel to be 160px wide at startup.
	// TODO: Save positioning settings somewhere?
	static constexpr int BankInfoPanelWidth = 256;
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
	d->initToolbar();
	d->updateLstBankList();
	d->updateWindowTitle();
	d->updateActionEnableStatus();

	/** Progress bar widgets **/
	d->lblMessage = new QLabel();
	d->lblMessage->setObjectName(QStringLiteral("lblMessage"));
	d->lblMessage->setTextFormat(Qt::PlainText);
	d->ui.statusBar->addWidget(d->lblMessage, 1);

	// TODO: Handle "Escape" keypresses as cancel?
	// Disabling focus to prevent it from being highlighted.
	d->btnCancel = new QToolButton();
	d->btnCancel->setObjectName(QStringLiteral("btnCancel"));
	d->btnCancel->setVisible(false);
	d->btnCancel->setFocusPolicy(Qt::NoFocus);
	d->btnCancel->setToolTip(tr("Cancel the current operation."));
	d->btnCancel->setStyleSheet(QStringLiteral("margin: 0px; padding: 0px;"));
	if (QIcon::hasThemeIcon(QStringLiteral("process-stop"))) {
		d->btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
	} else {
		d->btnCancel->setIcon(style()->standardIcon(
			QStyle::SP_DialogCloseButton, nullptr, d->btnCancel));
	}
	connect(d->btnCancel, &QToolButton::clicked,
		this, &QRvtHToolWindow::btnCancel_clicked);
	d->ui.statusBar->addPermanentWidget(d->btnCancel, 1);

	d->progressBar = new QProgressBar();
	d->progressBar->setObjectName(QStringLiteral("progressBar"));
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

	// Emit all configuration signals.
	d->cfg->notifyAll();
}

QRvtHToolWindow::~QRvtHToolWindow()
{
	delete d_ptr;
}

/**
 * Open an RVT-H Reader disk image.
 * @param filename Filename
 * @param isDevice True if opened using SelectDeviceDialog
 */
void QRvtHToolWindow::openRvtH(const QString &filename, bool isDevice)
{
	Q_D(QRvtHToolWindow);

	if (d->rvth) {
		d->model->setRvtH(nullptr);
		delete d->rvth;
		d->rvth = nullptr;
	}

	// Processing...
	QString text;
	if (isDevice) {
		text = tr("Opening RVT-H Reader device '%1'...").arg(filename);
	} else {
		text = tr("Opening disc image file '%1'...").arg(filename);
	}
	d->lblMessage->setText(text);
	markUiBusy();
	QCoreApplication::processEvents();

	// Open the specified RVT-H Reader disk image.
	// NOTE: RvtH expects native separators.

	int err = 0;
	const QString filenameNativeSeparators = QDir::toNativeSeparators(filename);
#ifdef _WIN32
	RvtH *const rvth_tmp = new RvtH(reinterpret_cast<const wchar_t*>(filenameNativeSeparators.utf16()), &err);
#else /* !_WIN32 */
	RvtH *const rvth_tmp = new RvtH(filenameNativeSeparators.toUtf8().constData(), &err);
#endif
	if (!rvth_tmp->isOpen() || err != 0) {
		// Unable to open the RVT-H Reader disk image.
		const QString errMsg = tr("An error occurred while opening '%1': %2")
			.arg(d->getDisplayFilename(filename), QString::fromUtf8(rvth_error(err)));
		d->ui.msgWidget->showMessage(errMsg, MessageWidget::ICON_CRITICAL);
		delete rvth_tmp;
		d->lblMessage->setText(QString());
		markUiNotBusy();
		return;
	}

	d->rvth = rvth_tmp;
	d->filename = filename;
	d->model->setRvtH(d->rvth);

	d->nhcd_status.clear();
	d->write_enabled = false;

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
				if (d->rvth->imageType() == RVTH_ImageType_HDD_Reader) {
					d->write_enabled = true;
				}
				break;

			default:
			case NHCD_STATUS_UNKNOWN:
			case NHCD_STATUS_MISSING:
				message = tr("NHCD table is missing.");
				d->nhcd_status = QStringLiteral("!NHCD");
				break;

			case NHCD_STATUS_HAS_MBR:
				message = tr("This appears to be a PC MBR-partitioned HDD.");
				d->nhcd_status = QStringLiteral("MBR?");
				break;

			case NHCD_STATUS_HAS_GPT:
				message = tr("This appears to be a PC GPT-partitioned HDD.");
				d->nhcd_status = QStringLiteral("GPT?");
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
	d->updateActionEnableStatus();

	// FIXME: If a file is opened from the command line,
	// QTreeView sort-of selects the first file.
	// (Signal is emitted, but nothing is highlighted.)
	d->lblMessage->setText(QString());
	markUiNotBusy();
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

	// Clear the filename and NHCD status.
	d->filename.clear();
	d->nhcd_status.clear();

	// Clear the status bar.
	d->btnCancel->setVisible(false);
	d->progressBar->setVisible(false);
	d->lblMessage->clear();

	// Update the UI.
	d->updateLstBankList();
	d->updateWindowTitle();
	d->updateActionEnableStatus();
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
 * Windows native event handler.
 * Used for TaskbarButtonManager.
 * @param eventType
 * @param message
 * @param result
 * @return
 */
bool QRvtHToolWindow::nativeEvent(const QByteArray &eventType, void *message, native_event_result_t *result)
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
		// TODO: Disable the close button?
		this->setCursor(Qt::WaitCursor);
		d->ui.menuBar->setEnabled(false);
		d->ui.toolBar->setEnabled(false);
		this->centralWidget()->setEnabled(false);

		// Make sure the cursor actually updates.
		QCoreApplication::processEvents();
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

		// Make sure the cursor actually updates.
		QCoreApplication::processEvents();
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
		QStringLiteral(" (*.img *.bin *.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString hddFilter = tr("RVT-H Reader Disk Image Files") +
		QStringLiteral(" (*.img *.bin)");
	const QString gcmFilter = tr("GameCube/Wii Disc Image Files") +
		QStringLiteral(" (*.gcm *.wbfs *.ciso *.cso *.iso)");
	const QString allFilter = tr("All Files") + QStringLiteral(" (*)");

	// NOTE: Using a QFileDialog instead of QFileDialog::getOpenFileName()
	// causes a non-native appearance on Windows. Hence, we should use
	// QFileDialog::getOpenFileName().
	const QString filters =
		allSupportedFilter + QStringLiteral(";;") +
		hddFilter + QStringLiteral(";;") +
		gcmFilter + QStringLiteral(";;") +
		allFilter;

	// Get the filename.
	QString filename = QFileDialog::getOpenFileName(this,
			tr("Open RVT-H Reader Disk Image"),	// Dialog title
			d->lastPath(),				// Default filename
			filters);				// Filters

	if (!filename.isEmpty()) {
		// Filename is selected.

		// Save the last path.
		d->setLastPath(QFileInfo(filename).absolutePath());

		// Open the RVT-H Reader disk image.
		openRvtH(filename, false);
	}
}

/**
 * Open an RVT-H Reader device.
 */
void QRvtHToolWindow::on_actionOpenDevice_triggered(void)
{
	// Prompt the user to select a device.
	Q_D(QRvtHToolWindow);
	SelectDeviceDialog *const selectDeviceDialog = new SelectDeviceDialog(d->cfg, this);
	selectDeviceDialog->setObjectName(QStringLiteral("selectDeviceDialog"));
	int ret = selectDeviceDialog->exec();
	if (ret == QDialog::Accepted) {
		QString deviceName = selectDeviceDialog->deviceName();
		if (!deviceName.isEmpty()) {
			// RVT-H Reader device is selected.
			openRvtH(deviceName, true);
		}
	}

	delete selectDeviceDialog;
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
	Q_D(QRvtHToolWindow);
	if (d->aboutDialog) {
		// FIXME: This doesn't seem to work on KDE Plasma 5.25...
		d->aboutDialog->raise();
		d->aboutDialog->setFocus();
		return;
	}

	d->aboutDialog = new AboutDialog(this);
	d->aboutDialog->setObjectName(QStringLiteral("aboutDialog"));
	QObject::connect(d->aboutDialog, &QObject::destroyed,
		[d](QObject *obj) { Q_UNUSED(obj); d->aboutDialog = nullptr; });
	d->aboutDialog->show();
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
	if (!selectionModel->hasSelection()) {
		return;
	}

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return;
	}

	const unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Prompt the user for a save location.
	QString filename = QFileDialog::getSaveFileName(this,
		tr("Extract Disc Image"),
		QString(),	// Default filename (TODO)
		// TODO: Remove extra space from the filename filter?
		tr("GameCube/Wii Disc Images") + QStringLiteral(" (*.gcm);;") +
		tr("All Files") + QStringLiteral(" (*)"));
	if (filename.isEmpty()) {
		return;
	}

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
	d->workerThread->setObjectName(QStringLiteral("workerThread"));
	d->workerObject = new WorkerObject();
	d->workerObject->setObjectName(QStringLiteral("workerObject"));
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
		tr("GameCube/Wii Disc Images") + QStringLiteral(" (*.gcm *.wbfs *.ciso);;") +
		tr("All Files") + QStringLiteral(" (*)"));
	if (filename.isEmpty()) {
		return;
	}

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
	d->workerThread->setObjectName(QStringLiteral("workerThread"));
	d->workerObject = new WorkerObject();
	d->workerObject->setObjectName(QStringLiteral("workerObject"));
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
	// FIXME: This doesn't work the first time a bank is added...
	// (possibly needs a dataChanged() signal)
	Q_D(QRvtHToolWindow);
	d->updateLstBankList();
}

void QRvtHToolWindow::rvthModel_rowsInserted(void)
{
	// A new bank entry was added to the RVT-H Reader.
	// Update the QTreeView columns.
	// FIXME: This doesn't work the first time a bank is added...
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
 * @param text Status bar text
 * @param progress_value Progress bar value (If -1, ignore this.)
 * @param progress_max Progress bar maximum (If -1, ignore this.)
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
 * @param text Status bar text
 * @param err Error code (0 on success)
 */
void QRvtHToolWindow::workerObject_finished(const QString &text, int err)
{
	Q_D(QRvtHToolWindow);

	// Hide the Cancel button.
	d->btnCancel->setVisible(false);

	if (err == 0) {
		// Process completed.
		d->lblMessage->setText(text);
		d->progressBar->setValue(d->progressBar->maximum());
		MessageSound::play(QMessageBox::Information, text, this);
	} else {
		// Process failed.
		// TODO: Change progress bar to red?
		// TOOD: Critical vs. warning.
		if (err == -ECANCELED) {
			// Operation was cancelled. Show the message as-is.
			d->lblMessage->setText(text);
			MessageSound::play(QMessageBox::Warning, text, this);
		} else {
			// Other error. Append the error message.
			QString errmsg;
			if (err >= 0) {
				errmsg = QCoreApplication::translate("RvtH|Error", rvth_error(err));
			} else {
				// POSIX error code.
				// TODO: QIODevice has some translations, but it requires
				// getting the C version first...
				errmsg = QString::fromUtf8(rvth_error(err));
			}

			// tr: %1 == general error from worker object; %2 == actual error message
			QString fullmsg = tr("%1: %2").arg(text, errmsg);
			d->lblMessage->setText(fullmsg);
			MessageSound::play(QMessageBox::Warning, fullmsg, this);
		}
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

	// Update BankEntryView.
	// TODO: Only if importing?
	const RvtH_BankEntry *const entry = d->selectedBankEntry();
	d->ui.bevBankEntryView->setBankEntry(entry);

	// Enable the main UI widgets.
	d->updateActionEnableStatus();
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

/** Configuration slots **/

/**
 * "Mask Device Serial Numbers" option was changed by the user.
 * @param mask If true, mask device serial numbers.
 */
void QRvtHToolWindow::on_actionMaskDeviceSerialNumbers_triggered(bool mask)
{
	Q_D(QRvtHToolWindow);
	d->cfg->set(QLatin1String("maskDeviceSerialNumbers"), mask);
}

/**
 * "Mask Device Serial Numbers" option was changed by the configuration.
 * @param mask If true, mask device serial numbers.
 */
void QRvtHToolWindow::maskDeviceSerialNumbers_cfg_slot(const QVariant &mask)
{
	Q_D(QRvtHToolWindow);
	d->ui.actionMaskDeviceSerialNumbers->setChecked(mask.toBool());

	// Update grpBankList's title.
	d->updateGrpBankListTitle();
}

/**
 * UI language was changed by the user.
 * @param locale Locale tag, e.g. "en_US".
 */
void QRvtHToolWindow::on_menuLanguage_languageChanged(const QString &locale)
{
	Q_D(QRvtHToolWindow);

	// TODO: Verify that the specified locale is valid.
	// (LanguageMenu::isLanguageSupported() or something?)
	// d->cfg->set() will trigger a notification.
	d->cfg->set(QLatin1String("language"), locale);
}

/**
 * UI language was changed by the configuration.
 * @param locale Locale tag, e.g. "en_US".
 */
void QRvtHToolWindow::setTranslation_cfg_slot(const QVariant &locale)
{
	Q_D(QRvtHToolWindow);
	d->ui.menuLanguage->setLanguage(locale.toString());
}
