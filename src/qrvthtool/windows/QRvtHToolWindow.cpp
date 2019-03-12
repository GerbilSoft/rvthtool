/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QRvtHToolWindow.cpp: Main window.                                       *
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

#include "QRvtHToolWindow.hpp"

#include "librvth/rvth.hpp"
#include "RvtHModel.hpp"
#include "RvtHSortFilterProxyModel.hpp"

#include "windows/SelectDeviceDialog.hpp"

// Qt includes.
#include <QtGui/QCloseEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>

// RVL_CryptoType_e
#include "libwiicrypto/sig_tools.h"

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
		QString displayFilename;	// filename without subdirectories

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
		// TODO: StatusBarManager.
		QLabel *lblMessage;
		QProgressBar *progressBar;

		// UI busy counter
		int uiBusyCounter;

	public:
		/**
		 * RVT-H progress callback.
		 * @param state		[in] Current progress.
		 * @param userdata	[in] User data specified when calling the RVT-H function.
		 * @return True to continue; false to abort.
		 */
		static bool progress_callback(const RvtH_Progress_State *state, void *userdata);

		// Current progress operation.
		QString cur_filenameOnly;
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
	// TODO: StatusBarManager.
	, lblMessage(nullptr)
	, progressBar(nullptr)
	, uiBusyCounter(0)
{
	// Connect the RvtHModel slots.
	QObject::connect(model, &RvtHModel::layoutChanged,
			 q, &QRvtHToolWindow::rvthModel_layoutChanged);
	QObject::connect(model, &RvtHModel::rowsInserted,
			 q, &QRvtHToolWindow::rvthModel_rowsInserted);
}

QRvtHToolWindowPrivate::~QRvtHToolWindowPrivate()
{
	// NOTE: Delete the MemCardModel first to prevent issues later.
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
		// Show the filename.
		// TODO: Get the device serial number.
		ui.grpBankList->setTitle(displayFilename);
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
	} else {
		// Memory card image is loaded.
		// TODO: Disable open, scan, and save (all) if we're scanning.
		ui.actionClose->setEnabled(true);
		ui.actionExtract->setEnabled(
			ui.lstBankList->selectionModel()->hasSelection());
	}
}

/**
 * Update the window title.
 */
void QRvtHToolWindowPrivate::updateWindowTitle(void)
{
	QString windowTitle;
	RvtHModel::IconID iconID;
	if (rvth) {
		windowTitle += displayFilename;
		windowTitle += QLatin1String(" - ");
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
		// Use the RVT-H icon as the default.
		iconID = RvtHModel::ICON_RVTH;
	}
	windowTitle += QApplication::applicationName();

	Q_Q(QRvtHToolWindow);
	q->setWindowTitle(windowTitle);

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
 * Initialize the toolbar.
 */
void QRvtHToolWindowPrivate::initToolbar(void)
{
	Q_Q(QRvtHToolWindow);

	// Disable per-bank actions by default.
	ui.actionExtract->setEnabled(false);

	// Recryption Hey.
	ui.toolBar->insertSeparator(ui.actionAbout);
	lblRecryptionKey = new QLabel(QRvtHToolWindow::tr("Recryption Key:"), q);
	lblRecryptionKey->setObjectName(QLatin1String("lblRecryptionKey"));
	lblRecryptionKey->setToolTip(QRvtHToolWindow::tr(
		"Set the encryption key to use when extracting disc images.\n"
		"Default is None, which retains the original key."));
	ui.toolBar->insertWidget(ui.actionAbout, lblRecryptionKey);

	cboRecryptionKey = new QComboBox(q);
	cboRecryptionKey->addItem(QString(), RVL_CryptoType_None);	// None
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

/**
 * RVT-H progress callback.
 * @param state		[in] Current progress.
 * @param userdata	[in] User data specified when calling the RVT-H function.
 * @return True to continue; false to abort.
 */
bool QRvtHToolWindowPrivate::progress_callback(const RvtH_Progress_State *state, void *userdata)
{
	QRvtHToolWindowPrivate *const d = static_cast<QRvtHToolWindowPrivate*>(userdata);

	#define MEGABYTE (1048576 / RVTH_BLOCK_SIZE)
	switch (state->type) {
		case RVTH_PROGRESS_EXTRACT:
			d->lblMessage->setText(
				QRvtHToolWindow::tr("Extracting to %1: %L2 MiB / %L3 MiB copied...")
				.arg(d->cur_filenameOnly)
				.arg(state->lba_processed / MEGABYTE)
				.arg(state->lba_total / MEGABYTE));
			break;
		case RVTH_PROGRESS_IMPORT:
			d->lblMessage->setText(
				QRvtHToolWindow::tr("Importing from %1: %L2 MiB / %L3 MiB copied...")
				.arg(d->cur_filenameOnly)
				.arg(state->lba_processed / MEGABYTE)
				.arg(state->lba_total / MEGABYTE));
			break;
		case RVTH_PROGRESS_RECRYPT:
			if (state->lba_total <= 1) {
				// TODO: Encryption types?
				if (state->lba_processed == 0) {
					d->lblMessage->setText(
						QRvtHToolWindow::tr("Recrypting the ticket(s) and TMD(s)..."));
				}
			} else {
				d->lblMessage->setText(
					QRvtHToolWindow::tr("Recrypting to %1: %L2 MiB / %L3 MiB copied...")
					.arg(d->cur_filenameOnly)
					.arg(state->lba_processed / MEGABYTE)
					.arg(state->lba_total / MEGABYTE));
			}
			break;
		default:
			// FIXME
			assert(false);
			return false;
	}

	// Update the progress bar.
	// NOTE: Checking existing maximum value to prevent unnecessary updates.
	// TODO: Handle RVTH_PROGRESS_RECRYPT?
	if (state->type != RVTH_PROGRESS_RECRYPT) {
		if (d->progressBar->maximum() != state->lba_total) {
			d->progressBar->setMaximum(state->lba_total);
		}
		d->progressBar->setValue(state->lba_processed);
	}

	// TODO: Use a separate thread instead of calling processEvents().
	qApp->processEvents();
	return true;
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

	// TODO: StatusBarManager.
	d->lblMessage = new QLabel();
	d->lblMessage->setTextFormat(Qt::PlainText);
	d->ui.statusBar->addWidget(d->lblMessage, 1);

	d->progressBar = new QProgressBar();
	d->progressBar->setVisible(false);
	d->ui.statusBar->addPermanentWidget(d->progressBar, 1);

	// Set the progress bar's size so it doesn't randomly resize.
	d->progressBar->setMinimumWidth(320);
	d->progressBar->setMaximumWidth(320);

	// Connect the lstBankList selection signal.
	connect(d->ui.lstBankList->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &QRvtHToolWindow::lstBankList_selectionModel_selectionChanged);
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
#ifdef _WIN32
	RvtH *const rvth_tmp = new RvtH(reinterpret_cast<const wchar_t*>(filename.utf16()), nullptr);
#else /* !_WIN32 */
	RvtH *const rvth_tmp = new RvtH(filename.toUtf8().constData(), nullptr);
#endif
	if (!rvth_tmp->isOpen()) {
		// FIXME: Show an error message?
		delete d->rvth;
		return;
	}

	d->rvth = rvth_tmp;
	d->filename = filename;
	d->model->setRvtH(d->rvth);

	// Extract the filename from the path.
	// TODO: Use QFileInfo or similar?
	d->displayFilename = filename;
	bool removeDir = true;
#ifdef _WIN32
	// Does the path start with "\\\\.\\PhysicalDriveN"?
	// FIXME: How does Qt's native slashes interact with this?
	if (d->displayFilename.startsWith(QLatin1String("\\\\.\\PhysicalDrive")) ||
	    d->displayFilename.startsWith(QLatin1String("//./PhysicalDrive")))
	{
		// Physical drive.
		// TODO: Make sure it's all backslashes.
		removeDir = false;
	}
#else /* !_WIN32 */
	// Does the path start with "/dev/"?
	if (d->displayFilename.startsWith(QLatin1String("/dev/"))) {
		// Physical drive.
		removeDir = false;
	}
#endif

	if (removeDir) {
		int lastSlash = d->displayFilename.lastIndexOf(QChar(L'/'));
		if (lastSlash >= 0) {
			d->displayFilename.remove(0, lastSlash + 1);
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
	d->displayFilename.clear();

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
	if (d->uiBusyCounter > 0) {
		// UI is busy. Ignore the close event.
		event->ignore();
		return;
	}

	// Pass the event to the base class.
	super::closeEvent(event);
}

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
	// TODO
	//AboutDialog::ShowSingle(this);
}

/** RVT-H disk image actions **/

/**
 * Extract the selected bank.
 */
void QRvtHToolWindow::on_actionExtract_triggered(void)
{
	Q_D(QRvtHToolWindow);

	// Only one bank can be selected.
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (!selectionModel->hasSelection())
		return;

	QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
	if (!index.isValid())
		return;

	// TODO: Sort proxy model like in mcrecover.
	unsigned int bank = d->proxyModel->mapToSource(index).row();

	// Prompt the user for a save location.
	QString filename = QFileDialog::getSaveFileName(this,
		tr("Extract Disc Image"),
		QString(),	// Default filename (TODO)
		// TODO: Remove extra space from the filename filter?
		tr("GameCube/Wii Disc Images") + QLatin1String(" (*.gcm);;") +
		tr("All Files") + QLatin1String(" (*)"));
	if (filename.isEmpty())
		return;

	// TODO:
	// - Use a separate thread.
	// - Port StatusBarManager from mcrecover.
	// - Add a "Cancel" button.

	// Disable main UI widgets.
	markUiBusy();

	// Show the progress bar.
	d->progressBar->setVisible(true);
	d->progressBar->setMaximum(100);
	d->progressBar->setValue(0);

	// Initial message.
	const QString filenameOnly = QFileInfo(filename).fileName();
	d->lblMessage->setText(tr("Extracting to %1:").arg(filenameOnly));

	// Recryption key.
	const int recrypt_key = d->cboRecryptionKey->currentData().toInt();

	// TODO: NDEV flag?
	const unsigned int flags = 0;

	// Save the information for the callback.
	d->cur_filenameOnly = filenameOnly;

	// Start the extraction.
	// NOTE: Callback is set to use the private class.
#ifdef _WIN32
	int ret = d->rvth->extract(bank, reinterpret_cast<const wchar_t*>(filename.utf16()),
		recrypt_key, flags, d->progress_callback, d);
#else /* !_WIN32 */
	int ret = d->rvth->extract(bank, filename.toUtf8().constData(),
		recrypt_key, flags, d->progress_callback, d);
#endif /* _WIN32 */

	// TODO: Better error handling in StatusBarManager.
	d->lblMessage->setText(tr("Bank %1 extracted into %2: return code %3")
		.arg(bank+1).arg(filenameOnly).arg(ret));

	// Enable main UI widgets.
	markUiNotBusy();
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
	Q_UNUSED(selected)
	Q_UNUSED(deselected)
	Q_D(QRvtHToolWindow);

	if (!d->rvth) {
		// No RVT-H Reader disk image.
		d->ui.bevBankEntryView->setBankEntry(nullptr);
		return;
	}

	// FIXME: QItemSelection::indexes() *crashes* in MSVC debug builds. (Qt 4.8.6)
	// References: (search for "QModelIndexList assertion", no quotes)
	// - http://www.qtforum.org/article/13355/qt4-qtableview-assertion-failure.html#post66572
	// - http://www.qtcentre.org/threads/55614-QTableView-gt-selectionModel%20%20-gt-selection%20%20-indexes%20%20-crash#8766774666573257762
	// - https://forum.qt.io/topic/24664/crash-with-qitemselectionmodel-selectedindexes
	//QModelIndexList indexes = selected.indexes();
	int bank = -1;
	const RvtH_BankEntry *entry = nullptr;
	QItemSelectionModel *const selectionModel = d->ui.lstBankList->selectionModel();
	if (selectionModel->hasSelection()) {
		// TODO: If multiple banks are selected, and one of the
		// banks was just now unselected, this will still be the
		// unselected bank.
		QModelIndex index = d->ui.lstBankList->selectionModel()->currentIndex();
		if (index.isValid()) {
			// TODO: Sort proxy model like in mcrecover.
			bank = d->proxyModel->mapToSource(index).row();
			// TODO: Check for errors?
			entry = d->rvth->bankEntry(bank, nullptr);
		}
	}

	// If file(s) are selected, enable the Save action.
	d->ui.actionExtract->setEnabled(bank >= 0);

	// Set the BankView's BankEntry to the selected bank.
	// NOTE: Only handles the first selected bank.
	d->ui.bevBankEntryView->setBankEntry(entry);
}
