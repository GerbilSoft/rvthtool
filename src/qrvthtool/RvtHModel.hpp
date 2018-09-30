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
			COL_TYPE,		// Type (HW)
			COL_TITLE,		// Title
			COL_GAMEID,		// Game ID
			COL_DISCNUM,		// Disc #
			COL_REVISION,		// Revision
			COL_REGION,		// Region
			COL_IOS_VERSION,	// IOS version (Wii only)
			
			COL_MAX
		};

		// Icon IDs.
		enum IconID {
			ICON_GCN,	// GameCube (retail)
			ICON_NR,	// NR Reader (debug)
			ICON_WII,	// Wii (retail)
			ICON_RVTR,	// RVT-R Reader (debug)
			ICON_RVTH,	// RVT-H Reader (debug)

			ICON_MAX
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

		/**
		 * Load an icon.
		 * @param id Icon ID.
		 * @return Icon.
		 */
		static QIcon getIcon(IconID id);

		/**
		 * Get the icon ID for the first bank.
		 * Used for the application icon.
		 * @return Icon ID, or ICON_MAX on error.
		 */
		IconID iconIDForBank1(void) const;

	private slots:
		/**
		 * The system theme has changed.
		 */
		void themeChanged_slot(void);
};

#endif /* __RVTHTOOL_QRVTHTOOL_RVTHMODEL_HPP__ */
