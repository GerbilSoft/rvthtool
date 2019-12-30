/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * MessageWidgetStack.hpp: Message widget stack.                           *
 *                                                                         *
 * Copyright (c) 2014-2019 by David Korth.                                 *
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

#include "MessageWidgetStack.hpp"

// Qt includes.
#include <QtCore/QSet>
#include <QtWidgets/QVBoxLayout>

// Qt animation includes.
#include <QtCore/QTimeLine>

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
		QSet<MessageWidget*> messageWidgets;
};

MessageWidgetStackPrivate::MessageWidgetStackPrivate(MessageWidgetStack *q)
	: q_ptr(q)
{ }

MessageWidgetStackPrivate::~MessageWidgetStackPrivate()
{
	// NOTE: The MessageWidgets are owned by MessageWidgetStack,
	// so we don't need to delete them manually.
	messageWidgets.clear();
}

/**
 * Initialize the UI.
 * @param MessageWidgetStack MessageWidgetStack.
 */
void MessageWidgetStackPrivate::Ui_MessageWidgetStack::setupUi(QWidget *MessageWidgetStack)
{
	if (MessageWidgetStack->objectName().isEmpty())
		MessageWidgetStack->setObjectName(QLatin1String("MessageWidgetStack"));

	vboxMain = new QVBoxLayout(MessageWidgetStack);
	vboxMain->setObjectName(QLatin1String("vboxMain"));
	vboxMain->setMargin(0);

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
			if (d->messageWidgets.contains(qobject_cast<MessageWidget*>(messageWidget))) {
				d->messageWidgets.remove(qobject_cast<MessageWidget*>(messageWidget));
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
	d->messageWidgets.remove(qobject_cast<MessageWidget*>(obj));
}

