/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QItemView_text.hpp: QItemView subclasses that show a message if no      *
 * items are present.                                                      *
 *                                                                         *
 * Copyright (c) 2013-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// Reference: https://stackoverflow.com/questions/20765547/qlistview-show-text-when-list-is-empty

#pragma once

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
		// FIXME: Qt6's viewOptions() is final.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
		QStyleOptionViewItem viewOptions(void) const final;
#endif

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
		// FIXME: Qt6's viewOptions() is final.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
		QStyleOptionViewItem viewOptions(void) const final;
#endif

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
		// FIXME: Qt6's viewOptions() is final.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
		QStyleOptionViewItem viewOptions(void) const final;
#endif

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
		// FIXME: Qt6's viewOptions() is final.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
		QStyleOptionViewItem viewOptions(void) const final;
#endif

	public:
		inline QString noItemText(void) const { return m_noItemText; }
		void setNoItemText(const QString &noItemText);

		inline QStyleOptionViewItem::Position decorationPosition(void) const { return m_decorationPosition; }
		void setDecorationPosition(QStyleOptionViewItem::Position position);
};
