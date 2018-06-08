/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * RvtHModel.hpp: QAbstractListModel for RvtH objects.                     *
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

#ifndef __RVTHTOOL_QRVTHTOOL_RVTHMODEL_HPP__
#define __RVTHTOOL_QRVTHTOOL_RVTHMODEL_HPP__

struct _RvtH;

// Qt includes.
#include <QtCore/QAbstractListModel>

class RvtHModelPrivate;
class RvtHModel : public QAbstractListModel
{
	Q_OBJECT
	typedef QAbstractListModel super;
	
	public:
		explicit RvtHModel(QObject *parent = 0);
		virtual ~RvtHModel();

	protected:
		RvtHModelPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(RvtHModel)
	private:
		Q_DISABLE_COPY(RvtHModel)

	public:
		enum Column {
			COL_BANKNUM,		// Bank #
			COL_ICON,		// Icon (HW)
			COL_TITLE,		// Title
			COL_GAMEID,		// Game ID
			COL_DISCNUM,		// Disc #
			COL_REVISION,		// Revision
			COL_REGION,		// Region
			COL_IOS_VERSION,	// IOS version (Wii only)
			COL_ENCRYPTION,		// Encryption type
			COL_SIG_TICKET,		// Ticket signature
			COL_SIG_TMD,		// TMD signature
			COL_APPLOADER,		// AppLoader error
			
			COL_MAX
		};

		// Dual-layer role.
		// If true, this is a Wii DL image and should be
		// represented as taking up two banks.
		static const int DualLayerRole = Qt::UserRole;

		// Qt Model/View interface.
		int rowCount(const QModelIndex& parent = QModelIndex()) const final;
		int columnCount(const QModelIndex& parent = QModelIndex()) const final;

		QVariant data(const QModelIndex& index, int role) const final;
		QVariant headerData(int section, Qt::Orientation orientation, int role) const final;

		/**
		 * Set the RVT-H Reader disk image to use in this model.
		 * @param rvth RVT-H Reader disk image.
		 */
		void setRvtH(struct _RvtH *rvth);

	private slots:
		/**
		 * The system theme has changed.
		 */
		void themeChanged_slot(void);
};

#endif /* __RVTHTOOL_QRVTHTOOL_RVTHMODEL_HPP__ */
