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

#include "librvth/rvth.hpp"

// For LBA_TO_BYTES()
#include "nhcd_structs.h"

// C includes. (C++ namespace)
#include <cassert>
#include <cstring>

// Qt includes.
#include <QtCore/QLocale>
#include <QtCore/QDateTime>

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
		static QString sigStatusString(RVL_SigType_e sig_type, RVL_SigStatus_e sig_status);

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
	RVL_SigType_e sig_type, RVL_SigStatus_e sig_status)
{
	QString status;

	switch (sig_type) {
		default:
			status = QString::number(sig_type);
			break;
		case RVL_SigType_Unknown:
			status = BankEntryView::tr("Unknown");
			break;
		case RVL_SigType_Debug:
			status = BankEntryView::tr("Debug");
			break;
		case RVL_SigType_Retail:
			status = BankEntryView::tr("Retail");
			break;
	}

	if (sig_type != RVL_SigType_Unknown) {
		switch (sig_status) {
			default:
			case RVL_SigStatus_Unknown:
				status += QLatin1String(" (") +
					QString::number(sig_status) +
					QChar(L')');
				break;
			case RVL_SigStatus_OK:
				status += QLatin1String(" (") +
					BankEntryView::tr("realsigned") + QChar(L')');
				break;
			case RVL_SigStatus_Invalid:
				status += QLatin1String(" (") +
					BankEntryView::tr("INVALID") + QChar(L')');
				break;
			case RVL_SigStatus_Fake:
				status += QLatin1String(" (") +
					BankEntryView::tr("fakesigned") + QChar(L')');
				break;
		}
	}

	return status;
}

/**
 * Update the widget display.
 */
void BankEntryViewPrivate::updateWidgetDisplay(void)
{
	if (bankEntry) {
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
	} else {
		// No bank entry.
		ui.lblType->hide();
		ui.lblTypeTitle->hide();
		ui.lblSize->hide();
		ui.lblSizeTitle->hide();
	}

	if (!bankEntry || bankEntry->type <= RVTH_BankType_Unknown ||
	    bankEntry->type == RVTH_BankType_Wii_DL_Bank2 ||
	    bankEntry->type >= RVTH_BankType_MAX)
	{
		// No bank entry is loaded, or the selected bank
		// cannot be displayed. Hide all widgets.
		// NOTE: Type and size are handled above.
		ui.lblGameTitle->hide();
		ui.lblTimestampTitle->hide();
		ui.lblTimestamp->hide();
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
	// Remove trailing NULL bytes.
	size_t len = strnlen(bankEntry->discHeader.game_title, sizeof(bankEntry->discHeader.game_title));
	// TODO: Convert from Japanese if necessary.
	// Also cp1252.
	QString s_title = QString::fromLatin1(
		bankEntry->discHeader.game_title, (int)len)
		.trimmed().toHtmlEscaped();
	if (bankEntry->is_deleted) {
		// Indicate that this bank is deleted.
		s_title += QLatin1String("<br/><b>");
		s_title += BankEntryView::tr("[DELETED]");
		s_title += QLatin1String("</b>");
	}
	ui.lblGameTitle->setText(s_title);
	ui.lblGameTitle->show();

	// Timestamp.
	if (bankEntry->timestamp >= 0) {
		QDateTime ts = QDateTime::fromMSecsSinceEpoch((qint64)bankEntry->timestamp * 1000, Qt::UTC);
		ui.lblTimestamp->setText(ts.toString(Qt::DefaultLocaleShortDate));
	} else {
		ui.lblTimestamp->setText(BankEntryView::tr("Unknown"));
	}
	ui.lblTimestamp->show();
	ui.lblTimestampTitle->show();

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
	static const char region_code_tbl[7][4] = {
		"JPN", "USA", "EUR", "ALL", "KOR", "CHN", "TWN"
	};
	if (bankEntry->region_code <= GCN_REGION_TWN) {
		s_region = QLatin1String(region_code_tbl[bankEntry->region_code]);
	} else {
		s_region = QString::number(bankEntry->region_code);
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
			default:
				s_encryption = QString::number(bankEntry->crypto_type);
				break;
			case RVL_CryptoType_Unknown:
				s_encryption = BankEntryView::tr("Unknown");
				break;
			case RVL_CryptoType_None:
				s_encryption = BankEntryView::tr("None");
				break;
			case RVL_CryptoType_Debug:
				s_encryption = BankEntryView::tr("Debug");
				break;
			case RVL_CryptoType_Retail:
				s_encryption = BankEntryView::tr("Retail");
				break;
			case RVL_CryptoType_Korean:
				s_encryption = BankEntryView::tr("Korean");
				break;
		}
		ui.lblEncryption->setText(s_encryption);
		ui.lblEncryption->show();
		ui.lblEncryptionTitle->show();

		// Ticket signature status.
		ui.lblTicketSig->setText(sigStatusString(
			static_cast<RVL_SigType_e>(bankEntry->ticket.sig_type),
			static_cast<RVL_SigStatus_e>(bankEntry->ticket.sig_status)));
		ui.lblTicketSig->show();
		ui.lblTicketSigTitle->show();

		// TMD signature status.
		ui.lblTMDSig->setText(sigStatusString(
			static_cast<RVL_SigType_e>(bankEntry->tmd.sig_type),
			static_cast<RVL_SigStatus_e>(bankEntry->tmd.sig_status)));
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

	// AppLoader status.
	// Check the AppLoader status.
	// TODO: Move strings to librvth?
	if (bankEntry->aplerr > APLERR_OK) {
		// NOTE: Not translatable. These strings are based on
		// the strings from the original AppLoader.
		// NOTE: lblAppLoader uses RichText.
		QString aplerr = QLatin1String("<b>APPLOADER ERROR >>></b><br>\n");
		switch (bankEntry->aplerr) {
			default:
				aplerr += QString::fromLatin1("Unknown (%1)")
					.arg(bankEntry->aplerr);
				break;

			// TODO: Get values for these errors.
			case APLERR_FSTLENGTH:
				aplerr += QString::fromLatin1("FSTLength(%1) in BB2 is greater than FSTMaxLength(%2).")
					.arg(bankEntry->aplerr_val[0])
					.arg(bankEntry->aplerr_val[1]);
				break;
			case APLERR_DEBUGMONSIZE_UNALIGNED:
				aplerr += QString::fromLatin1("Debug monitor size (%1) should be a multiple of 32.")
					.arg(bankEntry->aplerr_val[0]);
				break;
			case APLERR_SIMMEMSIZE_UNALIGNED:
				aplerr += QString::fromLatin1("Simulated memory size (%1) should be a multiple of 32.")
					.arg(bankEntry->aplerr_val[0]);
				break;
			case APLERR_PHYSMEMSIZE_MINUS_SIMMEMSIZE_NOT_GT_DEBUGMONSIZE:
				aplerr += QString::fromLatin1("[Physical memory size(0x%1)] - "
					"[Console simulated memory size(0x%2)] "
					"must be greater than debug monitor size(0x%3).")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16)
					.arg(bankEntry->aplerr_val[2], 0, 16);
				break;
			case APLERR_SIMMEMSIZE_NOT_LE_PHYSMEMSIZE:
				aplerr += QString::fromLatin1("Physical memory size is 0x%1 bytes."
					"Console simulated memory size (0x%2) must be smaller than "
					"or equal to the Physical memory size.")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16);
				break;
			case APLERR_ILLEGAL_FST_ADDRESS:
				aplerr += QString::fromLatin1("Illegal FST destination address! (0x%1)")
					.arg(bankEntry->aplerr_val[0], 0, 16);
				break;
			case APLERR_DOL_EXCEEDS_SIZE_LIMIT:
				aplerr += QString::fromLatin1("Total size of text/data sections of the dol file are too big (%1(0x%2) bytes). "
					"Currently the limit is set as %3(0x%4) bytes.")
					.arg(bankEntry->aplerr_val[0])
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'))
					.arg(bankEntry->aplerr_val[1])
					.arg(bankEntry->aplerr_val[1], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED:
				aplerr += QString::fromLatin1("One of the sections in the dol file exceeded its boundary. "
					"All the sections should not exceed 0x%1 (production mode).<br>\n"
					"<i>*** NOTE: This disc will still boot on devkits.</i>")
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED:
				aplerr += QString::fromLatin1("One of the sections in the dol file exceeded its boundary. "
					"All the sections should not exceed 0x%1 (development mode).")
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_TEXTSEG2BIG:
				aplerr += QString::fromLatin1("Too big text segment! (0x%1 - 0x%2)")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16);
				break;
			case APLERR_DOL_DATASEG2BIG:
				aplerr += QString::fromLatin1("Too big data segment! (0x%1 - 0x%2)")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16);
				break;
		}
		ui.lblAppLoader->setText(aplerr);
		ui.lblAppLoader->show();
	} else {
		// No AppLoader error.
		ui.lblAppLoader->hide();
	}
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
 * Update the currently displayed RvtH_BankEntry.
 */
void BankEntryView::update(void)
{
	Q_D(BankEntryView);
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
