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
		explicit SelectDeviceDialog(QWidget *parent = 0);
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

		// lstDevices slots
		void lstDevices_selectionModel_selectionChanged(
			const QItemSelection& selected, const QItemSelection& deselected);
		void on_lstDevices_doubleClicked(const QModelIndex &index);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_SELECTDEVICEDIALOG_HPP__ */
