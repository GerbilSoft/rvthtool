/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * SelectDeviceDialog.hpp: Select an RVT-H Reader device.                  *
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

/** SelectDeviceDialogPrivate **/

#include "ui_SelectDeviceDialog.h"
class SelectDeviceDialogPrivate
{
	public:
		explicit SelectDeviceDialogPrivate(SelectDeviceDialog *q);

	protected:
		SelectDeviceDialog *const q_ptr;
		Q_DECLARE_PUBLIC(SelectDeviceDialog)
	private:
		Q_DISABLE_COPY(SelectDeviceDialogPrivate)

	public:
		Ui::SelectDeviceDialog ui;

		// RVT-H Reader icon
		QIcon rvthReaderIcon;

		// Selected device.
		QString sel_deviceName;
		QString sel_serialNumber;
		int64_t sel_hddSize;

		// Device paths and serial numbers.
		QVector<QString> vecDeviceNames;
		QVector<QString> vecSerialNumbers;
		QVector<int64_t> vecHDDSizes;

	private:
		static inline int calc_frac_part(int64_t size, int64_t mask);

		/**
		* Format a block device size.
		* @param size	[in] Block device size.
		* @return Formatted block device size.
		*/
		static QString format_size(int64_t size);

	public:
		// Refresh the device list.
		void refreshDeviceList(void);
};

SelectDeviceDialogPrivate::SelectDeviceDialogPrivate(SelectDeviceDialog *q)
	: q_ptr(q)
{
	// Get the RVT-H Reader icon.
	rvthReaderIcon = RvtHModel::getIcon(RvtHModel::ICON_RVTH);
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
	vecDeviceNames.clear();
	vecSerialNumbers.clear();
	vecHDDSizes.clear();

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
						SelectDeviceDialog::tr("Try rerunning qrvthtool using an elevated command prompt.");
				} else {
					s_err += QLatin1String("\n\n") +
						SelectDeviceDialog::tr("Try rerunning qrvthtool using an Administrator account.");
				}
#else /* _WIN32 */
				s_err += SelectDeviceDialog::tr("Try rerunning qrvthtool as root.");
#endif /* _WIN32 */
			}
			return;
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

		// Device name and serial number.
#ifdef _WIN32
		QString deviceName = QString::fromUtf16(
			reinterpret_cast<const char16_t*>(p->device_name));
		QString serialNumber;
		if (p->usb_serial) {
			serialNumber = QString::fromUtf16(
			reinterpret_cast<const char16_t*>(p->usb_serial));
		}
#else /* !_WIN32 */
		QString deviceName = QString::fromUtf8(p->device_name);
		QString serialNumber;
		if (p->usb_serial) {
			serialNumber = QString::fromUtf8(p->usb_serial);
		}
#endif /* _WIN32 */
		int64_t hddSize = p->size;

		// Create the string.
		QString text = deviceName + QChar(L'\n') +
			serialNumber + QChar(L'\n') +
			format_size(hddSize);

		// Create the QListWidgetItem.
		// TODO: Verify that QListWidget takes ownership.
		// TODO: Switch to QListView and use a model.
		QListWidgetItem *const item = new QListWidgetItem(rvthReaderIcon, text, ui.lstDevices);
		item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

		// Save the device information.
		vecDeviceNames.append(deviceName);
		vecSerialNumbers.append(serialNumber);
		vecHDDSizes.append(hddSize);
	}

	rvth_query_free(devs);
#else /* !HAVE_QUERY */
	// TODO: Better error message.
	// TODO: Fall back to /dev/* scanning?
	ui.lstDevices->setNoItemText(
		SelectDeviceDialog::tr("ERROR: Device querying not supported in this build."));
#endif /* HAVE_QUERY */
}

/** SelectDeviceDialog **/

SelectDeviceDialog::SelectDeviceDialog(QWidget *parent)
	: super(parent)
	, d_ptr(new SelectDeviceDialogPrivate(this))
{
	Q_D(SelectDeviceDialog);
	d->ui.setupUi(this);

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
	btnRefresh->setText(tr("&Refresh"));
	btnRefresh->setIcon(QIcon::fromTheme(QLatin1String("view-refresh")));
	connect(btnRefresh, SIGNAL(clicked()), this, SLOT(refresh()));

	// Set the device list's decoration position.
	// NOTE: Qt automatically interprets this as "Right" if an RTL language is in use.
	d->ui.lstDevices->setDecorationPosition(QStyleOptionViewItem::Left);

	// Refresh the device list.
	d->refreshDeviceList();

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
	return d->sel_deviceName;
}

/**
 * Get the selected RVT-H Reader serial number.
 * @return Serial number, or empty string if not selected.
 */
QString SelectDeviceDialog::serialNumber(void) const
{
	Q_D(const SelectDeviceDialog);
	return d->sel_serialNumber;
}

/**
 * Get the selected RVT-H Reader HDD size.
 * @return HDD size (in bytes), or 0 if not selected.
 */
int64_t SelectDeviceDialog::hddSize(void) const
{
	Q_D(const SelectDeviceDialog);
	return d->sel_hddSize;
}

/** UI widget slots **/

/** QDialog slots **/

void SelectDeviceDialog::accept(void)
{
	// Save the selected device information.
	Q_D(SelectDeviceDialog);
	int row = d->ui.lstDevices->currentRow();
	if (row >= 0 && row < d->vecDeviceNames.size()) {
		// Item selected.
		d->sel_deviceName = d->vecDeviceNames[row];
		d->sel_serialNumber = d->vecSerialNumbers[row];
		d->sel_hddSize = d->vecHDDSizes[row];
	} else {
		// No item selected, or out of range.
		d->sel_deviceName.clear();
		d->sel_serialNumber.clear();
		d->sel_hddSize = 0;
	}

	super::accept();
}

void SelectDeviceDialog::reject(void)
{
	// Cancelled.
	Q_D(SelectDeviceDialog);
	d->sel_deviceName.clear();
	d->sel_serialNumber.clear();
	d->sel_hddSize = 0;
	super::reject();
}

void SelectDeviceDialog::done(int r)
{
	Q_D(SelectDeviceDialog);
	bool clear = true;
	if (r == QDialog::Accepted) {
		// Save the selected device information.
		int row = d->ui.lstDevices->currentRow();
		if (row >= 0 && row < d->vecDeviceNames.size()) {
			// Item selected.
			d->sel_deviceName = d->vecDeviceNames[row];
			d->sel_serialNumber = d->vecSerialNumbers[row];
			d->sel_hddSize = d->vecHDDSizes[row];
			clear = false;
		}
	}

	if (clear) {
		// No item selected, or out of range.
		d->sel_deviceName.clear();
		d->sel_serialNumber.clear();
		d->sel_hddSize = 0;
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
