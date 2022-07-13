/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * SelectDeviceDialog.hpp: Select an RVT-H Reader device.                  *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "SelectDeviceDialog.hpp"

// librvth
#include "librvth/config.librvth.h"
#include "librvth/query.h"

// for the RVT-H Reader icon
#include "../RvtHModel.hpp"

#ifdef _WIN32
# include <windows.h>
#endif

// Qt includes.
#include <QtCore/QLocale>
#include <QPushButton>
#include <QMetaMethod>

/** SelectDeviceDialogPrivate **/

#include "ui_SelectDeviceDialog.h"
class SelectDeviceDialogPrivate
{
	public:
		explicit SelectDeviceDialogPrivate(SelectDeviceDialog *q);
		~SelectDeviceDialogPrivate();

	protected:
		SelectDeviceDialog *const q_ptr;
		Q_DECLARE_PUBLIC(SelectDeviceDialog)
	private:
		Q_DISABLE_COPY(SelectDeviceDialogPrivate)

	public:
		Ui::SelectDeviceDialog ui;

		// RVT-H Reader icon
		QIcon rvthReaderIcon;

		// Selected device
		DeviceQueryData *sel_device;

		// Device information
		QList<DeviceQueryData> lstQueryData;

		// Device listener
		RvtH_ListenForDevices *listener;

	private:
		static inline int calc_frac_part(int64_t size, int64_t mask);

		/**
		* Format a block device size.
		* @param size	[in] Block device size.
		* @return Formatted block device size.
		*/
		static QString format_size(int64_t size);

		/**
		 * Add a device to the list.
		 * @param queryData DeviceQueryData
		 */
		void addDevice(DeviceQueryData &&queryData);

	public:
		// Refresh the device list.
		void refreshDeviceList(void);

		/**
		 * RvtH device listener callback.
		 * @param listener Listener
		 * @param entry Device that was added/removed (if removed, only contains device name)
		 * @param state Device state (see RvtH_Listen_State_e)
		 * @param userdata User data specified on initialization
		 *
		 * NOTE: query's data is not guaranteed to remain valid once the
		 * callback function returns. Copy everything out immediately!
		 *
		 * NOTE: On RVTH_LISTEN_DISCONNECTED, only query->device_name is set.
		 * udev doesn't seem to let us get the correct USB parent device, so
		 * the callback function will need to verify that query->device_name
		 * is a previously-received RVT-H Reader device.
		 */
		static void rvth_listener_callback(RvtH_ListenForDevices *listener,
			const RvtH_QueryEntry *entry, RvtH_Listen_State_e state, void *userdata);
};

SelectDeviceDialogPrivate::SelectDeviceDialogPrivate(SelectDeviceDialog *q)
	: q_ptr(q)
	, sel_device(nullptr)
	, listener(nullptr)
{
	// Get the RVT-H Reader icon.
	rvthReaderIcon = RvtHModel::getIcon(RvtHModel::ICON_RVTH);
}

SelectDeviceDialogPrivate::~SelectDeviceDialogPrivate()
{
	if (listener) {
		rvth_listener_stop(listener);
	}
}

inline int SelectDeviceDialogPrivate::calc_frac_part(int64_t size, int64_t mask)
{
	float f = (float)(size & (mask - 1)) / (float)mask;
	int frac_part = (int)(f * 1000.0f);

	// MSVC added round() and roundf() in MSVC 2013.
	// Use our own rounding code instead.
	int round_adj = (frac_part % 10 > 5);
	frac_part /= 10;
	frac_part += round_adj;
	return frac_part;
}

/**
 * Format a block device size.
 * @param size	[in] Block device size.
 * @return Formatted block device size.
 */
QString SelectDeviceDialogPrivate::format_size(int64_t size)
{
	QString sbuf;
	sbuf.reserve(16);

	// frac_part is always 0 to 100.
	// If whole_part >= 10, frac_part is divided by 10.
	int whole_part, frac_part;

	// TODO: Optimize this?
	const QLocale sysLocale = QLocale::system();
	QString suffix;
	if (size < 0) {
		// Invalid size. Print the value as-is.
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 10)) {
		// tr: Bytes (< 1,024)
		suffix = SelectDeviceDialog::tr("byte(s)", nullptr, size);
		whole_part = (int)size;
		frac_part = 0;
	} else if (size < (2LL << 20)) {
		// tr: Kilobytes
		suffix = SelectDeviceDialog::tr("KiB");
		whole_part = (int)(size >> 10);
		frac_part = calc_frac_part(size, (1LL << 10));
	} else if (size < (2LL << 30)) {
		// tr: Megabytes
		suffix = SelectDeviceDialog::tr("MiB");
		whole_part = (int)(size >> 20);
		frac_part = calc_frac_part(size, (1LL << 20));
	} else if (size < (2LL << 40)) {
		// tr: Gigabytes
		suffix = SelectDeviceDialog::tr("GiB");
		whole_part = (int)(size >> 30);
		frac_part = calc_frac_part(size, (1LL << 30));
	} else if (size < (2LL << 50)) {
		// tr: Terabytes
		suffix = SelectDeviceDialog::tr("TiB");
		whole_part = (int)(size >> 40);
		frac_part = calc_frac_part(size, (1LL << 40));
	} else if (size < (2LL << 60)) {
		// tr: Petabytes
		suffix = SelectDeviceDialog::tr("PiB");
		whole_part = (int)(size >> 50);
		frac_part = calc_frac_part(size, (1LL << 50));
	} else /*if (size < (2ULL << 70))*/ {
		// tr: Exabytes
		suffix = SelectDeviceDialog::tr("EiB");
		whole_part = (int)(size >> 60);
		frac_part = calc_frac_part(size, (1LL << 60));
	}

	sbuf = sysLocale.toString(whole_part);
	if (size >= (2LL << 10)) {
		// KiB or larger. There is a fractional part.
		int frac_digits = 2;
		if (whole_part >= 10) {
			int round_adj = (frac_part % 10 > 5);
			frac_part /= 10;
			frac_part += round_adj;
			frac_digits = 1;
		}

		char fdigit[12];
		snprintf(fdigit, sizeof(fdigit), "%0*d", frac_digits, frac_part);
		sbuf += sysLocale.decimalPoint();
		sbuf += QLatin1String(fdigit);
	}

	if (!suffix.isEmpty()) {
		sbuf += QChar(L' ');
		sbuf += suffix;
	}

	return sbuf;
}

/**
 * Add a device to the list.
 * @param queryData DeviceQueryData
 */
void SelectDeviceDialogPrivate::addDevice(DeviceQueryData &&queryData)
{
	// Create the string.
	QString text = queryData.device_name + QChar(L'\n') +
		queryData.usb_serial + QChar(L'\n') +
		format_size(queryData.size);

	// Create the QListWidgetItem.
	// TODO: Verify that QListWidget takes ownership.
	// TODO: Switch to QListView and use a model.
	QListWidgetItem *const item = new QListWidgetItem(rvthReaderIcon, text, ui.lstDevices);
	item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	// Save the device query data.
	lstQueryData.append(queryData);
}

/**
 * Refresh the device list.
 */
void SelectDeviceDialogPrivate::refreshDeviceList(void)
{
	// TODO: Switch from QListWidget to QListView and use a model?

	// OK button is disabled when the list is cleared,
	// and will be re-enabled if a device is selected.
	ui.lstDevices->clear();
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	// TODO: Automatically update on device hotplug?
	lstQueryData.clear();

#ifdef HAVE_QUERY
	int err = 0;
	RvtH_QueryEntry *devs = rvth_query_devices(&err);
	if (!devs) {
		// No devices found.

		// If err is non-zero, this may be a permissions issue.
		QString s_err;
		if (err != 0) {
			// TODO: Enable rich text for the message and bold the first line?
			s_err = QLatin1String("*** ") +
				SelectDeviceDialog::tr("ERROR enumerating RVT-H Reader devices:") +
				QLatin1String(" ***\n");
			// TODO: Translate strerror().
			s_err += QString::fromUtf8(strerror(err));
			if (err == EACCES) {
#ifdef _WIN32
				OSVERSIONINFO osvi;
				osvi.dwOSVersionInfoSize = sizeof(osvi);
				if (!GetVersionEx(&osvi)) {
					// GetVersionEx() failed.
					// Assume it's an old version of Windows.
					osvi.dwMajorVersion = 0;
				}
				if (osvi.dwMajorVersion >= 6) {
					s_err += QLatin1String("\n\n") +
						SelectDeviceDialog::tr("Try rerunning qrvthtool as Administrator.");
				} else {
					s_err += QLatin1String("\n\n") +
						SelectDeviceDialog::tr("Try rerunning qrvthtool using an Administrator account.");
				}
#else /* _WIN32 */
				s_err += SelectDeviceDialog::tr("Try rerunning qrvthtool as root.");
#endif /* _WIN32 */
			}
		} else {
			// Not a permissions issue.
			// We simply didn't find any RVT-H Reader devices.
			s_err = SelectDeviceDialog::tr("No RVT-H Reader devices found.");
		}

		ui.lstDevices->setNoItemText(s_err);
		return;
	}

	// Found devices.
	const RvtH_QueryEntry *p = devs;
	for (; p != NULL; p = p->next) {
		if (!p->device_name) {
			// No device name. Skip it.
			continue;
		}

		// Add this device.
		addDevice(DeviceQueryData(p));
	}

	rvth_query_free(devs);
#else /* !HAVE_QUERY */
	// TODO: Better error message.
	// TODO: Fall back to /dev/* scanning?
	ui.lstDevices->setNoItemText(
		SelectDeviceDialog::tr("ERROR: Device querying not supported in this build."));
#endif /* HAVE_QUERY */
}

void SelectDeviceDialogPrivate::rvth_listener_callback(
	RvtH_ListenForDevices *listener,
	const RvtH_QueryEntry *entry,
	RvtH_Listen_State_e state,
	void *userdata)
{
	Q_UNUSED(listener)

	// NOTE: entry will be deleted once this callback returns.
	// We'll need to copy the relevant data into a custom QObject.

	// NOTE: This is running in a different thread.
	// Need to use a QueuedConnection to trigger the Qt slot.
	// NOTE: Due to Qt weirdness, the slot uses a const ref
	// instead of a const pointer.
	QMetaObject::invokeMethod(static_cast<SelectDeviceDialog*>(userdata),
		"deviceStateChanged",
		Qt::QueuedConnection,
		Q_ARG(DeviceQueryData, DeviceQueryData(entry)),
		Q_ARG(RvtH_Listen_State_e, state));
}

/** SelectDeviceDialog **/

SelectDeviceDialog::SelectDeviceDialog(QWidget *parent)
	: super(parent,
		Qt::WindowSystemMenuHint |
		Qt::WindowTitleHint |
		Qt::WindowCloseButtonHint)
	, d_ptr(new SelectDeviceDialogPrivate(this))
{
	Q_D(SelectDeviceDialog);
	d->ui.setupUi(this);

	qRegisterMetaType<DeviceQueryData>();
	qRegisterMetaType<RvtH_Listen_State_e>();

	// Do NOT delete the window on close.
	// The selected device needs to be accessible by the caller.
	this->setAttribute(Qt::WA_DeleteOnClose, false);

#ifdef Q_OS_MAC
	// Remove the window icon. (Mac "proxy icon")
	// TODO: Use the RVT-H Reader device?
	this->setWindowIcon(QIcon());
#endif /* Q_OS_MAC */

	// TODO: Add device hotplug detection.

	// Change the "Reset" button to "Refresh".
	QPushButton *const btnRefresh = d->ui.buttonBox->button(QDialogButtonBox::Reset);
	btnRefresh->setObjectName(QLatin1String("btnRefresh"));
	btnRefresh->setText(tr("&Refresh"));
	btnRefresh->setIcon(QIcon::fromTheme(QLatin1String("view-refresh")));
	connect(btnRefresh, SIGNAL(clicked()), this, SLOT(refresh()));

	// Set the device list's decoration position.
	// NOTE: Qt automatically interprets this as "Right" if an RTL language is in use.
	d->ui.lstDevices->setDecorationPosition(QStyleOptionViewItem::Left);

	// Refresh the device list.
	d->refreshDeviceList();

	// Attempt to create a device listener.
	// TODO: Callbcak
	d->listener = rvth_listen_for_devices(d->rvth_listener_callback, this);
	if (d->listener) {
		// Device listener was created.
		// Hide the "Refresh" button.
		btnRefresh->hide();
	}

	// Connect the lstDevices selection signal.
	connect(d->ui.lstDevices->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &SelectDeviceDialog::lstDevices_selectionModel_selectionChanged);
}

SelectDeviceDialog::~SelectDeviceDialog()
{
	delete d_ptr;
}

/**
 * Widget state has changed.
 * @param event State change event.
 */
void SelectDeviceDialog::changeEvent(QEvent *event)
{
	Q_D(SelectDeviceDialog);

	switch (event->type()) {
		case QEvent::LanguageChange:
			// Retranslate the UI.
			d->ui.retranslateUi(this);
			break;

		default:
			break;
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}

/** Properties **/

/**
 * Get the selected RVT-H Reader device name.
 * @return Device name, or empty string if not selected.
 */
QString SelectDeviceDialog::deviceName(void) const
{
	Q_D(const SelectDeviceDialog);
	return (d->sel_device ? d->sel_device->device_name : QString());
}

/**
 * Get the selected RVT-H Reader serial number.
 * @return Serial number, or empty string if not selected.
 */
QString SelectDeviceDialog::serialNumber(void) const
{
	Q_D(const SelectDeviceDialog);
	return (d->sel_device ? d->sel_device->usb_serial : QString());
}

/**
 * Get the selected RVT-H Reader HDD size.
 * @return HDD size (in bytes), or 0 if not selected.
 */
int64_t SelectDeviceDialog::hddSize(void) const
{
	Q_D(const SelectDeviceDialog);
	return (d->sel_device ? d->sel_device->size : 0);
}

/** UI widget slots **/

/** QDialog slots **/

void SelectDeviceDialog::accept(void)
{
	// Save the selected device information.
	Q_D(SelectDeviceDialog);
	int row = d->ui.lstDevices->currentRow();
	if (row >= 0 && row < d->lstQueryData.size()) {
		// Item selected.
		d->sel_device = &d->lstQueryData[row];
	} else {
		// No item selected, or out of range.
		d->sel_device = nullptr;
	}

	super::accept();
}

void SelectDeviceDialog::reject(void)
{
	// Cancelled.
	Q_D(SelectDeviceDialog);
	d->sel_device = nullptr;
	super::reject();
}

void SelectDeviceDialog::done(int r)
{
	Q_D(SelectDeviceDialog);
	bool clear = true;
	if (r == QDialog::Accepted) {
		// Save the selected device information.
		int row = d->ui.lstDevices->currentRow();
		if (row >= 0 && row < d->lstQueryData.size()) {
			// Item selected.
			d->sel_device = &d->lstQueryData[row];
			clear = false;
		}
	}

	if (clear) {
		// No item selected, or out of range.
		d->sel_device = nullptr;
	}

	super::done(r);
}

void SelectDeviceDialog::refresh(void)
{
	Q_D(SelectDeviceDialog);
	d->refreshDeviceList();
}

/** lstDevices slots **/

void SelectDeviceDialog::lstDevices_selectionModel_selectionChanged(
	const QItemSelection& selected, const QItemSelection& deselected)
{
	Q_UNUSED(deselected)

	// Enable the "OK" button if exactly one item is selected.
	Q_D(SelectDeviceDialog);
	d->ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
		selected.indexes().size() == 1);
}

void SelectDeviceDialog::on_lstDevices_doubleClicked(const QModelIndex &index)
{
	Q_UNUSED(index)

	// Act like the "OK" button was clicked.
	this->accept();
}

/**
 * A device state has changed.
 * @param queryData Device query data
 * @param state Device state
 */
void SelectDeviceDialog::deviceStateChanged(const DeviceQueryData &queryData, RvtH_Listen_State_e state)
{
	printf("device state changed: %s -> %d\n", queryData.device_name.toUtf8().constData(), state);
}
