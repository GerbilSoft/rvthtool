/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * BankEntryView.hpp: Bank Entry view widget.                              *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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

#ifndef __RVTHTOOL_QRVTHTOOL_WIDGETS_BANKENTRYVIEW_HPP__
#define __RVTHTOOL_QRVTHTOOL_WIDGETS_BANKENTRYVIEW_HPP__

#include <QWidget>

struct _RvtH_BankEntry;

class BankEntryViewPrivate;
class BankEntryView : public QWidget
{
	Q_OBJECT
	typedef QWidget super;

	Q_PROPERTY(const struct _RvtH_BankEntry* bankEntry READ bankEntry WRITE setBankEntry)

	public:
		explicit BankEntryView(QWidget *parent = 0);
		virtual ~BankEntryView();

	protected:
		BankEntryViewPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(BankEntryView)
	private:
		Q_DISABLE_COPY(BankEntryView)

	public:
		/**
		 * Get the RvtH_BankEntry being displayed.
		 * @return RvtH_BankEntry.
		 */
		const struct _RvtH_BankEntry *bankEntry(void) const;

		/**
		 * Set the RvtH_BankEntry being displayed.
		 * @param bankEntry RvtH_BankEntry.
		 */
		void setBankEntry(const struct _RvtH_BankEntry *bankEntry);

	protected:
		// State change event. (Used for switching the UI language at runtime.)
		void changeEvent(QEvent *event);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WIDGETS_BANKENTRYVIEW_HPP__ */
