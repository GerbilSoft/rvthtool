/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QItemView_text.cpp: QItemView subclasses that show a message if no      *
 * items are present.                                                      *
 *                                                                         *
 * Copyright (c) 2013-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "QItemView_Text.hpp"

// FIXME: Qt6's QListView::viewOptions() is final.
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#  define IMPL_VIEWOPTIONS(klass)
#else
#  define IMPL_VIEWOPTIONS(klass) \
QStyleOptionViewItem klass##_Text::viewOptions(void) const \
{ \
	QStyleOptionViewItem option = super::viewOptions(); \
	option.decorationPosition = m_decorationPosition; \
	return option; \
}
#endif

#define QItemView_Text_Functions(klass) \
void klass##_Text::paintEvent(QPaintEvent *e) \
{ \
	super::paintEvent(e); \
	if ((!model() || model()->rowCount(rootIndex()) == 0) && \
	    !m_noItemText.isEmpty()) \
	{ \
		/* The view is empty. */ \
		QPainter p(this->viewport()); \
		p.drawText(rect(), Qt::AlignCenter, m_noItemText); \
	} \
} \
\
IMPL_VIEWOPTIONS(klass) \
\
void klass##_Text::setNoItemText(const QString &noItemText) \
{ \
	if (m_noItemText == noItemText) \
		return; \
\
	m_noItemText = noItemText; \
	if (!model() || model()->rowCount(rootIndex()) == 0) { \
		/* No items. Need to update. */ \
		update(); \
	} \
} \
\
void klass##_Text::setDecorationPosition(QStyleOptionViewItem::Position position) \
{ \
	if (m_decorationPosition == position) \
		return; \
\
	m_decorationPosition = position; \
	if (model() && model()->rowCount(rootIndex()) > 0) { \
		/* We have items. Need to update. */ \
		update(); \
	} \
}

QItemView_Text_Functions(QListView)
QItemView_Text_Functions(QListWidget)
QItemView_Text_Functions(QTreeView)
QItemView_Text_Functions(QTreeWidget)
