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

#ifndef __RVTHTOOL_QRVTHTOOL_RVTHSORTFILTERPROXYMODEL_HPP__
#define __RVTHTOOL_QRVTHTOOL_RVTHSORTFILTERPROXYMODEL_HPP__

#include <QtCore/QSortFilterProxyModel>

class RvtHSortFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
	typedef QSortFilterProxyModel super;

	public:
		explicit RvtHSortFilterProxyModel(QObject *parent = 0);

	private:
		Q_DISABLE_COPY(RvtHSortFilterProxyModel)

	public:
		bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const final;
		bool lessThan(const QModelIndex &left, const QModelIndex &right) const final;
};

#endif /* __RVTHTOOL_QRVTHTOOL_RVTHSORTFILTERPROXYMODEL_HPP__ */
