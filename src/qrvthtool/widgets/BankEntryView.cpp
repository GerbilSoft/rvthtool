/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * BankEntryView.cpp: Bank Entry view widget.                              *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "BankEntryView.hpp"

// for LBA_TO_BYTES()
#include "nhcd_structs.h"
// for formatSize()
#include "../FormatSize.hpp"

// C includes (C++ namespace)
#include <cassert>
#include <cstring>

// Qt includes
#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

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

	/**
	 * Get a string for ticket/TMD status.
	 * @param sig_type Signature type
	 * @param sig_status Signature status
	 * @return String for ticket/TMD status
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

/**
 * Get a string for ticket/TMD status.
 * @param sig_type Signature type
 * @param sig_status Signature status
 * @return String for ticket/TMD status
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
				status += QStringLiteral(" (%1)")
					.arg(static_cast<int>(sig_status));
				break;
			case RVL_SigStatus_OK:
				status += QStringLiteral(" (%1)")
					.arg(BankEntryView::tr("realsigned"));
				break;
			case RVL_SigStatus_Invalid:
				status += QStringLiteral(" (%1)")
					.arg(BankEntryView::tr("INVALID"));
				break;
			case RVL_SigStatus_Fake:
				status += QStringLiteral(" (%1)")
					.arg(BankEntryView::tr("fakesigned"));
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
	if (!bankEntry || bankEntry->type <= RVTH_BankType_Unknown ||
	    bankEntry->type == RVTH_BankType_Wii_DL_Bank2 ||
	    bankEntry->type >= RVTH_BankType_MAX)
	{
		// No bank entry is loaded, or the selected bank
		// cannot be displayed. Hide all widgets.
		// NOTE: Type and size are handled above.
		Q_Q(BankEntryView);
		for (QObject *obj : q->children()) {
			QWidget *const widget = qobject_cast<QWidget*>(obj);
			if (widget) {
				widget->hide();
			}
		}
		return;
	}

	// Type
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

	// Size
	ui.lblSize->setText(formatSize(LBA_TO_BYTES(bankEntry->lba_len)));
	ui.lblSize->show();
	ui.lblSizeTitle->show();

	// Game title
	// Remove trailing NULL bytes.
	size_t len = strnlen(bankEntry->discHeader.game_title, sizeof(bankEntry->discHeader.game_title));
	// TODO: Convert from Japanese if necessary.
	// Also cp1252.
	QString s_title = QString::fromLatin1(
		bankEntry->discHeader.game_title, (int)len)
		.trimmed().toHtmlEscaped();
	if (bankEntry->is_deleted) {
		// Indicate that this bank is deleted.
		s_title += QStringLiteral("<br/><b>%1</b>")
			.arg(BankEntryView::tr("[DELETED]"));
	}
	ui.lblGameTitle->setText(s_title);
	ui.lblGameTitle->show();

	// Timestamp
	if (bankEntry->timestamp >= 0) {
		// NOTE: Qt::DefaultLocaleShortDate was removed in Qt6,
		// so switching to QLocale for formatting.
		QLocale locale(QLocale::system());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const QTimeZone timeZone = QTimeZone(QTimeZone::UTC);
#else /* QT_VERSION < QT_VERSION_CHECK(6, 0, 0) */
		static constexpr Qt::TimeSpec timeZone = Qt::UTC;
#endif
		QDateTime ts = QDateTime::fromMSecsSinceEpoch(
			(qint64)bankEntry->timestamp * 1000, timeZone);
		ui.lblTimestamp->setText(ts.toString(
			locale.dateTimeFormat(QLocale::ShortFormat)));
	} else {
		ui.lblTimestamp->setText(BankEntryView::tr("Unknown"));
	}
	ui.lblTimestamp->show();
	ui.lblTimestampTitle->show();

	// Game ID
	ui.lblGameID->setText(QLatin1String(
		bankEntry->discHeader.id6,
		sizeof(bankEntry->discHeader.id6)));
	ui.lblGameID->show();
	ui.lblGameIDTitle->show();

	// Disc number
	ui.lblDiscNum->setText(QString::number(bankEntry->discHeader.disc_number));
	ui.lblDiscNum->show();
	ui.lblDiscNumTitle->show();

	// Revision. (TODO: BCD?)
	ui.lblRevision->setText(QString::number(bankEntry->discHeader.revision));
	ui.lblRevision->show();
	ui.lblRevisionTitle->show();

	// Region (TODO: Icon?)
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
			case RVL_CryptoType_vWii:
				s_encryption = BankEntryView::tr("vWii");
				break;
			case RVL_CryptoType_vWii_Debug:
				s_encryption = BankEntryView::tr("vWii (debug)");
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
		QString aplerr = QStringLiteral("<b>APPLOADER ERROR >>></b><br>\n");
		switch (bankEntry->aplerr) {
			default:
				aplerr += QStringLiteral("Unknown (%1)")
					.arg(bankEntry->aplerr);
				break;

			// TODO: Get values for these errors.
			case APLERR_FSTLENGTH:
				aplerr += QStringLiteral("FSTLength(%1) in BB2 is greater than FSTMaxLength(%2).")
					.arg(bankEntry->aplerr_val[0])
					.arg(bankEntry->aplerr_val[1]);
				break;
			case APLERR_DEBUGMONSIZE_UNALIGNED:
				aplerr += QStringLiteral("Debug monitor size (%1) should be a multiple of 32.")
					.arg(bankEntry->aplerr_val[0]);
				break;
			case APLERR_SIMMEMSIZE_UNALIGNED:
				aplerr += QStringLiteral("Simulated memory size (%1) should be a multiple of 32.")
					.arg(bankEntry->aplerr_val[0]);
				break;
			case APLERR_PHYSMEMSIZE_MINUS_SIMMEMSIZE_NOT_GT_DEBUGMONSIZE:
				aplerr += QStringLiteral("[Physical memory size(0x%1)] - [Console simulated memory size(0x%2)] must be greater than debug monitor size(0x%3).")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16)
					.arg(bankEntry->aplerr_val[2], 0, 16);
				break;
			case APLERR_SIMMEMSIZE_NOT_LE_PHYSMEMSIZE:
				aplerr += QStringLiteral("Physical memory size is 0x%1 bytes. Console simulated memory size (0x%2) must be smaller than or equal to the Physical memory size.")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16);
				break;
			case APLERR_ILLEGAL_FST_ADDRESS:
				aplerr += QStringLiteral("Illegal FST destination address! (0x%1)")
					.arg(bankEntry->aplerr_val[0], 0, 16);
				break;
			case APLERR_DOL_EXCEEDS_SIZE_LIMIT:
				aplerr += QStringLiteral("Total size of text/data sections of the dol file are too big (%1(0x%2) bytes). Currently the limit is set as %3(0x%4) bytes.")
					.arg(bankEntry->aplerr_val[0])
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'))
					.arg(bankEntry->aplerr_val[1])
					.arg(bankEntry->aplerr_val[1], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_ADDR_LIMIT_RETAIL_EXCEEDED:
				aplerr += QStringLiteral("One of the sections in the dol file exceeded its boundary. All the sections should not exceed 0x%1 (production mode).<br>\n<i>*** NOTE: This disc will still boot on devkits.</i>")
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_ADDR_LIMIT_DEBUG_EXCEEDED:
				aplerr += QStringLiteral("One of the sections in the dol file exceeded its boundary. All the sections should not exceed 0x%1 (development mode).")
					.arg(bankEntry->aplerr_val[0], 0, 16, QChar(L'0'));
				break;
			case APLERR_DOL_TEXTSEG2BIG:
				aplerr += QStringLiteral("Too big text segment! (0x%1 - 0x%2)")
					.arg(bankEntry->aplerr_val[0], 0, 16)
					.arg(bankEntry->aplerr_val[1], 0, 16);
				break;
			case APLERR_DOL_DATASEG2BIG:
				aplerr += QStringLiteral("Too big data segment! (0x%1 - 0x%2)")
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
 * @return RvtH_BankEntry
 */
const RvtH_BankEntry *BankEntryView::bankEntry(void) const
{
	Q_D(const BankEntryView);
	return d->bankEntry;
}

/**
 * Set the RvtH_BankEntry being displayed.
 * @param bankEntry RvtH_BankEntry
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
	Q_D(BankEntryView);

	switch (event->type()) {
		case QEvent::LanguageChange:
		case QEvent::LocaleChange:
			// Retranslate the UI.
			d->ui.retranslateUi(this);
			d->updateWidgetDisplay();
			break;

		default:
			break;
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}
