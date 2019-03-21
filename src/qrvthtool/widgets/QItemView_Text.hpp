/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QItemView_text.hpp: QItemView subclasses that show a message if no      *
 * items are present.                                                      *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
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

// Reference: https://stackoverflow.com/questions/20765547/qlistview-show-text-when-list-is-empty

#ifndef __RVTHTOOL_QRVTHTOOL_WIDGETS_QITEMVIEW_TEXT_HPP__
#define __RVTHTOOL_QRVTHTOOL_WIDGETS_QITEMVIEW_TEXT_HPP__

#include <QListView>
#include <QListWidget>
#include <QTreeView>
#include <QTreeWidget>

#include <QPainter>

class QListView_Text : public QListView
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	typedef QListView super;

	public:
		QListView_Text(QWidget *parent = nullptr)
			: QListView(parent) { }

	private:
		Q_DISABLE_COPY(QListView_Text)
		QString m_noItemText;

	protected:
		void paintEvent(QPaintEvent *e)
		{
			QListView::paintEvent(e);
			if ((!model() || model()->rowCount(rootIndex()) == 0) &&
			    !m_noItemText.isEmpty())
			{
				/* The view is empty. */
				QPainter p(this->viewport());
				p.drawText(rect(), Qt::AlignCenter, m_noItemText);
			}
		}

		QStyleOptionViewItem viewOptions(void) const final
		{
			// TODO: Make this an adjustable parameter.
			// Display the icon on the left side of the text,
			// similar to Windows Explorer's "Tiles" view.
			QStyleOptionViewItem option = super::viewOptions();
			option.decorationPosition = QStyleOptionViewItem::Left;
			return option;
		}

	public:
		inline QString noItemText(void) const { return m_noItemText; }

		void setNoItemText(const QString &noItemText)
		{
			if (m_noItemText == noItemText)
				return;

			m_noItemText = noItemText;
			if (!model() || model()->rowCount(rootIndex()) == 0) {
				/* No items. Need to update. */
				update();
			}
		}
};

class QListWidget_Text : public QListWidget
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	typedef QListWidget super;

	public:
		QListWidget_Text(QWidget *parent = nullptr)
			: QListWidget(parent) { }

	private:
		Q_DISABLE_COPY(QListWidget_Text)
		QString m_noItemText;

	protected:
		void paintEvent(QPaintEvent *e)
		{
			QListWidget::paintEvent(e);
			if ((!model() || model()->rowCount(rootIndex()) == 0) &&
			    !m_noItemText.isEmpty())
			{
				/* The view is empty. */
				QPainter p(this->viewport());
				p.drawText(rect(), Qt::AlignCenter, m_noItemText);
			}
		}

		QStyleOptionViewItem viewOptions(void) const final
		{
			// TODO: Make this an adjustable parameter.
			// Display the icon on the left side of the text,
			// similar to Windows Explorer's "Tiles" view.
			QStyleOptionViewItem option = super::viewOptions();
			option.decorationPosition = QStyleOptionViewItem::Left;
			return option;
		}

	public:
		inline QString noItemText(void) const { return m_noItemText; }

		void setNoItemText(const QString &noItemText)
		{
			if (m_noItemText == noItemText)
				return;

			m_noItemText = noItemText;
			if (!model() || model()->rowCount(rootIndex()) == 0) {
				/* No items. Need to update. */
				update();
			}
		}
};

class QTreeView_Text : public QTreeView
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	typedef QTreeView super;

	public:
		QTreeView_Text(QWidget *parent = nullptr)
			: QTreeView(parent) { }

	private:
		Q_DISABLE_COPY(QTreeView_Text)
		QString m_noItemText;

	protected:
		void paintEvent(QPaintEvent *e)
		{
			QTreeView::paintEvent(e);
			if ((!model() || model()->rowCount(rootIndex()) == 0) &&
			    !m_noItemText.isEmpty())
			{
				/* The view is empty. */
				QPainter p(this->viewport());
				p.drawText(rect(), Qt::AlignCenter, m_noItemText);
			}
		}

	public:
		inline QString noItemText(void) const { return m_noItemText; }

		void setNoItemText(const QString &noItemText)
		{
			if (m_noItemText == noItemText)
				return;

			m_noItemText = noItemText;
			if (!model() || model()->rowCount(rootIndex()) == 0) {
				/* No items. Need to update. */
				update();
			}
		}
};

class QTreeWidget_Text : public QTreeWidget
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	typedef QTreeWidget super;

	public:
		QTreeWidget_Text(QWidget *parent = nullptr)
			: QTreeWidget(parent) { }

	private:
		Q_DISABLE_COPY(QTreeWidget_Text)
		QString m_noItemText;

	protected:
		void paintEvent(QPaintEvent *e)
		{
			QTreeWidget::paintEvent(e);
			if ((!model() || model()->rowCount(rootIndex()) == 0) &&
			    !m_noItemText.isEmpty())
			{
				/* The view is empty. */
				QPainter p(this->viewport());
				p.drawText(rect(), Qt::AlignCenter, m_noItemText);
			}
		}

	public:
		inline QString noItemText(void) const { return m_noItemText; }

		void setNoItemText(const QString &noItemText)
		{
			if (m_noItemText == noItemText)
				return;

			m_noItemText = noItemText;
			if (!model() || model()->rowCount(rootIndex()) == 0) {
				/* No items. Need to update. */
				update();
			}
		}
};

#endif /* __RVTHTOOL_QRVTHTOOL_WIDGETS_QITEMVIEW_TEXT_HPP__ */
