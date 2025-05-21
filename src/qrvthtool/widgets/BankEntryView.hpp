/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * BankEntryView.hpp: Bank Entry view widget.                              *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <QWidget>

// NOTE: Qt6's moc doesn't like incomplete types.
#include "librvth/rvth.hpp"

class BankEntryViewPrivate;
class BankEntryView : public QWidget
{
	Q_OBJECT
	typedef QWidget super;

	Q_PROPERTY(const RvtH_BankEntry* bankEntry READ bankEntry WRITE setBankEntry)

	public:
		explicit BankEntryView(QWidget *parent = nullptr);
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
		const RvtH_BankEntry *bankEntry(void) const;

		/**
		 * Set the RvtH_BankEntry being displayed.
		 * @param bankEntry RvtH_BankEntry.
		 */
		void setBankEntry(const RvtH_BankEntry *bankEntry);

		/**
		 * Update the currently displayed RvtH_BankEntry.
		 */
		void update(void);

	protected:
		// State change event. (Used for switching the UI language at runtime.)
		void changeEvent(QEvent *event) final;
};
