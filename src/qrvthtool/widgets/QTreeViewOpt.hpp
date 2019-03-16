/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QTreeViewOpt.hpp: QTreeView with drawing optimizations.                 *
 * Specifically, don't update rows that are offscreen.                     *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
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

#ifndef __RVTHTOOL_QRVTHTOOL_WIDGETS_QTREEVIEWOPT_HPP__
#define __RVTHTOOL_QRVTHTOOL_WIDGETS_QTREEVIEWOPT_HPP__

// Qt includes and classes.
#include <QTreeView>
class QKeyEvent;
class QFocusEvent;

class QTreeViewOpt : public QTreeView
{
	Q_OBJECT
	typedef QTreeView super;

	public:
		explicit QTreeViewOpt(QWidget *parent = nullptr);

	private:
		Q_DISABLE_COPY(QTreeViewOpt);

	public:
		void dataChanged(const QModelIndex &topLeft,
			const QModelIndex &bottomRight,
			const QVector<int> &roles = QVector<int>()) final;

	protected slots:
		void showColumnContextMenu(const QPoint &point);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WIDGETS_QTREEVIEWOPT_HPP__ */
