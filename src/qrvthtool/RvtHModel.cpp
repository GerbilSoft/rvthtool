/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * RvtHModel.hpp: QAbstractListModel for RvtH objects.                     *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "RvtHModel.hpp"

#include "librvth/rvth.hpp"

// C includes (C++ namespace)
#include <cassert>
#include <cstring>

// C++ includes
#include <array>
using std::array;

// Qt includes.
#include <QApplication>
#include <QtGui/QBrush>
#include <QtGui/QFont>
#include <QtGui/QIcon>
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

		// Monosapced font.
		QFont fntMonospace;
	};
	style_t style;

	/**
	 * Load an icon.
	 * @param id Icon ID
	 * @return Icon
	 */
	static QIcon getIcon(RvtHModel::IconID id);

private:
	/**
	 * Load an icon from the Qt resources.
	 * @param dir Base directory
	 * @param name Icon name
	 */
	static QIcon loadIcon(const QString &dir, const QString &name);

	// Icons for COL_TYPE.
	// TODO: QMutexLocker?
	static QIcon ms_icons[RvtHModel::ICON_MAX];

public:
	/**
	 * Get the icon ID for the specified bank.
	 * @param bank Bank number
	 * @return RvtHModel::IconID, or RvtHModel::ICON_MAX on error.
	 */
	RvtHModel::IconID iconIDForBank(unsigned int bank) const;

	/**
	 * Get the icon for the specified bank.
	 * @param bank Bank number
	 * @return QIcon
	 */
	QIcon iconForBank(unsigned int bank) const;
};

/** RvtHModelPrivate **/

QIcon RvtHModelPrivate::ms_icons[RvtHModel::ICON_MAX];

RvtHModelPrivate::RvtHModelPrivate(RvtHModel *q)
	: q_ptr(q)
	, rvth(nullptr)
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
	if (v < 128) v += 48;	// for dark themes
	bgColor_lostFile.setHsv(h, s, v);

	// "Lost" file. (Alternate)
	bgColor_lostFile_alt.getHsv(&h, &s, &v, nullptr);
	h = 60;
	s = (255 - s);
	if (v < 128) v += 48;	// for dark themes
	bgColor_lostFile_alt.setHsv(h, s, v);

	// Save the background colors in QBrush objects.
	brush_lostFile = QBrush(bgColor_lostFile);
	brush_lostFile_alt = QBrush(bgColor_lostFile_alt);

	// Monospaced font.
	fntMonospace = QFont(QStringLiteral("Monospace"));
	fntMonospace.setStyleHint(QFont::TypeWriter);
}

/**
 * Load an icon.
 * @param id Icon ID
 * @return Icon
 */
QIcon RvtHModelPrivate::getIcon(RvtHModel::IconID id)
{
	assert(id >= 0);
	assert(id < RvtHModel::ICON_MAX);
	if (id < 0 || id >= RvtHModel::ICON_MAX) {
		return {};
	}

	if (ms_icons[id].isNull()) {
		static const array<const char*, 5> names = {{
			"gcn", "nr", "wii", "rvtr", "rvth"
		}};
		static_assert(names.size() == RvtHModel::ICON_MAX, "names[] needs to be updated!");
		ms_icons[id] = loadIcon(QStringLiteral("hw"), QLatin1String(names[id]));
		assert(!ms_icons[id].isNull());
	}

	return ms_icons[id];
}

/**
 * Load an icon from the Qt resources.
 * @param dir Base directory
 * @param name Icon name
 */
QIcon RvtHModelPrivate::loadIcon(const QString &dir, const QString &name)
{
	// Icon sizes
	static const array<uint8_t, 7> icoSz = {{16, 22, 24, 32, 48, 64, 128}};

	QIcon icon;
	for (uint8_t p : icoSz) {
		const QString s_sz = QString::number(p);
		const QString full_path = QStringLiteral(":/%1/%2x%2/%3.png").arg(dir, s_sz, name);
		QPixmap pxm(full_path);
		if (!pxm.isNull()) {
			icon.addPixmap(pxm);
		}
	}

	return icon;
}

/**
 * Get the icon ID for the specified bank.
 * @param bank Bank number
 * @return RvtHModel::IconID, or RvtHModel::ICON_MAX on error.
 */
RvtHModel::IconID RvtHModelPrivate::iconIDForBank(unsigned int bank) const
{
	if (!rvth) {
		// No RVT-H Reader image.
		return RvtHModel::ICON_MAX;
	}

	const RvtH_BankEntry *entry = rvth->bankEntry(bank);
	assert(entry != nullptr);
	if (!entry) {
		// No bank entry here...
		return RvtHModel::ICON_MAX;
	}
	const RvtH_ImageType_e imageType = rvth->imageType();

	switch (entry->type) {
		default:
		case RVTH_BankType_Empty:
		case RVTH_BankType_Unknown:
		case RVTH_BankType_Wii_DL_Bank2:
			// No icon for these banks.
			return RvtHModel::ICON_MAX;

		case RVTH_BankType_GCN:
			// GameCube.
			if (imageType == RVTH_ImageType_GCM) {
				// Standalone GCM. Use the retail icon.
				return RvtHModel::ICON_GCN;
			} else {
				// GCM with SDK header, or RVT-H Reader.
				// Use the NR Reader icon.
				return RvtHModel::ICON_NR;
			}
			break;

		case RVTH_BankType_Wii_SL:
		case RVTH_BankType_Wii_DL:
			// Check the signature type.
			// TODO: Check both ticket and TMD?
			switch (entry->ticket.sig_type) {
				default:
					// Should not happen...
					return RvtHModel::ICON_MAX;

				case RVL_SigType_Unknown:
					// Something's not quite right with this bank.
					// Use a retail Wii icon.
					return RvtHModel::ICON_WII;

				case RVL_SigType_Retail:
					return RvtHModel::ICON_WII;

				case RVL_SigType_Debug:
					// Check the encryption type.
					switch (entry->crypto_type) {
						default:
						case RVL_CryptoType_Unknown:
						case RVL_CryptoType_Retail:
						case RVL_CryptoType_Korean:
						case RVL_CryptoType_vWii:	// FIXME: Debug vWii?
							// Should not happen...
							return RvtHModel::ICON_MAX;

						case RVL_CryptoType_None:
							return RvtHModel::ICON_RVTH;

						case RVL_CryptoType_Debug:
							return RvtHModel::ICON_RVTR;
					}
					break;
			}
			break;
	}

	assert(!"Should not get here!");
	return RvtHModel::ICON_MAX;
}

/**
 * Get the icon for the specified bank.
 * @param bank Bank number
 * @return QIcon
 */
QIcon RvtHModelPrivate::iconForBank(unsigned int bank) const
{
	const RvtHModel::IconID iconID = iconIDForBank(bank);
	if (iconID >= RvtHModel::ICON_MAX) {
		// No icon.
		return QIcon();
	}
	return getIcon(iconID);
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
		return d->rvth->bankCount();
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
	if (!d->rvth || !index.isValid()) {
		return {};
	}
	if (index.row() >= rowCount()) {
		return {};
	}

	// Get the bank entry.
	const unsigned int bank = static_cast<unsigned int>(index.row());
	const RvtH_BankEntry *const entry = d->rvth->bankEntry(bank);
	if (!entry) {
		// No entry...
		return {};
	}

	// HACK: Increase icon width on Windows.
	// Figure out a better method later.
#ifdef Q_OS_WIN
	static constexpr int iconWadj = 8;
#else
	static constexpr int iconWadj = 0;
#endif

	switch (entry->type) {
		case RVTH_BankType_Empty:
			// Empty slot.
			switch (index.column()) {
				case COL_BANKNUM:
					// Bank number.
					switch (role) {
						case Qt::DisplayRole:
							return QString::number(bank + 1);
						case Qt::TextAlignmentRole:
							// Center-align the text.
							return Qt::AlignCenter;
						default:
							break;
					}
					break;

				case COL_TYPE:
					if (role == Qt::SizeHintRole) {
						// Using 32x32 icons.
						// (Hi-DPI is handled by Qt automatically.)
						QSize size(32 + iconWadj, 32);
						if (entry->type == RVTH_BankType_Wii_DL) {
							// Double-size the row.
							size.setHeight(size.height() * 2);
						}
						return size;
					}
					break;

				default:
					break;
			}
			// All other columns are empty.
			return {};

		case RVTH_BankType_Wii_DL_Bank2:
			return {};

		default:
			break;
	}

	// TODO: Move some of this to RvtHItemDelegate?
	switch (role) {
		case Qt::DisplayRole:
			// TODO: Cache these?
			switch (index.column()) {
				case COL_BANKNUM: {
					QString banknum = QString::number(bank + 1);
					if (entry->type == RVTH_BankType_Wii_DL) {
						// Print both bank numbers.
						banknum += QChar(L'\n') + QString::number(bank + 2);
					}
					return banknum;
				}
				case COL_TITLE: {
					// Remove trailing NULL bytes.
					size_t len = strnlen(entry->discHeader.game_title, sizeof(entry->discHeader.game_title));
					// TODO: Convert from Japanese if necessary.
					// Also cp1252.
					return QString::fromLatin1(entry->discHeader.game_title, (int)len).trimmed();
				}
				case COL_GAMEID:
					return QLatin1String(entry->discHeader.id6, sizeof(entry->discHeader.id6));
				case COL_DISCNUM:
					return QString::number(entry->discHeader.disc_number);
				case COL_REVISION:
					// TODO: BCD?
					return QString::number(entry->discHeader.revision);

				case COL_REGION: {
					// TODO: Icon?
					QString s_region;
					static const char region_code_tbl[7][4] = {
						"JPN", "USA", "EUR", "ALL", "KOR", "CHN", "TWN"
					};
					if (entry->region_code <= GCN_REGION_TWN) {
						s_region = QLatin1String(region_code_tbl[entry->region_code]);
					} else {
						s_region = QString::number(entry->region_code);
					}
					return s_region;
				}

				case COL_IOS_VERSION:
					// Wii only.
					if (entry->type == RVTH_BankType_Wii_SL ||
					    entry->type == RVTH_BankType_Wii_DL)
					{
						return QString::number(entry->ios_version);
					}
					break;

				default:
					break;
			}
			break;

		case Qt::DecorationRole:
			if (index.column() == COL_TYPE) {
				// Get the icon for this bank.
				return d->iconForBank(bank);
			}
			break;

		case Qt::TextAlignmentRole:
			switch (index.column()) {
				case COL_TITLE:
					// Left-align, center vertically.
					return (int)(Qt::AlignLeft | Qt::AlignVCenter);

				case COL_TYPE:
				case COL_BANKNUM:
				case COL_GAMEID:
				case COL_DISCNUM:
				case COL_REVISION:
				case COL_REGION:
				case COL_IOS_VERSION:
				default:
					// Center-align the text.
					return Qt::AlignCenter;
			}

		case Qt::FontRole:
			switch (index.column()) {
				case COL_GAMEID: {
					// These columns should be monospaced.
					return d->style.fntMonospace;
				}

				default:
					break;
			}
			break;

		case Qt::BackgroundRole:
			// "Deleted" banks should be displayed using a different color.
			if (entry->is_deleted) {
				// TODO: Check if the item view is using alternating row colors before using them.
				if (bank & 1)
					return d->style.brush_lostFile_alt;
				else
					return d->style.brush_lostFile;
			}
			break;

		case Qt::SizeHintRole: {
			if (index.column() == COL_TYPE) {
				// Using 32x32 icons.
				// (Hi-DPI is handled by Qt automatically.)
				QSize size(32 + iconWadj, 32);
				if (entry->type == RVTH_BankType_Wii_DL) {
					// Double-size the row.
					size.setHeight(size.height() * 2);
				}
				return size;
			}
			break;
		}

		case DualLayerRole:
			return (entry->type == RVTH_BankType_Wii_DL);

		default:
			break;
	}

	// Default value.
	return {};
}

QVariant RvtHModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(orientation);

	switch (role) {
		case Qt::DisplayRole:
			switch (section) {
				case COL_BANKNUM:	return tr("#");
				case COL_TYPE:		return tr("Type");
				case COL_TITLE:		return tr("Title");
				//: 6-character game ID, e.g. GALE01.
				case COL_GAMEID:	return tr("Game ID");
				case COL_DISCNUM:	return tr("Disc #");
				case COL_REVISION:	return tr("Revision");
				case COL_REGION:	return tr("Region");
				case COL_IOS_VERSION:	return tr("IOS");

				default:
					break;
			}
			break;

		case Qt::TextAlignmentRole:
			// Center-align the text.
			return Qt::AlignHCenter;
	}

	// Default value.
	return {};
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
		const int bankCount = d->rvth->bankCount();
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
		const int bankCount = rvth->bankCount();
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

/**
 * Load an icon.
 * @param id Icon ID.
 * @return Icon.
 */
QIcon RvtHModel::getIcon(IconID id)
{
	return RvtHModelPrivate::getIcon(id);
}

/**
 * Get the icon ID for the first bank.
 * Used for the application icon.
 * @return Icon ID, or ICON_MAX on error.
 */
RvtHModel::IconID RvtHModel::iconIDForBank1(void) const
{
	Q_D(const RvtHModel);
	return d->iconIDForBank(0);
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

/**
 * Force the RVT-H model to update a bank.
 * @param bank Bank number.
 */
void RvtHModel::forceBankUpdate(unsigned int bank)
{
	Q_D(RvtHModel);
	if (!d->rvth)
		return;

	const unsigned int bankCount = d->rvth->bankCount();
	if (bank >= bankCount) {
		// Out of range.
		return;
	}

	// Data for this bank is changed.
	// Force update this bank and the next bank,
	// in case the bank was previously DL.
	const unsigned int bank2 = (bank == bankCount-1 ? bank : bank+1);
	QModelIndex idxStart = index(bank, 0);
	QModelIndex idxEnd = index(bank2, COL_MAX-1);
	emit dataChanged(idxStart, idxEnd);
}
