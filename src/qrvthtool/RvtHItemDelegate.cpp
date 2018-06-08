/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHItemDelegate.cpp: RvtH item delegate for QTreeView.                 *
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

#include "RvtHItemDelegate.hpp"
#include "RvtHModel.hpp"

RvtHItemDelegate::RvtHItemDelegate(QObject *parent)
	: super(parent)
{ }

// TODO: Should dual-layer rows be top-aligned or center-aligned?

QSize RvtHItemDelegate::sizeHint(const QStyleOptionViewItem &option,
				 const QModelIndex &index) const
{
	if (!index.isValid()) {
		// Index is invalid.
		// Use the default sizeHint().
		return super::sizeHint(option, index);
	}

	QSize size = super::sizeHint(option, index);
	if (index.data(RvtHModel::DualLayerRole).toBool()) {
		size.setHeight(size.height() * 2);
	}
	return size;
}
