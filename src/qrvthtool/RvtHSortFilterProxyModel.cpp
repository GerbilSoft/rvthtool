/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHSortFilterProxyModel.hpp: RvtHModel sort filter proxy.              *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
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

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
	if (vLeft.typeId() == QMetaType::QString &&
	    vRight.typeId() == QMetaType::QString)
#else /* QT_VERSION < QT_VERSION_CHECK(6,0,0) */
	if (vLeft.type() == QVariant::String &&
	    vRight.type() == QVariant::String)
#endif /* QT_VERSION >= QT_VERSION_CHECK(6,0,0) */
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
