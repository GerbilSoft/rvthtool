/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * BankEntryView.cpp: Bank Entry view widget.                              *
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

#include "BankEntryView.hpp"

#include "librvth/rvth.h"

// C includes. (C++ namespace)
#include <cassert>

// Qt includes.
#include <QtCore/QLocale>

/** BankEntryViewPrivate **/

#include "ui_BankEntryView.h"
class BankEntryViewPrivate
{
	public:
		explicit BankEntryViewPrivate(BankEntryView *q);

	protected:
		BankEntryView *const q_ptr;
		Q_DECLARE_PUBLIC(BankEntryView)
	private:
		Q_DISABLE_COPY(BankEntryViewPrivate)

	public:
		// UI
		Ui::BankEntryView ui;

		const RvtH_BankEntry *bankEntry;

		static inline int calc_frac_part(quint64 size, quint64 mask);

		/**
		 * Format a file size.
		 * TODO: Move to a common file so other files can use this?
		 * @param size File size.
		 * @return Formatted file size.
		 */
		static QString formatFileSize(quint64 size);

		/**
		 * Get a string for ticket/TMD status.
		 * @param sig_type Signature type.
		 * @param sig_status Signature status.
		 * @return String for ticket/TMD status.
		 */
		static QString sigStatusString(RvtH_SigType_e sig_type, RvtH_SigStatus_e sig_status);

		/**
		 * Update the widget display.
		 */
		void updateWidgetDisplay(void);
};

BankEntryViewPrivate::BankEntryViewPrivate(BankEntryView *q)
	: q_ptr(q)
	, bankEntry(nullptr)
{ }

inline int BankEntryViewPrivate::calc_frac_part(quint64 size, quint64 mask)
{
	float f = static_cast<float>(size & (mask - 1)) / static_cast<float>(mask);
	unsigned int frac_part = static_cast<unsigned int>(f * 1000.0f);

	// MSVC added round() and roundf() in MSVC 2013.
	// Use our own rounding code instead.
	unsigned int round_adj = (frac_part % 10 > 5);
	frac_part /= 10;
	frac_part += round_adj;
	return frac_part;
}

/**
 * Format a file size.
 * TODO: Move to a common file so other files can use this?
 * @param size File size.
 * @return Formatted file size.
 */
QString BankEntryViewPrivate::formatFileSize(quint64 size)
{
	// System locale.
	QLocale locale = QLocale::system();

	// Localized suffix.
	QString suffix;
	// frac_part is always 0 to 100.
	// If whole_part >= 10, frac_part is divided by 10.
	unsigned int whole_part, frac_part;

	// TODO: Optimize this?
	if (size < (2ULL << 10)) {
		// tr: Bytes (< 1,024)
		suffix = BankEntryView::tr("byte(s)", "", (int)size);
		whole_part = static_cast<unsigned int>(size);
		frac_part = 0;
	} else if (size < (2ULL << 20)) {
		// tr: Kilobytes
		suffix = BankEntryView::tr("KiB");
		whole_part = static_cast<unsigned int>(size >> 10);
		frac_part = calc_frac_part(size, (1ULL << 10));
	} else if (size < (2ULL << 30)) {
		// tr: Megabytes
		suffix = BankEntryView::tr("MiB");
		whole_part = static_cast<unsigned int>(size >> 20);
		frac_part = calc_frac_part(size, (1ULL << 20));
	} else if (size < (2ULL << 40)) {
		// tr: Gigabytes
		suffix = BankEntryView::tr("GiB");
		whole_part = static_cast<unsigned int>(size >> 30);
		frac_part = calc_frac_part(size, (1ULL << 30));
	} else if (size < (2ULL << 50)) {
		// tr: Terabytes
		suffix = BankEntryView::tr("TiB");
		whole_part = static_cast<unsigned int>(size >> 40);
		frac_part = calc_frac_part(size, (1ULL << 40));
	} else if (size < (2ULL << 60)) {
		// tr: Petabytes
		suffix = BankEntryView::tr("PiB");
		whole_part = static_cast<unsigned int>(size >> 50);
		frac_part = calc_frac_part(size, (1ULL << 50));
	} else /*if (size < (2ULL << 70))*/ {
		// tr: Exabytes
		suffix = BankEntryView::tr("EiB");
		whole_part = static_cast<unsigned int>(size >> 60);
		frac_part = calc_frac_part(size, (1LL << 60));
	}

	// Localize the whole part.
	QString s_value = locale.toString(whole_part);

	if (size >= (2LL << 10)) {
		// Fractional part.
		unsigned int frac_digits = 2;
		if (whole_part >= 10) {
			unsigned int round_adj = (frac_part % 10 > 5);
			frac_part /= 10;
			frac_part += round_adj;
			frac_digits = 1;
		}

		// Get the localized decimal point.
		s_value += locale.decimalPoint();

		// Append the fractional part using the required number of digits.
		char buf[16];
		snprintf(buf, sizeof(buf), "%0*u", frac_digits, frac_part);
		s_value += QLatin1String(buf);
	}

	if (!suffix.isEmpty()) {
		//: %1 == localized value, %2 == suffix (e.g. MiB)
		return BankEntryView::tr("%1 %2").arg(s_value).arg(suffix);
	} else {
		return s_value;
	}

	// Should not get here...
	assert(!"Invalid code path.");
	return QLatin1String("QUACK");
}

/**
 * Get a string for ticket/TMD status.
 * @param sig_type Signature type.
 * @param sig_status Signature status.
 * @return String for ticket/TMD status.
 */
QString BankEntryViewPrivate::sigStatusString(
	RvtH_SigType_e sig_type, RvtH_SigStatus_e sig_status)
{
	QString status;

	switch (sig_type) {
		case RVTH_SigType_Unknown:
		default:
			status = QString::number(sig_type);
			break;
		case RVTH_SigType_Debug:
			status = BankEntryView::tr("Debug");
			break;
		case RVTH_SigType_Retail:
			status = BankEntryView::tr("Retail");
			break;
	}

	switch (sig_status) {
		case RVTH_SigStatus_Unknown:
		default:
			status += QLatin1String(" (") +
				QString::number(sig_status) +
				QChar(L')');
			break;
		case RVTH_SigStatus_OK:
			break;
		case RVTH_SigStatus_Invalid:
			status += QLatin1String(" (") +
				BankEntryView::tr("INVALID") + QChar(L')');
			break;
		case RVTH_SigStatus_Fake:
			status += QLatin1String(" (") +
				BankEntryView::tr("fakesigned") + QChar(L')');
			break;
	}

	return status;
}

/**
 * Update the widget display.
 */
void BankEntryViewPrivate::updateWidgetDisplay(void)
{
	Q_Q(BankEntryView);

	if (!bankEntry || bankEntry->type <= RVTH_BankType_Unknown ||
	    bankEntry->type == RVTH_BankType_Wii_DL_Bank2 ||
	    bankEntry->type >= RVTH_BankType_MAX)
	{
		// No bank entry is loaded, or the selected bank
		// cannot be displayed. Hide all widgets.
		ui.lblGameTitle->hide();
		ui.lblTypeTitle->hide();
		ui.lblType->hide();
		ui.lblSizeTitle->hide();
		ui.lblSize->hide();
		ui.lblGameIDTitle->hide();
		ui.lblGameID->hide();
		ui.lblDiscNumTitle->hide();
		ui.lblDiscNum->hide();
		ui.lblRevisionTitle->hide();
		ui.lblRevision->hide();
		ui.lblRegionTitle->hide();
		ui.lblRegion->hide();
		ui.lblIOSVersionTitle->hide();
		ui.lblIOSVersion->hide();
		ui.lblEncryptionTitle->hide();
		ui.lblEncryption->hide();
		ui.lblTicketSigTitle->hide();
		ui.lblTicketSig->hide();
		ui.lblTMDSigTitle->hide();
		ui.lblTMDSig->hide();
		ui.lblAppLoader->hide();
		return;
	}

	// Set the widget display.

	// Game title.
	// TODO: Is Shift-JIS permissible in the game title?
	// TODO: cp1252
	ui.lblGameTitle->setText(QString::fromLatin1(
		bankEntry->discHeader.game_title,
		sizeof(bankEntry->discHeader.game_title)).trimmed());
	ui.lblGameTitle->show();

	// Type.
	QString s_type;
	switch (bankEntry->type) {
		case RVTH_BankType_Unknown:
		default:
			s_type = BankEntryView::tr("Unknown");
			break;
		case RVTH_BankType_Empty:
			s_type = BankEntryView::tr("Empty");
			break;
		case RVTH_BankType_GCN:
			s_type = BankEntryView::tr("GameCube");
			break;
		case RVTH_BankType_Wii_SL:
			s_type = BankEntryView::tr("Wii (Single-Layer)");
			break;
		case RVTH_BankType_Wii_DL:
			s_type = BankEntryView::tr("Wii (Dual-Layer)");
			break;
		case RVTH_BankType_Wii_DL_Bank2:
			s_type = BankEntryView::tr("Wii (DL Bank 2)");
			break;
	}
	ui.lblType->setText(s_type);
	ui.lblType->show();
	ui.lblTypeTitle->show();

	// Size.
	ui.lblSize->setText(formatFileSize(LBA_TO_BYTES(bankEntry->lba_len)));
	ui.lblSize->show();
	ui.lblSizeTitle->show();

	// Game ID.
	ui.lblGameID->setText(QLatin1String(
		bankEntry->discHeader.id6,
		sizeof(bankEntry->discHeader.id6)));
	ui.lblGameID->show();
	ui.lblGameIDTitle->show();

	// Disc number.
	ui.lblDiscNum->setText(QString::number(bankEntry->discHeader.disc_number));
	ui.lblDiscNum->show();
	ui.lblDiscNumTitle->show();

	// Revision. (TODO: BCD?)
	ui.lblRevision->setText(QString::number(bankEntry->discHeader.revision));
	ui.lblRevision->show();
	ui.lblRevisionTitle->show();

	// Region. (TODO: Icon?)
	QString s_region;
	switch (bankEntry->region_code) {
		default:
			s_region = QString::number(bankEntry->region_code);
			break;
		case GCN_REGION_JAPAN:
			s_region = QLatin1String("JPN");
			break;
		case GCN_REGION_USA:
			s_region = QLatin1String("USA");
			break;
		case GCN_REGION_PAL:
			s_region = QLatin1String("EUR");
			break;
		case GCN_REGION_FREE:
			s_region = QLatin1String("ALL");
			break;
		case GCN_REGION_SOUTH_KOREA:
			s_region = QLatin1String("KOR");
			break;
	}
	ui.lblRegion->setText(s_region);
	ui.lblRegion->show();
	ui.lblRegionTitle->show();

	// Wii-only fields.
	if (bankEntry->type == RVTH_BankType_Wii_SL ||
	    bankEntry->type == RVTH_BankType_Wii_DL)
	{
		// IOS version.
		ui.lblIOSVersion->setText(QString::number(bankEntry->ios_version));
		ui.lblIOSVersion->show();
		ui.lblIOSVersionTitle->show();

		// Encryption.
		QString s_encryption;
		switch (bankEntry->crypto_type) {
			case RVTH_CryptoType_Unknown:
			default:
				s_encryption = QString::number(bankEntry->crypto_type);
				break;
			case RVTH_CryptoType_None:
				s_encryption = BankEntryView::tr("None");
				break;
			case RVTH_CryptoType_Debug:
				s_encryption = BankEntryView::tr("Debug");
				break;
			case RVTH_CryptoType_Retail:
				s_encryption = BankEntryView::tr("Retail");
				break;
			case RVTH_CryptoType_Korean:
				s_encryption = BankEntryView::tr("Korean");
				break;
		}
		ui.lblEncryption->setText(s_encryption);
		ui.lblEncryption->show();
		ui.lblEncryptionTitle->show();

		// Ticket signature status.
		ui.lblTicketSig->setText(sigStatusString(
			static_cast<RvtH_SigType_e>(bankEntry->ticket.sig_type),
			static_cast<RvtH_SigStatus_e>(bankEntry->ticket.sig_status)));
		ui.lblTicketSig->show();
		ui.lblTicketSigTitle->show();

		// TMD signature status.
		ui.lblTMDSig->setText(sigStatusString(
			static_cast<RvtH_SigType_e>(bankEntry->tmd.sig_type),
			static_cast<RvtH_SigStatus_e>(bankEntry->tmd.sig_status)));
		ui.lblTMDSig->show();
		ui.lblTMDSigTitle->show();
	} else {
		// Not Wii. Hide the fields.
		ui.lblIOSVersionTitle->hide();
		ui.lblIOSVersion->hide();
		ui.lblEncryptionTitle->hide();
		ui.lblEncryption->hide();
		ui.lblTicketSigTitle->hide();
		ui.lblTicketSig->hide();
		ui.lblTMDSigTitle->hide();
		ui.lblTMDSig->hide();
	}

	// TODO: AppLoader status.
}

/** BankEntryView **/

BankEntryView::BankEntryView(QWidget *parent)
	: super(parent)
	, d_ptr(new BankEntryViewPrivate(this))
{
	Q_D(BankEntryView);
	d->ui.setupUi(this);
	d->updateWidgetDisplay();
}

BankEntryView::~BankEntryView()
{
	Q_D(BankEntryView);
	delete d;
}

/**
 * Get the RvtH_BankEntry being displayed.
 * @return RvtH_BankEntry.
 */
const RvtH_BankEntry *BankEntryView::bankEntry(void) const
{
	Q_D(const BankEntryView);
	return d->bankEntry;
}

/**
 * Set the RvtH_BankEntry being displayed.
 * @param bankEntry RvtH_BankEntry.
 */
void BankEntryView::setBankEntry(const RvtH_BankEntry *bankEntry)
{
	Q_D(BankEntryView);
	d->bankEntry = bankEntry;
	d->updateWidgetDisplay();
}


/**
 * Widget state has changed.
 * @param event State change event.
 */
void BankEntryView::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange) {
		// Retranslate the UI.
		Q_D(BankEntryView);
		d->ui.retranslateUi(this);
		d->updateWidgetDisplay();
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}
