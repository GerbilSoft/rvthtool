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
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_SELECTDEVICEDIALOG_HPP__ */
