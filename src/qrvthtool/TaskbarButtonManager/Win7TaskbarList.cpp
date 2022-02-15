/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * Win7TaskbarList.cpp: Windows 7 ITaskBarList3 implementation.            *
 *                                                                         *
 * Copyright (c) 2013-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "Win7TaskbarList.hpp"

// Qt includes.
#include <QWidget>

// Windows.
// TODO: Compatibility for older Windows SDKs that
// don't have ITaskbarList3?
#include <windows.h>
#include <shobjidl.h>

#include "TaskbarButtonManager_p.hpp"
class Win7TaskbarListPrivate : public TaskbarButtonManagerPrivate
{
	public:
		explicit Win7TaskbarListPrivate(Win7TaskbarList *const q);
		virtual ~Win7TaskbarListPrivate();

	private:
		typedef TaskbarButtonManagerPrivate super;
		Q_DECLARE_PUBLIC(Win7TaskbarList)
		Q_DISABLE_COPY(Win7TaskbarListPrivate);

	public:
		/**
		 * Update the ITaskbarList3 status.
		 */
		void update(void);

		/**
		 * Release the ITaskbarList3 object.
		 */
		void close(void);

	private:
		// Win7TaskbarList.
		ITaskbarList3 *w7taskbar;
};

Win7TaskbarListPrivate::Win7TaskbarListPrivate(Win7TaskbarList *const q)
	: super(q)
	, w7taskbar(nullptr)
{ }

Win7TaskbarListPrivate::~Win7TaskbarListPrivate()
{
	if (w7taskbar) {
		w7taskbar->Release();
	}
}

/**
 * Release the ITaskbarList3 object.
 */
void Win7TaskbarListPrivate::close(void)
{
	if (w7taskbar) {
		w7taskbar->Release();
		w7taskbar = nullptr;
	}
}

/**
 * Update the ITaskbarList3 status.
 */
void Win7TaskbarListPrivate::update(void)
{
	if (!window || !w7taskbar)
		return;

	// TODO: winId(), or effectiveWinId()?
	HWND hWnd = reinterpret_cast<HWND>(window->winId());
	if (!hWnd)
		return;

	// Update the ITaskbarList3.
	// TODO: Error checking; also 'state' value.
	Q_Q(Win7TaskbarList);
	int curVal = q->progressBarValue();
	int curMax = q->progressBarMax();
	if (curVal < 0 || curMax <= 0) {
		// No operation in progress.
		w7taskbar->SetProgressState(hWnd, TBPF_NOPROGRESS);
	} else {
		// Operation in progress.
		w7taskbar->SetProgressValue(hWnd, curVal, curMax);
	}
}

/** Win7TaskbarList **/

Win7TaskbarList::Win7TaskbarList(QObject* parent)
	: super(new Win7TaskbarListPrivate(this), parent)
{ }

/**
 * Is this TaskbarButtonManager usable?
 * @return True if usable; false if not.
 */
bool Win7TaskbarList::IsUsable(void)
{
	bool success = false;

	// Attempt to instantiate an ITaskbarList3.
	ITaskbarList3 *obj;
	HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
			IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&obj));
	if (SUCCEEDED(hr)) {
		hr = obj->HrInit();
		if (SUCCEEDED(hr)) {
			// ITaskbarList3 is supported.
			success = true;
		}
		obj->Release();
	}

	return success;
}

/**
 * Set the window this TaskbarButtonManager should manage.
 * This must be a top-level window in order to work properly.
 *
 * Subclasses should reimplement this function if special
 * initialization is required to set up the taskbar button.
 *
 * TODO: Make a separate protected function that setWindow() calls?
 *
 * @param window Window.
 */
void Win7TaskbarList::setWindow(QWidget *window)
{
	Q_D(Win7TaskbarList);
	if (d->window == window)
		return;

	// Delete the existing ITaskbarList3.
	d->close();

	// Set the new window.
	super::setWindow(window);

	if (window != nullptr) {
		// Instantiate the ITaskbarList3.
		// Reference: http://nicug.blogspot.com/2011/03/windows-7-taskbar-extensions-in-qt.html
		HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
				IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&d->w7taskbar));
		if (SUCCEEDED(hr)) {
			hr = d->w7taskbar->HrInit();
			if (FAILED(hr)) {
				// Failed to initialize the ITaskbarList3.
				d->w7taskbar->Release();
				d->w7taskbar = nullptr;
			}
		} else {
			// Failed to get an ITaskbarList3.
			// Ensure w7taskbar is NULL.
			d->w7taskbar = nullptr;
		}

		// Update the ITaskbarList3.
		d->update();
	}
}

/**
 * Update the taskbar button.
 */
void Win7TaskbarList::update(void)
{
	Q_D(Win7TaskbarList);
	d->update();
}
