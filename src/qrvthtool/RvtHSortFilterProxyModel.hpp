/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHSortFilterProxyModel.hpp: RvtHModel sort filter proxy.              *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <QtCore/QSortFilterProxyModel>

class RvtHSortFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
	typedef QSortFilterProxyModel super;

	public:
		explicit RvtHSortFilterProxyModel(QObject *parent = nullptr);

	private:
		Q_DISABLE_COPY(RvtHSortFilterProxyModel)

	public:
		bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const final;
		bool lessThan(const QModelIndex &left, const QModelIndex &right) const final;
};
