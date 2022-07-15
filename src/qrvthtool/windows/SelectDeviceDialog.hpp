/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * SelectDeviceDialog.hpp: Select an RVT-H Reader device.                  *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_QRVTHTOOL_WINDOWS_SELECTDEVICEDIALOG_HPP__
#define __RVTHTOOL_QRVTHTOOL_WINDOWS_SELECTDEVICEDIALOG_HPP__

#include <QDialog>
#include <QItemSelection>

#include "librvth/query.h"
Q_DECLARE_METATYPE(RvtH_Listen_State_e)

/**
 * Convert a TCHAR string to QString.
 * @param str TCHAR string
 * @return QString
 */
static inline QString qsFromTCHAR(const TCHAR *str)
{
	if (!str || str[0] == _T('\0'))
		return QString();

#ifdef _WIN32
	return QString::fromUtf16(reinterpret_cast<const char16_t*>(str));
#else /* !_WIN32 */
	return QString::fromUtf8(str);
#endif /* _WIN32 */
}

class DeviceQueryData {
	public:
		QString device_name;	// Device name, e.g. "/dev/sdc" or "\\.\PhysicalDrive3".

		QString usb_vendor;	// USB vendor name.
		QString usb_product;	// USB product name.
		QString usb_serial;	// USB serial number, in ASCII.

		QString hdd_vendor;	// HDD vendor.
		QString hdd_model;	// HDD model number.
		QString hdd_fwver;	// HDD firmware version.

#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
		QString hdd_serial;	// HDD serial number, in ASCII.
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */

		uint64_t size;		// HDD size, in bytes.

	public:
		DeviceQueryData() : size(0) { }

		DeviceQueryData(const RvtH_QueryEntry *entry)
			: device_name(qsFromTCHAR(entry->device_name))
			, usb_vendor(qsFromTCHAR(entry->usb_vendor))
			, usb_product(qsFromTCHAR(entry->usb_product))
			, usb_serial(qsFromTCHAR(entry->usb_serial))
			, hdd_vendor(qsFromTCHAR(entry->hdd_vendor))
			, hdd_model(qsFromTCHAR(entry->hdd_model))
			, hdd_fwver(qsFromTCHAR(entry->hdd_fwver))
#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
			, hdd_serial(qsFromTCHAR(entry->hdd_serial))
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */
			, size(entry->size)
		{ }
};
Q_DECLARE_METATYPE(DeviceQueryData)

class SelectDeviceDialogPrivate;
class SelectDeviceDialog : public QDialog
{
	Q_OBJECT
	typedef QDialog super;
	
	public:
		explicit SelectDeviceDialog(QWidget *parent = nullptr);
		virtual ~SelectDeviceDialog();

	protected:
		SelectDeviceDialogPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(SelectDeviceDialog)
	private:
		Q_DISABLE_COPY(SelectDeviceDialog)

	protected:
		// State change event. (Used for switching the UI language at runtime.)
		void changeEvent(QEvent *event) final;

#ifdef _WIN32
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
		typedef qintptr native_event_result_t;
#else /* QT_VERSION < QT_VERSION_CHECK(6,0,0) */
		typedef long native_event_result_t;
#endif /* QT_VERSION >= QT_VERSION_CHECK(6,0,0) */

		/**
		 * Native event
		 * @param eventType
		 * @param message
		 * @param result
		 * @return
		 */
		bool nativeEvent(const QByteArray &eventType, void *message,
			native_event_result_t *result) final;
#endif /* _WIN32 */

	public:
		/** Properties **/

		/**
		 * Get the selected RVT-H Reader device name.
		 * @return Device name, or empty string if not selected.
		 */
		QString deviceName(void) const;

		/**
		 * Get the selected RVT-H Reader serial number.
		 * @return Serial number, or empty string if not selected.
		 */
		QString serialNumber(void) const;

		/**
		 * Get the selected RVT-H Reader HDD size.
		 * @return HDD size (in bytes), or 0 if not selected.
		 */
		int64_t hddSize(void) const;

	protected slots:
		/** UI widget slots **/

		// QDialog slots
		void accept(void) final;
		void reject(void) final;
		void done(int r) final;
		void refresh(void);

		// lstDevices slots
		void lstDevices_selectionModel_selectionChanged(
			const QItemSelection& selected, const QItemSelection& deselected);
		void on_lstDevices_doubleClicked(const QModelIndex &index);

		/**
		 * A device state has changed.
		 * @param queryData Device query data
		 * @param state Device state
		 */
		void deviceStateChanged(const DeviceQueryData &queryData, RvtH_Listen_State_e state);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_SELECTDEVICEDIALOG_HPP__ */
