/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QTreeViewOpt.hpp: QTreeView with drawing optimizations.                 *
 * Specifically, don't update rows that are offscreen.                     *
 *                                                                         *
 * Copyright (c) 2013-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

// Qt includes and classes.
#include <QTreeView>
class QKeyEvent;
class QFocusEvent;

class QTreeViewOpt : public QTreeView
{
Q_OBJECT
typedef QTreeView super;

public:
	explicit QTreeViewOpt(QWidget *parent = nullptr);

private:
	Q_DISABLE_COPY(QTreeViewOpt);

public:
	void dataChanged(const QModelIndex &topLeft,
		const QModelIndex &bottomRight,
		const QVector<int> &roles = QVector<int>()) final;

protected slots:
	void showColumnContextMenu(const QPoint &point);
};
