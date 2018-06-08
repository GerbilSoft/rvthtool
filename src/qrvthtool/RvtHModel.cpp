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

#include "RvtHModel.hpp"

#include "librvth/rvth.h"

// Qt includes.
#include <QApplication>
#include <QtGui/QBrush>
#include <QtGui/QPalette>

/** RvtHModelPrivate **/

class RvtHModelPrivate
{
	public:
		explicit RvtHModelPrivate(RvtHModel *q);

	protected:
		RvtHModel *const q_ptr;
		Q_DECLARE_PUBLIC(RvtHModel)
	private:
		Q_DISABLE_COPY(RvtHModelPrivate)

	public:
		RvtH *rvth;

		// Style variables.
		struct style_t {
			/**
			 * Initialize the style variables.
			 */
			void init(void);

			// Background colors for "deleted" banks.
			QBrush brush_lostFile;
			QBrush brush_lostFile_alt;

			// Pixmaps for COL_ISVALID.
			static const QSize szPxmIsValid;
			QPixmap pxmIsValid_unknown;
			QPixmap pxmIsValid_invalid;
			QPixmap pxmIsValid_good;
		};
		style_t style;

		// Row insert start/end indexes.
		int insertStart;
		int insertEnd;
};

const QSize RvtHModelPrivate::style_t::szPxmIsValid = QSize(16, 16);

RvtHModelPrivate::RvtHModelPrivate(RvtHModel *q)
	: q_ptr(q)
	, rvth(nullptr)
	, insertStart(-1)
	, insertEnd(-1)
{
	// Initialize the style variables.
	style.init();
}

/**
 * Initialize the style variables.
 */
void RvtHModelPrivate::style_t::init(void)
{
	// TODO: Call this function if the UI style changes.

	// Initialize the background colors for "lost" files.
	QPalette pal = QApplication::palette("QTreeView");
	QColor bgColor_lostFile = pal.base().color();
	QColor bgColor_lostFile_alt = pal.alternateBase().color();

	// Adjust the colors to have a yellow hue.
	int h, s, v;

	// "Lost" file. (Main)
	bgColor_lostFile.getHsv(&h, &s, &v, nullptr);
	h = 60;
	s = (255 - s);
	bgColor_lostFile.setHsv(h, s, v);

	// "Lost" file. (Alternate)
	bgColor_lostFile_alt.getHsv(&h, &s, &v, nullptr);
	h = 60;
	s = (255 - s);
	bgColor_lostFile_alt.setHsv(h, s, v);

	// Save the background colors in QBrush objects.
	brush_lostFile = QBrush(bgColor_lostFile);
	brush_lostFile_alt = QBrush(bgColor_lostFile_alt);

	// TODO: Pixmaps.
#if 0
	// Initialize the COL_ISVALID pixmaps.
	// NOTE: Using Oxygen icons for all systems.
	// TODO: Better dialog-question icon.
	// TODO: Hi-DPI icon handling.
	static const QSize sz(16, 16);
	const QString s_sz = QString::number(sz.width()) +
		QChar(L'x') + QString::number(sz.height());
	pxmIsValid_unknown = QPixmap(QLatin1String(":/oxygen/") + s_sz +
		QLatin1String("/dialog-question.png"));
	pxmIsValid_invalid = QPixmap(QLatin1String(":/oxygen/") + s_sz +
		QLatin1String("/dialog-error.png"));
	pxmIsValid_good    = QPixmap(QLatin1String(":/oxygen/") + s_sz +
		QLatin1String("/dialog-ok-apply.png"));
#endif
}

/** RvtHModel **/

RvtHModel::RvtHModel(QObject *parent)
	: super(parent)
	, d_ptr(new RvtHModelPrivate(this))
{
	// TODO: Not implemented yet...
#if 0
	// Connect the "themeChanged" signal.
	connect(qApp, SIGNAL(themeChanged()),
		this, SLOT(themeChanged_slot()));
#endif
}

RvtHModel::~RvtHModel()
{
	Q_D(RvtHModel);
	delete d;
}


int RvtHModel::rowCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	Q_D(const RvtHModel);
	if (d->rvth) {
		return rvth_get_BankCount(d->rvth);
	}
	return 0;
}

int RvtHModel::columnCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	Q_D(const RvtHModel);
	if (d->rvth) {
		return COL_MAX;
	}
	return 0;
}

QVariant RvtHModel::data(const QModelIndex& index, int role) const
{
	Q_D(const RvtHModel);
	if (!d->rvth || !index.isValid())
		return QVariant();
	if (index.row() >= rowCount())
		return QVariant();

	// Get the bank entry.
	const RvtH_BankEntry *const entry = rvth_get_BankEntry(d->rvth, index.row(), nullptr);
	if (!entry) {
		// No entry...
		return QVariant();
	}

	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty slot.
			// TODO: Indicate that this is empty.
			if (role == Qt::DisplayRole &&
			    index.column() == COL_BANKNUM)
			{
				// Print the bank number.
				return QString::number(index.row() + 1);
			}
			return QVariant();
		case RVTH_BankType_Wii_DL_Bank2:
			// TODO: Make the previous bank double-tall.
			return QVariant();
		default:
			break;
	}

	// TODO: Move some of this to RvtHItemDelegate?
	switch (role) {
		case Qt::DisplayRole:
			// TODO: Cache these?
			switch (index.column()) {
				case COL_BANKNUM:
					return QString::number(index.row() + 1);
				case COL_TITLE:
					// TODO: Convert from Japanese if necessary.
					// Also cp1252.
					return QString::fromLatin1(entry->discHeader.game_title, sizeof(entry->discHeader.game_title)).trimmed();
				case COL_GAMEID:
					return QLatin1String(entry->discHeader.id6, sizeof(entry->discHeader.id6));
				case COL_DISCNUM:
					return QString::number(entry->discHeader.disc_number);
				case COL_REVISION:
					// TODO: BCD?
					return QString::number(entry->discHeader.revision);
				case COL_REGION:
					// TODO: Parse the number.
					return QString::number(entry->region_code);
				case COL_IOS:
					// TODO: Not on GCN.
					return QString::number(entry->ios_version);

				case COL_ENCRYPTION:
				case COL_SIG_TICKET:
				case COL_SIG_TMD:
				case COL_APPLOADER:
					// TODO
					break;

				default:
					break;
			}
			break;

		case Qt::DecorationRole:
			// TODO
			break;

		case Qt::TextAlignmentRole:
			switch (index.column()) {
				case COL_BANKNUM:
				case COL_ICON:
				case COL_TITLE:
					// Left-align, center vertically.
					return (int)(Qt::AlignLeft | Qt::AlignVCenter);

				case COL_GAMEID:
				case COL_DISCNUM:
				case COL_REVISION:
				case COL_REGION:
				case COL_IOS:
				case COL_ENCRYPTION:
				case COL_SIG_TICKET:
				case COL_SIG_TMD:
				case COL_APPLOADER:
				default:
					// Center-align the text.
					return Qt::AlignCenter;
			}

		case Qt::FontRole:
			// TODO: Monospaced columns.
			break;

		case Qt::BackgroundRole:
			// "Deleted" banks should be displayed using a different color.
			if (entry->is_deleted) {
				// TODO: Check if the item view is using alternating row colors before using them.
				if (index.row() & 1)
					return d->style.brush_lostFile_alt;
				else
					return d->style.brush_lostFile;
			}
			break;

		case Qt::SizeHintRole:
			// TODO
			break;

		default:
			break;
	}

	// Default value.
	return QVariant();
}

QVariant RvtHModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(orientation);

	switch (role) {
		case Qt::DisplayRole:
			switch (section) {
				case COL_ICON:		return tr("Icon");
				case COL_TITLE:	return tr("Title");
				//: 6-digit game ID, e.g. GALE01.
				case COL_GAMEID:	return tr("Game ID");
				case COL_DISCNUM:	return tr("Disc #");
				case COL_REVISION:	return tr("Revision");
				case COL_REGION:	return tr("Region");
				case COL_IOS:		return tr("IOS");
				case COL_ENCRYPTION:	return tr("Encryption");
				case COL_SIG_TICKET:	return tr("Ticket Sig");
				case COL_SIG_TMD:	return tr("TMD Sig");
				case COL_APPLOADER:	return tr("AppLoader");

				default:
					break;
			}
			break;

		case Qt::TextAlignmentRole:
			switch (section) {
				case COL_BANKNUM:
					// Left-align the text.
					return Qt::AlignLeft;

				case COL_ICON:
				case COL_TITLE:
				case COL_GAMEID:
				case COL_DISCNUM:
				case COL_REVISION:
				case COL_REGION:
				case COL_IOS:
				case COL_ENCRYPTION:
				case COL_SIG_TICKET:
				case COL_SIG_TMD:
				case COL_APPLOADER:
				default:
					// Center-align the text.
					return Qt::AlignHCenter;
			}
			break;
	}

	// Default value.
	return QVariant();
}

/**
 * Set the RVT-H Reader disk image to use in this model.
 * @param card RVT-H Reader disk image.
 */
void RvtHModel::setRvtH(RvtH *rvth)
{
	Q_D(RvtHModel);

	// NOTE: No signals, since librvth is a C library.

	// Disconnect the Card's changed() signal if a Card is already set.
	if (d->rvth) {
		// Notify the view that we're about to remove all rows.
		const int bankCount = rvth_get_BankCount(d->rvth);
		if (bankCount > 0) {
			beginRemoveRows(QModelIndex(), 0, (bankCount - 1));
		}

		d->rvth = nullptr;

		// Done removing rows.
		if (bankCount > 0) {
			endRemoveRows();
		}
	}

	if (rvth) {
		// Notify the view that we're about to add rows.
		const int bankCount = rvth_get_BankCount(rvth);
		if (bankCount > 0) {
			beginInsertRows(QModelIndex(), 0, (bankCount - 1));
		}

		d->rvth = rvth;

		// Done adding rows.
		if (bankCount > 0) {
			endInsertRows();
		}
	}
}

/** Slots. **/

/**
 * The system theme has changed.
 */
void RvtHModel::themeChanged_slot(void)
{
	// Reinitialize the style.
	Q_D(RvtHModel);
	d->style.init();

	// TODO: Force an update?
}
