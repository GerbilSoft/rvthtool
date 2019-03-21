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
	Q_PROPERTY(QStyleOptionViewItem::Position decorationPosition READ decorationPosition WRITE setDecorationPosition)
	typedef QListView super;

	public:
		QListView_Text(QWidget *parent = nullptr)
			: super(parent)
			, m_decorationPosition(QStyleOptionViewItem::Left) { }

	private:
		Q_DISABLE_COPY(QListView_Text)
		QString m_noItemText;
		QStyleOptionViewItem::Position m_decorationPosition;

	protected:
		void paintEvent(QPaintEvent *e) final;
		QStyleOptionViewItem viewOptions(void) const final;

	public:
		inline QString noItemText(void) const { return m_noItemText; }
		void setNoItemText(const QString &noItemText);

		inline QStyleOptionViewItem::Position decorationPosition(void) const { return m_decorationPosition; }
		void setDecorationPosition(QStyleOptionViewItem::Position position);
};

class QListWidget_Text : public QListWidget
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	Q_PROPERTY(QStyleOptionViewItem::Position decorationPosition READ decorationPosition WRITE setDecorationPosition)
	typedef QListWidget super;

	public:
		QListWidget_Text(QWidget *parent = nullptr)
			: super(parent)
			, m_decorationPosition(QStyleOptionViewItem::Left) { }

	private:
		Q_DISABLE_COPY(QListWidget_Text)
		QString m_noItemText;
		QStyleOptionViewItem::Position m_decorationPosition;

	protected:
		void paintEvent(QPaintEvent *e) final;
		QStyleOptionViewItem viewOptions(void) const final;

	public:
		inline QString noItemText(void) const { return m_noItemText; }
		void setNoItemText(const QString &noItemText);

		inline QStyleOptionViewItem::Position decorationPosition(void) const { return m_decorationPosition; }
		void setDecorationPosition(QStyleOptionViewItem::Position position);
};

class QTreeView_Text : public QTreeView
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	Q_PROPERTY(QStyleOptionViewItem::Position decorationPosition READ decorationPosition WRITE setDecorationPosition)
	typedef QTreeView super;

	public:
		QTreeView_Text(QWidget *parent = nullptr)
			: super(parent)
			, m_decorationPosition(QStyleOptionViewItem::Left) { }

	private:
		Q_DISABLE_COPY(QTreeView_Text)
		QString m_noItemText;
		QStyleOptionViewItem::Position m_decorationPosition;

	protected:
		void paintEvent(QPaintEvent *e) final;
		QStyleOptionViewItem viewOptions(void) const final;

	public:
		inline QString noItemText(void) const { return m_noItemText; }
		void setNoItemText(const QString &noItemText);

		inline QStyleOptionViewItem::Position decorationPosition(void) const { return m_decorationPosition; }
		void setDecorationPosition(QStyleOptionViewItem::Position position);
};

class QTreeWidget_Text : public QTreeWidget
{
	Q_OBJECT
	Q_PROPERTY(QString noItemText READ noItemText WRITE setNoItemText)
	Q_PROPERTY(QStyleOptionViewItem::Position decorationPosition READ decorationPosition WRITE setDecorationPosition)
	typedef QTreeWidget super;

	public:
		QTreeWidget_Text(QWidget *parent = nullptr)
			: super(parent)
			, m_decorationPosition(QStyleOptionViewItem::Left) { }

	private:
		Q_DISABLE_COPY(QTreeWidget_Text)
		QString m_noItemText;
		QStyleOptionViewItem::Position m_decorationPosition;

	protected:
		void paintEvent(QPaintEvent *e) final;
		QStyleOptionViewItem viewOptions(void) const final;

	public:
		inline QString noItemText(void) const { return m_noItemText; }
		void setNoItemText(const QString &noItemText);

		inline QStyleOptionViewItem::Position decorationPosition(void) const { return m_decorationPosition; }
		void setDecorationPosition(QStyleOptionViewItem::Position position);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WIDGETS_QITEMVIEW_TEXT_HPP__ */
