/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QItemView_text.cpp: QItemView subclasses that show a message if no      *
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

#include "QItemView_Text.hpp"

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
QStyleOptionViewItem klass##_Text::viewOptions(void) const \
{ \
	QStyleOptionViewItem option = super::viewOptions(); \
	option.decorationPosition = m_decorationPosition; \
	return option; \
} \
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
