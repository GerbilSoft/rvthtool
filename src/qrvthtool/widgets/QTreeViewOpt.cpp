/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * QTreeViewOpt.cpp: QTreeView with drawing optimizations.                 *
 * Specifically, don't update rows that are offscreen.                     *
 *                                                                         *
 * Copyright (c) 2013-2018 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "QTreeViewOpt.hpp"

// Qt classes.
#include <QHeaderView>
#include <QMenu>
#include <QAction>

QTreeViewOpt::QTreeViewOpt(QWidget *parent)
	: super(parent)
{
	// Connect the signal for hiding/showing columns.
	this->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this->header(), &QHeaderView::customContextMenuRequested,
		this, &QTreeViewOpt::showColumnContextMenu);
}

/**
 * Data has changed in the item model.
 * @param topLeft	[in] Top-left item.
 * @param bottomRight	[in] Bottom-right item.
 * @param roles		[in] (Qt5) Roles that have changed.
 */
void QTreeViewOpt::dataChanged(const QModelIndex &topLeft,
	const QModelIndex &bottomRight,
	const QVector<int> &roles)
{
	bool propagateEvent = true;
	// TODO: Support for checking multiple items.
	if (topLeft == bottomRight) {
		// Single item. This might be an icon animation.
		// If it is, make sure the icon is onscreen.
		QRect itemRect = this->visualRect(topLeft);

		// Get the viewport rect.
		QRect viewportRect(QPoint(0, 0), this->viewport()->size());
		if (!viewportRect.intersects(itemRect)) {
			// Item is NOT visible.
			// Don't propagate the event.
			propagateEvent = false;
		}
	}

	if (propagateEvent) {
		// Propagate the dataChanged() event.
		super::dataChanged(topLeft, bottomRight, roles);
	}
}

/**
 * Show the column context menu.
 * Based on KSysGuard's KSysGuardProcessList::showColumnContextMenu().
 * @param point Point where context menu was requested.
 */
void QTreeViewOpt::showColumnContextMenu(const QPoint &point)
{
	QMenu *menu = new QMenu();
	QAction *action;

	QHeaderView* const header = this->header();
	int index = header->logicalIndexAt(point);
	if (index >= 0) {
		// Column is selected. Add an option to hide it.
		action = new QAction(menu);
		// Set data to negative index (minus 1) to hide a column,
		// and positive index to show a column.
		action->setData(-index - 1);
		action->setText(tr("Hide Column '%1'")
				.arg(this->model()->headerData(index, Qt::Horizontal, Qt::DisplayRole).toString()));
		menu->addAction(action);

		if (this->header()->sectionsHidden())
			menu->addSeparator();
	}

	// Add options to show hidden columns.
	if (header->sectionsHidden()) {
		int num_sections = this->model()->columnCount();
		for (int i = 0; i < num_sections; i++) {
			if (header->isSectionHidden(i)) {
				action = new QAction(menu);
				action->setText(tr("Show Column '%1'")
					.arg(this->model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString()));
				// Set data to negative index (minus 1) to hide a column,
				// and positive index to show a column.
				action->setData(i);
				menu->addAction(action);
			}
		}
	}

	// Show the menu and wait for the user to select an option.
	QAction *result = menu->exec(header->mapToGlobal(point));
	if (!result) {
		// Menu was cancelled.
		menu->deleteLater();
		return;
	}

	// Show/hide the selected column.
	// TODO: Save column visibility settings somewhere.
	int i = result->data().toInt();
	if (i < 0) {
		this->hideColumn(-1 - i);
	} else {
		this->showColumn(i);
		this->resizeColumnToContents(i);
		this->resizeColumnToContents(this->model()->columnCount());
	}

	menu->deleteLater();
}
