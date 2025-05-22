/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHModel.hpp: QAbstractListModel for RvtH objects.                     *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

class RvtH;

// Qt includes.
#include <QtCore/QAbstractListModel>

class RvtHModelPrivate;
class RvtHModel : public QAbstractListModel
{
Q_OBJECT
typedef QAbstractListModel super;
	
public:
	explicit RvtHModel(QObject *parent = nullptr);
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

	// Icon IDs
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
	 * @param rvth RVT-H Reader disk image
	 */
	void setRvtH(RvtH *rvth);

	/**
	 * Load an icon.
	 * @param id Icon ID
	 * @return Icon
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

public slots:
	/**
	 * Force the RVT-H model to update a bank.
	 * @param bank Bank number
	 */
	void forceBankUpdate(unsigned int bank);
};
