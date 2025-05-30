/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * MessageWidget.hpp: Message widget.                                      *
 *                                                                         *
 * Copyright (c) 2014-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "MessageWidget.hpp"

// Qt includes.
#include <QtCore/QTimer>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

// Qt widgets.
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

// Qt animation includes.
#include <QtCore/QTimeLine>

/** MessageWidgetPrivate **/

class MessageWidgetPrivate
{
public:
	explicit MessageWidgetPrivate(MessageWidget *q);
	~MessageWidgetPrivate();

protected:
	MessageWidget *const q_ptr;
	Q_DECLARE_PUBLIC(MessageWidget)
private:
	Q_DISABLE_COPY(MessageWidgetPrivate)

public:
	struct Ui_MessageWidget {
		QHBoxLayout *hboxMain;
		QFrame *content;
		QHBoxLayout *hboxFrame;
		QLabel *lblIcon;
		QLabel *lblMessage;
		QToolButton *btnDismiss;

		void setupUi(QWidget *MessageWidget);
	};
	Ui_MessageWidget ui;

	/**
	 * Set the icon.
	 * @param icon Icon to set
	 */
	void setIcon(MessageWidget::MsgIcon icon);

	/*
	 * Calculate the best height for the widget.
	 * @return Best height
	 */
	int calcBestHeight(void) const;

public:
	// Colors
	// TODO: Use system colors on KDE?
	static constexpr QRgb colorCritical = 0xEE4444;
	static constexpr QRgb colorQuestion = 0x66EE66;
	static constexpr QRgb colorWarning = 0xEECC66;
	static constexpr QRgb colorInformation = 0x66CCEE;

public:
	QTimer* tmrTimeout;			// Message timeout
	QTimeLine* timeLine;			// Animation timeline
	MessageWidget::MsgIcon icon;		// Icon
	static constexpr int iconSz = 22;	// Icon size
	bool timeout;				// True if message was dismissed via timeout.
	bool animateOnShow;			// Animate the widget on show?
};

MessageWidgetPrivate::MessageWidgetPrivate(MessageWidget *q)
	: q_ptr(q)
	, tmrTimeout(new QTimer(q))
	, timeLine(new QTimeLine(500, q))
	, icon(MessageWidget::ICON_NONE)
	, timeout(false)
	, animateOnShow(false)
{ }

MessageWidgetPrivate::~MessageWidgetPrivate()
{ }

/**
 * Initialize the UI.
 * @param MessageWidget MessageWidget.
 */
void MessageWidgetPrivate::Ui_MessageWidget::setupUi(QWidget *MessageWidget)
{
	if (MessageWidget->objectName().isEmpty())
		MessageWidget->setObjectName(QStringLiteral("MessageWidget"));

	hboxMain = new QHBoxLayout(MessageWidget);
	hboxMain->setContentsMargins(2, 2, 2, 2);
	hboxMain->setObjectName(QStringLiteral("hboxMain"));

	content = new QFrame(MessageWidget);
	content->setObjectName(QStringLiteral("content"));
	QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(content->sizePolicy().hasHeightForWidth());
	content->setSizePolicy(sizePolicy);
	content->setFrameShape(QFrame::NoFrame);
	content->setLineWidth(0);

	hboxFrame = new QHBoxLayout(content);
	hboxFrame->setContentsMargins(0, 0, 0, 0);
	hboxFrame->setObjectName(QStringLiteral("hboxFrame"));

	lblIcon = new QLabel(content);
	lblIcon->setObjectName(QStringLiteral("lblIcon"));
	QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(lblIcon->sizePolicy().hasHeightForWidth());
	lblIcon->setSizePolicy(sizePolicy1);
	lblIcon->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);

	hboxFrame->addWidget(lblIcon);
	hboxFrame->setAlignment(lblIcon, Qt::AlignTop);

	lblMessage = new QLabel(content);
	lblMessage->setObjectName(QStringLiteral("lblMessage"));
	lblMessage->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
	lblMessage->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

	hboxFrame->addWidget(lblMessage);
	hboxFrame->setAlignment(lblMessage, Qt::AlignTop);

	btnDismiss = new QToolButton(content);
	btnDismiss->setObjectName(QStringLiteral("btnDismiss"));
	btnDismiss->setFocusPolicy(Qt::NoFocus);
	btnDismiss->setStyleSheet(QStringLiteral("margin: 0px; padding: 0px;"));

	// Icon for the "Dismiss" button.
	if (QIcon::hasThemeIcon(QStringLiteral("dialog-close"))) {
		btnDismiss->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
	} else {
		btnDismiss->setIcon(MessageWidget->style()->standardIcon(
			QStyle::SP_DialogCloseButton, nullptr, btnDismiss));
	}

	hboxFrame->addWidget(btnDismiss);
	hboxFrame->setAlignment(btnDismiss, Qt::AlignTop);

	hboxMain->addWidget(content);

	QMetaObject::connectSlotsByName(MessageWidget);
}

/**
 * Set the icon.
 * @param icon Icon to set
 */
void MessageWidgetPrivate::setIcon(MessageWidget::MsgIcon icon)
{
	if (icon < MessageWidget::ICON_NONE || icon >= MessageWidget::ICON_MAX)
		icon = MessageWidget::ICON_NONE;
	if (this->icon == icon)
		return;
	this->icon = icon;

	QStyle::StandardPixmap standardPixmap;
	bool hasStandardPixmap = false;
	switch (icon) {
		case MessageWidget::ICON_NONE:
		default:
			hasStandardPixmap = false;
			break;
		case MessageWidget::ICON_CRITICAL:
			standardPixmap = QStyle::SP_MessageBoxCritical;
			hasStandardPixmap = true;
			break;
		case MessageWidget::ICON_QUESTION:
			standardPixmap = QStyle::SP_MessageBoxQuestion;
			hasStandardPixmap = true;
			break;
		case MessageWidget::ICON_WARNING:
			standardPixmap = QStyle::SP_MessageBoxWarning;
			hasStandardPixmap = true;
			break;
		case MessageWidget::ICON_INFORMATION:
			standardPixmap = QStyle::SP_MessageBoxInformation;
			hasStandardPixmap = true;
			break;
	}

	Q_Q(MessageWidget);
	if (!hasStandardPixmap) {
		ui.lblIcon->setVisible(false);
	} else {
		QIcon icon = q->style()->standardIcon(standardPixmap, nullptr, ui.lblIcon);
		ui.lblIcon->setPixmap(icon.pixmap(iconSz, iconSz));
		ui.lblIcon->setVisible(true);
	}

	if (q->isVisible()) {
		q->update();
	}
}

/**
 * Calculate the best height for the widget.
 * @return Best height
 */
int MessageWidgetPrivate::calcBestHeight(void) const
{
	int height = ui.content->sizeHint().height();
	height += ui.hboxMain->contentsMargins().top();
	height += ui.hboxMain->contentsMargins().bottom();
	height += ui.hboxFrame->contentsMargins().top();
	height += ui.hboxFrame->contentsMargins().bottom();
	return height;
}

/** MessageWidget **/

MessageWidget::MessageWidget(QWidget *parent)
	: super(parent)
	, d_ptr(new MessageWidgetPrivate(this))
{
	Q_D(MessageWidget);
	d->ui.setupUi(this);
	d->setIcon(d->icon);

	// Connect the timer signal.
	QObject::connect(d->tmrTimeout, &QTimer::timeout,
		this, &MessageWidget::tmrTimeout_timeout);

	// Connect the timeline signals.
	connect(d->timeLine, &QTimeLine::valueChanged,
		this, &MessageWidget::timeLineChanged_slot);
	connect(d->timeLine, &QTimeLine::finished,
		this, &MessageWidget::timeLineFinished_slot);
}

MessageWidget::~MessageWidget()
{
	Q_D(MessageWidget);
	delete d;
}

/** Events. **/

/**
 * Paint event.
 * @param event QPaintEvent.
 */
void MessageWidget::paintEvent(QPaintEvent *event)
{
	// Call the superclass paintEvent first.
	super::paintEvent(event);

	// Drawing rectangle should be this->rect(),
	// minus one pixel width and height.
	QRect drawRect(this->rect());
	drawRect.setWidth(drawRect.width() - 1);
	drawRect.setHeight(drawRect.height() - 1);

	// Determine the background color based on icon.
	QBrush bgColor;
	Q_D(MessageWidget);
	switch (d->icon) {
		case ICON_CRITICAL:
			bgColor = QColor(MessageWidgetPrivate::colorCritical);
			break;
		case ICON_QUESTION:
			bgColor = QColor(MessageWidgetPrivate::colorQuestion);
			break;
		case ICON_WARNING:
			bgColor = QColor(MessageWidgetPrivate::colorWarning);
			break;
		case ICON_INFORMATION:
			bgColor = QColor(MessageWidgetPrivate::colorInformation);
			break;
		default:
			// FIXME: This looks terrible.
			bgColor = QColor(Qt::white);
			break;
	}

	if (bgColor != QColor(Qt::white)) {
		QPainter painter(this);
		painter.setPen(QColor(Qt::black));
		painter.setBrush(bgColor);
		painter.drawRoundedRect(drawRect, 5.0, 5.0);
	}
}

/**
 * Show event.
 * @param event QShowEent.
 */
void MessageWidget::showEvent(QShowEvent *event)
{
	// Call the superclass showEvent.
	super::showEvent(event);

	Q_D(MessageWidget);
	d->timeout = false;
	if (d->animateOnShow) {
		// Start the animation.
		d->animateOnShow = false;
		d->timeLine->setDirection(QTimeLine::Forward);
		if (d->timeLine->state() == QTimeLine::NotRunning) {
			d->timeLine->start();
		}
	}
}

/** Slots. **/

/**
 * Show a message.
 * @param msg Message text (supports Qt RichText formatting)
 * @param icon Icon
 * @param timeout Timeout, in milliseconds (0 for no timeout)
 * @param closeOnDestroy Close the message when the specified QObject is destroyed.
 */
void MessageWidget::showMessage(const QString &msg, MsgIcon icon, int timeout, QObject *closeOnDestroy)
{
	Q_D(MessageWidget);
	d->ui.lblMessage->setText(msg);
	d->setIcon(icon);

	// Set up the timer.
	d->tmrTimeout->stop();
	d->tmrTimeout->setInterval(timeout);

	// Close the widget when the specified QObject is destroyed.
	if (closeOnDestroy) {
		connect(closeOnDestroy, &QObject::destroyed,
			this, &MessageWidget::on_btnDismiss_clicked,
			Qt::UniqueConnection);
	}

	// If the widget is already visible, just update it.
	if (this->isVisible()) {
		update();
		return;
	}

	// Do an animated show.
	this->showAnimated();
}

/**
 * Show the MessageWidget using animation.
 */
void MessageWidget::showAnimated(void)
{
	Q_D(MessageWidget);
	setFixedHeight(0);
	d->ui.content->setGeometry(0, 0, width(), d->calcBestHeight());
	d->animateOnShow = true;
	d->tmrTimeout->stop();
	this->show();
}

/**
 * Hide the MessageWidget using animation.
 */
void MessageWidget::hideAnimated(void)
{
	Q_D(MessageWidget);

	// Start the animation.
	d->animateOnShow = false;
	d->tmrTimeout->stop();
	d->timeLine->setDirection(QTimeLine::Backward);
	if (d->timeLine->state() == QTimeLine::NotRunning) {
		d->timeLine->start();
	}
}

/**
 * Message timer has expired.
 */
void MessageWidget::tmrTimeout_timeout(void)
{
	// Hide the message using animation.
	Q_D(MessageWidget);
	d->timeout = true;
	this->hideAnimated();
}

/**
 * Animation timeline has changed.
 * @param value Timeline value
 */
void MessageWidget::timeLineChanged_slot(qreal value)
{
	Q_D(MessageWidget);
	this->setFixedHeight(qMin(value, qreal(1.0)) * d->calcBestHeight());
}

/**
 * Animation timeline has finished.
 */
void MessageWidget::timeLineFinished_slot(void)
{
	Q_D(MessageWidget);
	if (d->timeLine->direction() == QTimeLine::Forward) {
		// Make sure the widget is full-size.
		this->setFixedHeight(d->calcBestHeight());

		// Start the timeout timer, if specified.
		d->timeout = false;
		if (d->tmrTimeout->interval() > 0)
			d->tmrTimeout->start();
	} else {
		// Message is dismissed.
		// NOTE: This used to call this->hide(),
		// but that causes a deadlock when
		// used with MessageWidgetStack.
		d->tmrTimeout->stop();
		emit dismissed(d->timeout);
	}
}

/**
 * "Dismiss" button has been clicked.
 */
void MessageWidget::on_btnDismiss_clicked(void)
{
	// Hide the message using animation.
	Q_D(MessageWidget);
	if (d->timeLine->state() == QTimeLine::NotRunning) {
		d->tmrTimeout->stop();
		d->timeout = false;
		this->hideAnimated();
	}
}
