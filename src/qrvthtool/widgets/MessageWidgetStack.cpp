/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * MessageWidgetStack.hpp: Message widget stack.                           *
 *                                                                         *
 * Copyright (c) 2014-2023 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "MessageWidgetStack.hpp"

// Qt includes
#include <QtWidgets/QVBoxLayout>

// Qt animation includes
#include <QtCore/QTimeLine>

// C++ STL classes
#include <set>

/** MessageWidgetStackPrivate **/

class MessageWidgetStackPrivate
{
	public:
		explicit MessageWidgetStackPrivate(MessageWidgetStack *q);
		~MessageWidgetStackPrivate();

	protected:
		MessageWidgetStack *const q_ptr;
		Q_DECLARE_PUBLIC(MessageWidgetStack)
	private:
		Q_DISABLE_COPY(MessageWidgetStackPrivate)

	public:
		struct Ui_MessageWidgetStack {
			QVBoxLayout *vboxMain;

			void setupUi(QWidget *MessageWidgetStack);
		};
		Ui_MessageWidgetStack ui;

		// All active MessageWidgets.
		std::set<MessageWidget*> messageWidgets;
};

MessageWidgetStackPrivate::MessageWidgetStackPrivate(MessageWidgetStack *q)
	: q_ptr(q)
{ }

MessageWidgetStackPrivate::~MessageWidgetStackPrivate()
{
	// Delete the MessageWidgets manually in order to prevent
	// deletion signals from triggering messageWidget_destroyed_slot().
	qDeleteAll(messageWidgets);
	messageWidgets.clear();
}

/**
 * Initialize the UI.
 * @param MessageWidgetStack MessageWidgetStack.
 */
void MessageWidgetStackPrivate::Ui_MessageWidgetStack::setupUi(QWidget *MessageWidgetStack)
{
	if (MessageWidgetStack->objectName().isEmpty())
		MessageWidgetStack->setObjectName(QStringLiteral("MessageWidgetStack"));

	vboxMain = new QVBoxLayout(MessageWidgetStack);
	vboxMain->setObjectName(QStringLiteral("vboxMain"));
	vboxMain->setContentsMargins(0, 0, 0, 0);

	QMetaObject::connectSlotsByName(MessageWidgetStack);
}

/** MessageWidgetStack **/

MessageWidgetStack::MessageWidgetStack(QWidget *parent)
	: super(parent)
	, d_ptr(new MessageWidgetStackPrivate(this))
{
	Q_D(MessageWidgetStack);
	d->ui.setupUi(this);
}

MessageWidgetStack::~MessageWidgetStack()
{
	Q_D(MessageWidgetStack);
	delete d;
}

/**
 * Show a message.
 * @param msg Message text. (supports Qt RichText formatting)
 * @param icon Icon.
 * @param timeout Timeout, in milliseconds. (0 for no timeout)
 * @param closeOnDestroy Close the message when the specified QObject is destroyed.
 */
void MessageWidgetStack::showMessage(const QString &msg, MessageWidget::MsgIcon icon, int timeout, QObject *closeOnDestroy)
{
	Q_D(MessageWidgetStack);
	MessageWidget *messageWidget = new MessageWidget(this);
	d->messageWidgets.insert(messageWidget);

	// Connect the signals.
	QObject::connect(messageWidget, &QObject::destroyed,
		this, &MessageWidgetStack::messageWidget_destroyed_slot);
	QObject::connect(messageWidget, &MessageWidget::dismissed,
		[d, messageWidget]() {
			// Remove and delete the widget.
			auto iter = d->messageWidgets.find(qobject_cast<MessageWidget*>(messageWidget));
			if (iter != d->messageWidgets.end()) {
				d->messageWidgets.erase(iter);
				delete messageWidget;
			}
		});

	// Show the message.
	d->ui.vboxMain->addWidget(messageWidget);
	d->ui.vboxMain->setAlignment(messageWidget, Qt::AlignTop);
	messageWidget->showMessage(msg, icon, timeout, closeOnDestroy);
}

/** Slots. **/

/**
 * A MessageWidget has been destroyed.
 * @param obj QObject that was destroyed.
 */
void MessageWidgetStack::messageWidget_destroyed_slot(QObject *obj)
{
	// Remove the widget.
	Q_D(MessageWidgetStack);
	d->messageWidgets.erase(qobject_cast<MessageWidget*>(obj));
}
