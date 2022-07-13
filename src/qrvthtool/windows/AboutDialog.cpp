/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * AboutDialog.cpp: About Dialog.                                          *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.qrvthtool.h"
#include "AboutDialog.hpp"
#include "git.h"

// C includes.
#include <string.h>

// Qt includes.
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <QScrollArea>

/** AboutDialogPrivate **/

#include "ui_AboutDialog.h"
class AboutDialogPrivate
{
	public:
		explicit AboutDialogPrivate(AboutDialog *q);

	protected:
		AboutDialog *const q_ptr;
		Q_DECLARE_PUBLIC(AboutDialog)
	private:
		Q_DISABLE_COPY(AboutDialogPrivate)

	public:
		static AboutDialog *ms_AboutDialog;
		Ui::AboutDialog ui;

		bool scrlAreaInit;

		// Initialize the About Dialog text.
		void initAboutDialogText(void);

		/**
		 * Initialize the "Credits" tab.
		 */
		void initCreditsTab(void);

		/**
		 * Initialize the "Libraries" tab.
		 */
		void initLibrariesTab(void);

		/**
		 * Initialize the "Support" tab.
		 */
		void initSupportTab(void);
};

// Static member initialization.
AboutDialog *AboutDialogPrivate::ms_AboutDialog = nullptr;


AboutDialogPrivate::AboutDialogPrivate(AboutDialog* q)
	: q_ptr(q)
	, scrlAreaInit(false)
{ }

/**
 * Initialize the About Dialog text.
 */
void AboutDialogPrivate::initAboutDialogText(void)
{
	// Line break string.
	const QLatin1String sLineBreak("<br/>\n");

	// Build the program title text.
	QString sPrgTitle;
	sPrgTitle.reserve(4096);
	sPrgTitle += QLatin1String("<b>") +
			QApplication::applicationDisplayName() +
			QLatin1String("</b>") + sLineBreak +
			AboutDialog::tr("Version %1")
			.arg(QApplication::applicationVersion());
#ifdef RP_GIT_VERSION
	sPrgTitle += sLineBreak + QString::fromUtf8(RP_GIT_VERSION);
#ifdef RP_GIT_DESCRIBE
	sPrgTitle += sLineBreak + QString::fromUtf8(RP_GIT_DESCRIBE);
#endif /* RP_GIT_DESCRIBE */
#endif /* RP_GIT_DESCRIBE */

	// Set the program title text.
        ui.lblTitle->setText(sPrgTitle);

	// Initialize the tabs.
	initCreditsTab();
	initLibrariesTab();
	initSupportTab();

	if (!scrlAreaInit) {
		// Create the scroll areas.
		// Qt Designer's QScrollArea implementation is horribly broken.
		// Also, this has to be done after the labels are set, because
		// QScrollArea is kinda dumb.
		const QString css = QLatin1String(
			"QScrollArea, QLabel { background-color: transparent; }");

		QScrollArea *scrlCredits = new QScrollArea();
		scrlCredits->setFrameShape(QFrame::NoFrame);
		scrlCredits->setFrameShadow(QFrame::Plain);
		scrlCredits->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrlCredits->setStyleSheet(css);
		ui.lblCredits->setStyleSheet(css);
		scrlCredits->setWidget(ui.lblCredits);
		scrlCredits->setWidgetResizable(true);
		ui.vboxCredits->addWidget(scrlCredits);

		QScrollArea *scrlLibraries = new QScrollArea();
		scrlLibraries->setFrameShape(QFrame::NoFrame);
		scrlLibraries->setFrameShadow(QFrame::Plain);
		scrlLibraries->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrlLibraries->setStyleSheet(css);
		ui.lblLibraries->setStyleSheet(css);
		scrlLibraries->setWidget(ui.lblLibraries);
		scrlLibraries->setWidgetResizable(true);
		ui.vboxLibraries->addWidget(scrlLibraries);

		QScrollArea *scrlSupport = new QScrollArea();
		scrlSupport->setFrameShape(QFrame::NoFrame);
		scrlSupport->setFrameShadow(QFrame::Plain);
		scrlSupport->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrlSupport->setStyleSheet(css);
		ui.lblSupport->setStyleSheet(css);
		scrlSupport->setWidget(ui.lblSupport);
		scrlSupport->setWidgetResizable(true);
		ui.vboxSupport->addWidget(scrlSupport);

		// Scroll areas initialized.
		scrlAreaInit = true;
	}

	// Set initial focus to the tabWidget.
	ui.tabWidget->setFocus();
}

/**
 * Initialize the "Credits" tab.
 */
void AboutDialogPrivate::initCreditsTab(void)
{
	const QLatin1String sLineBreak("<br/>\n");
	const QLatin1String sIndent("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
	static const QChar chrBullet(0x2022);  // U+2022: BULLET

	QString credits;
	credits.reserve(4096);
	credits += QLatin1String("Copyright (c) 2018-2019 by David Korth.");
	credits += sLineBreak;
	credits += QLatin1String("This program is <b>NOT</b> licensed or endorsed by Nintendo Co, Ltd.");

	enum CreditType_t {
		CT_CONTINUE = 0,	// Continue previous type.
		CT_TESTERS,		// Testers
		CT_TRANSLATORS,		// Translators

		CT_MAX
	};

	struct CreditsData_t {
		CreditType_t type;
		const char *name;
		const char *url;
		const char *sub;
	};

	// Credits data.
	static const CreditsData_t CreditsData[] = {
		//{CT_TRANSLATORS,	"", nullptr, "en_GB"},
		{CT_MAX, nullptr, nullptr, nullptr}
	};
	
	CreditType_t lastCreditType = CT_CONTINUE;
	for (const CreditsData_t *creditsData = &CreditsData[0];
	     creditsData->type < CT_MAX; creditsData++)
	{
		if (creditsData->type != CT_CONTINUE &&
		    creditsData->type != lastCreditType)
		{
			// New credit type.
			credits += sLineBreak + sLineBreak;
			credits += QLatin1String("<b>");

			switch (creditsData->type) {
				case CT_TESTERS:
					credits += AboutDialog::tr("Testers:");
					break;
				case CT_TRANSLATORS:
					credits += AboutDialog::tr("UI Translators:");
				default:
					break;
			}

			credits += QLatin1String("</b>");
		}

		// Append the contributor's name.
		credits += sLineBreak + sIndent +
			chrBullet + QChar(L' ');
		if (creditsData->url) {
			credits += QLatin1String("<a href='") +
				QLatin1String(creditsData->url) +
				QLatin1String("'>");
		}
		credits += QString::fromUtf8(creditsData->name);
		if (creditsData->url) {
			credits += QLatin1String("</a>");
		}
		if (creditsData->sub) {
			credits += QLatin1String(" (") +
				QLatin1String(creditsData->sub) +
				QChar(L')');
		}
	}

	// We're done building the string.
	ui.lblCredits->setText(credits);
}

/**
 * Initialize the "Libraries" tab.
 */
void AboutDialogPrivate::initLibrariesTab(void)
{
	// Double linebreak.
	const QLatin1String sDLineBreak("\n\n");

	// NOTE: These strings can NOT be static.
	// Otherwise, they won't be retranslated if the UI language
	// is changed at runtime.

	//: Using an internal copy of a library.
	const QString sIntCopyOf = AboutDialog::tr("Internal copy of %1.");
	//: Compiled with a specific version of an external library.
	const QString sCompiledWith = AboutDialog::tr("Compiled with %1.");
	//: Using an external library, e.g. libpcre.so
	const QString sUsingDll = AboutDialog::tr("Using %1.");
	//: License: (libraries with only a single license)
	const QString sLicense = AboutDialog::tr("License: %1");
	//: Licenses: (libraries with multiple licenses)
	const QString sLicenses = AboutDialog::tr("Licenses: %1");

	// Included libraries string.
	QString sLibraries;
	sLibraries.reserve(4096);

	// Icon set.
	sLibraries = QLatin1String(
		"Icon set is based on KDE's Oxygen icons. (5.46.0)\n"
		"Copyright (C) 2005-2018 by David Vignoni.\n");
	sLibraries += sLicenses.arg(QLatin1String("CC BY-SA 3.0, GNU LGPL v2.1+"));

	// TODO: Don't show compiled-with version if the same as in-use version?

	/** Qt **/
	sLibraries += sDLineBreak;
	QString qtVersion = QLatin1String("Qt ") + QLatin1String(qVersion());
#ifdef QT_IS_STATIC
	sLibraries += sIntCopyOf.arg(qtVersion);
#else
	QString qtVersionCompiled = QLatin1String("Qt " QT_VERSION_STR);
	sLibraries += sCompiledWith.arg(qtVersionCompiled) + QChar(L'\n');
	sLibraries += sUsingDll.arg(qtVersion);
#endif /* QT_IS_STATIC */
	sLibraries += QChar(L'\n') +
		QLatin1String("Copyright (C) 1995-2019 The Qt Company Ltd. and/or its subsidiaries.");
	sLibraries += QChar(L'\n') + sLicenses.arg(QLatin1String("GNU LGPL v2.1+, GNU GPL v2+"));

	/** nettle (TODO) **/
	/** getopt_msvc (TODO) **/

	// We're done building the string.
	ui.lblLibraries->setText(sLibraries);
}

/**
 * Initialize the "Support" tab.
 */
void AboutDialogPrivate::initSupportTab(void)
{
	const QLatin1String sLineBreak("<br/>\n");
	static const QChar chrBullet(0x2022);  // U+2022: BULLET

	QString sSupport;
	sSupport.reserve(4096);
	sSupport = AboutDialog::tr(
			"For technical support, you can visit the following websites:") +
			sLineBreak;

	struct supportSite_t {
		const char *name;
		const char *url;
	};

	// Support sites.
	// TODO: Other sites?
	static const supportSite_t supportSites[] = {
		{"GitHub", "https://github.com/GerbilSoft/rvthtool"},
		{nullptr, nullptr}
	};

	for (const supportSite_t *supportSite = &supportSites[0];
	     supportSite->name != nullptr; supportSite++)
	{
		QString siteUrl = QLatin1String(supportSite->url);
		QString siteName = QLatin1String(supportSite->name);
		QString siteUrlHtml = QLatin1String("<a href=\"") +
					siteUrl +
					QLatin1String("\">") +
					siteName +
					QLatin1String("</a>");

		sSupport += chrBullet + QChar(L' ') + siteUrlHtml + sLineBreak;
	}

	// Email the author.
	sSupport += sLineBreak +
			AboutDialog::tr(
				"You can also email the developer directly:") +
			sLineBreak + chrBullet + QChar(L' ') +
			QLatin1String(
				"<a href=\"mailto:gerbilsoft@gerbilsoft.com\">"
				"gerbilsoft@gerbilsoft.com"
				"</a>");

	// We're done building the string.
	ui.lblSupport->setText(sSupport);
}

/** AboutDialog **/

/**
 * Initialize the About Dialog.
 */
AboutDialog::AboutDialog(QWidget *parent)
	: super(parent,
		Qt::Dialog |
		Qt::CustomizeWindowHint |
		Qt::WindowTitleHint |
		Qt::WindowSystemMenuHint |
		Qt::WindowCloseButtonHint)
	, d_ptr(new AboutDialogPrivate(this))
{
	Q_D(AboutDialog);
	d->ui.setupUi(this);

	// Make sure the window is deleted on close.
	this->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_OS_MAC
	// Remove the window icon. (Mac "proxy icon")
	this->setWindowIcon(QIcon());

	// Hide the frames.
	d->ui.fraCopyrights->setFrameShape(QFrame::NoFrame);
	d->ui.fraCopyrights->layout()->setContentsMargins(0, 0, 0, 0);
	d->ui.fraLibraries->setFrameShape(QFrame::NoFrame);
	d->ui.fraLibraries->layout()->setContentsMargins(0, 0, 0, 0);
	d->ui.fraCredits->setFrameShape(QFrame::NoFrame);
	d->ui.fraCredits->layout()->setContentsMargins(0, 0, 0, 0);
#endif

	// Initialize the About Dialog text.
	d->initAboutDialogText();
}

/**
 * Shut down the About Dialog.
 */
AboutDialog::~AboutDialog()
{
	AboutDialogPrivate::ms_AboutDialog = nullptr;
	delete d_ptr;
}

/**
 * Show a single instance of the About Dialog.
 * @param parent Parent window.
 */
void AboutDialog::ShowSingle(QWidget *parent)
{
	if (AboutDialogPrivate::ms_AboutDialog != nullptr) {
		// About Dialog is already displayed.
		// Activate the dialog.
		AboutDialogPrivate::ms_AboutDialog->activateWindow();
	} else {
		// About Dialog is not displayed.
		// Display it.
		AboutDialogPrivate::ms_AboutDialog = new AboutDialog(parent);
		AboutDialogPrivate::ms_AboutDialog->show();
	}
}

/**
 * Widget state has changed.
 * @param event State change event.
 */
void AboutDialog::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange) {
		// Retranslate the UI.
		Q_D(AboutDialog);
		d->ui.retranslateUi(this);

		// Reinitialize the About Dialog text.
		d->initAboutDialogText();
	}

	// Pass the event to the base class.
	super::changeEvent(event);
}
