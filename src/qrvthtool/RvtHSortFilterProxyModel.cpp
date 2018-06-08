/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHSortFilterProxyModel.hpp: RvtHModel sort filter proxy.              *
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

#include "RvtHSortFilterProxyModel.hpp"

RvtHSortFilterProxyModel::RvtHSortFilterProxyModel(QObject *parent)
	: super(parent)
{ }

bool RvtHSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	// If the bank number column is empty, don't show this row.
	QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
	if (!index.data().isValid()) {
		// Don't show this row.
		return false;
	}

	return super::filterAcceptsRow(source_row, source_parent);
}

bool RvtHSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
	if (!left.isValid() || !right.isValid()) {
		// One or both indexes are invalid.
		// Use the default lessThan().
		return super::lessThan(left, right);
	}

	const QVariant vLeft = left.data();
	const QVariant vRight = right.data();

	if (vLeft.type() == QVariant::String &&
	    vRight.type() == QVariant::String)
	{
		// String. Do a case-insensitive comparison.
		QString sLeft = vLeft.toString();
		QString sRight = vRight.toString();
		return (sLeft.compare(sRight, Qt::CaseInsensitive) < 0);
	}

	// Unhandled type.
	// Use the default lessThan().
	return super::lessThan(left, right);
}
