/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHItemDelegate.hpp: RvtH item delegate for QTreeView.                 *
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

#ifndef __RVTHTOOL_QRVTHTOOL_RVTHITEMDELEGATE_HPP__
#define __RVTHTOOL_QRVTHTOOL_RVTHITEMDELEGATE_HPP__

// Qt includes.
#include <QStyledItemDelegate>

class RvtHItemDelegatePrivate;
class RvtHItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT
	typedef QStyledItemDelegate super;

	public:
		explicit RvtHItemDelegate(QObject *parent);

	public:
		QSize sizeHint(const QStyleOptionViewItem &option,
			       const QModelIndex &index) const final;
};

#endif /* __RVTHTOOL_QRVTHTOOL_RVTHITEMDELEGATE_HPP__ */
