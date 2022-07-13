/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * AboutDialog.hpp: About Dialog.                                          *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __RVTHTOOL_QRVTHTOOL_WINDOWS_ABOUTDIALOG_HPP__
#define __RVTHTOOL_QRVTHTOOL_WINDOWS_ABOUTDIALOG_HPP__

#include <QDialog>

// Qt classes.
class QWidget;

class AboutDialogPrivate;
class AboutDialog : public QDialog
{
	Q_OBJECT
	typedef QDialog super;
	
	public:
		explicit AboutDialog(QWidget *parent = nullptr);
		virtual ~AboutDialog();

	protected:
		AboutDialogPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(AboutDialog)
	private:
		Q_DISABLE_COPY(AboutDialog)

	protected:
		// State change event. (Used for switching the UI language at runtime.)
		void changeEvent(QEvent *event) final;
};

#endif /* __RVTHTOOL_QRVTHTOOL_WINDOWS_ABOUTDIALOG_HPP__ */
