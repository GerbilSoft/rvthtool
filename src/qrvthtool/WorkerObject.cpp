/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * WorkerObject.cpp: Worker object for extract/import/etc.                 *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "WorkerObject.hpp"

// librvth
#include "librvth/nhcd_structs.h"

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>

// Qt includes.
#include <QtCore/QFileInfo>

/** WorkerObjectPrivate **/

class WorkerObjectPrivate
{
public:
	explicit WorkerObjectPrivate(WorkerObject *q)
		: q_ptr(q)
		, rvth(nullptr)
		, bank(~0U)
		, flags(0U)
		, recryption_key(-1)
		, cancel(false)
	{ }

protected:
	WorkerObject *const q_ptr;
	Q_DECLARE_PUBLIC(WorkerObject)
private:
	Q_DISABLE_COPY(WorkerObjectPrivate)

public:
	RvtH *rvth;
	QString gcmFilename;
	QString gcmFilenameOnly;

	unsigned int bank;
	unsigned int flags;
	int recryption_key;

	// Cancel the current process.
	// TODO: Mutex?
	bool cancel;

public:
	/**
	 * RVT-H progress callback.
	 * @param state		[in] Current progress
	 * @param userdata	[in] User data specified when calling the RVT-H function
	 * @return True to continue; false to abort.
	 */
	static bool progress_callback(const RvtH_Progress_State *state, void *userdata);
};

/** WorkerObjectPrivate **/

/**
 * RVT-H progress callback.
 * @param state		[in] Current progress
 * @param userdata	[in] User data specified when calling the RVT-H function
 * @return True to continue; false to abort.
 */
bool WorkerObjectPrivate::progress_callback(const RvtH_Progress_State *state, void *userdata)
{
	WorkerObjectPrivate *const d = static_cast<WorkerObjectPrivate*>(userdata);
	WorkerObject *const q = d->q_ptr;

	// TODO: Don't show the bank number if the source image is a standalone disc image.
	static constexpr uint32_t MEGABYTE = (1048576U / LBA_SIZE);
	QString text;
	switch (state->type) {
		case RVTH_PROGRESS_EXTRACT:
			text = WorkerObject::tr("Extracting Bank %1 to %2: %L3 MiB / %L4 MiB copied...")
				.arg(d->bank+1)
				.arg(d->gcmFilenameOnly)
				.arg(state->lba_processed / MEGABYTE)
				.arg(state->lba_total / MEGABYTE);
			break;
		case RVTH_PROGRESS_IMPORT:
			text = WorkerObject::tr("Importing from %1 to Bank %2: %L3 MiB / %L4 MiB copied...")
				.arg(d->gcmFilenameOnly)
				.arg(d->bank+1)
				.arg(state->lba_processed / MEGABYTE)
				.arg(state->lba_total / MEGABYTE);
			break;
		case RVTH_PROGRESS_RECRYPT:
			if (state->lba_total <= 1) {
				// TODO: Encryption types?
				if (state->lba_processed == 0) {
					text = WorkerObject::tr("Recrypting the ticket(s) and TMD(s)...");
				}
			} else {
				// TODO: This doesn't seem to be used yet...
				text = WorkerObject::tr("Recrypting Bank %1 to %2: %L3 MiB / %L4 MiB copied...")
					.arg(d->bank+1)
					.arg(d->gcmFilenameOnly)
					.arg(state->lba_processed / MEGABYTE)
					.arg(state->lba_total / MEGABYTE);
			}
			break;
		default:
			// FIXME
			assert(false);
			return false;
	}

	// Update the progress bar.
	if (state->type != RVTH_PROGRESS_RECRYPT) {
		// Progress is valid.
		emit q->updateStatus(text,
			static_cast<int>(state->lba_processed),
			static_cast<int>(state->lba_total));
	} else {
		// Progress is not useful here.
		// Specify -1 for the values.
		emit q->updateStatus(text, -1, -1);
	}

	// Return `true` to continue.
	// If `cancel` is set, return `false` to cancel.
	return !d->cancel;
}

/** WorkerObject **/

WorkerObject::WorkerObject(QObject *parent)
	: super(parent)
	, d_ptr(new WorkerObjectPrivate(this))
{ }

/** Properties **/

/**
 * Get the RVT-H object.
 * @return RVT-H object
 */
RvtH *WorkerObject::rvth(void) const
{
	Q_D(const WorkerObject);
	return d->rvth;
}

/**
 * Set the RVT-H object.
 * @param rvth RVT-H object
 */
void WorkerObject::setRvtH(RvtH *rvth)
{
	Q_D(WorkerObject);
	d->rvth = rvth;
}

/**
 * Get the RVT-H bank number.
 * @return RVT-H bank number (~0 if not set)
 */
unsigned int WorkerObject::bank(void) const
{
	Q_D(const WorkerObject);
	return d->bank;
}

/**
 * Set the RVT-H bank number.
 * @param bank Bank number
 */
void WorkerObject::setBank(unsigned int bank)
{
	Q_D(WorkerObject);
	d->bank = bank;
}

/**
 * Get the GCM filename.
 * For extract: This will be the destination GCM.
 * For import: This will be the source GCM.
 * @return GCM filename
 */
QString WorkerObject::gcmFilename(void) const
{
	Q_D(const WorkerObject);
	return d->gcmFilename;
}

/**
 * Set the GCM filename.
 * For extract: This will be the destination GCM.
 * For import: This will be the source GCM.
 * @param filename GCM filename
 */
void WorkerObject::setGcmFilename(QString filename)
{
	Q_D(WorkerObject);
	d->gcmFilename = filename;
	d->gcmFilenameOnly = QFileInfo(filename).fileName();
}

/**
 * Get the recryption key.
 * @return Recryption key (-1 for no recryption)
 */
int WorkerObject::recryptionKey(void) const
{
	Q_D(const WorkerObject);
	return d->recryption_key;
}

/**
 * Set the recryption key.
 * @param recryption_key Recryption key (-1 for no recryption)
 */
void WorkerObject::setRecryptionKey(int recryption_key)
{
	Q_D(WorkerObject);
	d->recryption_key = recryption_key;
}

/**
 * Get the flags. (operation-specific)
 * @return Flags
 */
unsigned int WorkerObject::flags(void) const
{
	Q_D(const WorkerObject);
	return d->flags;
}

/**
* Set the flags. (operation-specific)
* @param flags Flags
*/
void WorkerObject::setFlags(unsigned int flags)
{
	Q_D(WorkerObject);
	d->flags = flags;
}

/** Worker functions **/

/**
 * Cancel the current process.
 */
void WorkerObject::cancel(void)
{
	Q_D(WorkerObject);
	d->cancel = true;
}

/**
 * Start an extraction process.
 *
 * The following properties must be set before calling this function:
 * - rvth
 * - bank
 * - gcmFilename
 *
 * Optional parameters:
 * - recryptionKey (default is -1)
 * - flags (default is 0)
 */
void WorkerObject::doExtract(void)
{
	// NOTE: Callback is set to use the private class.
	const QString fn(QLatin1String("doImport"));

	Q_D(WorkerObject);
	if (!d->rvth) {
		emit finished(tr("%1() ERROR: rvth object is not set.").arg(fn), -EINVAL);
		return;
	} else if (d->bank >= d->rvth->bankCount()) {
		if (d->bank == ~0U) {
			emit finished(tr("%1() ERROR: Bank number is not set.").arg(fn), -EINVAL);
		} else {
			emit finished(tr("%1() ERROR: Bank number %2 is out of range.")
				.arg(fn).arg(d->bank+1), -ERANGE);
		}
		return;
	} else if (d->gcmFilename.isEmpty()) {
		emit finished(tr("%1() ERROR: gcmFilename is not set.").arg(fn), -EINVAL);
		return;
	}

	d->cancel = false;
#ifdef _WIN32
	int ret = d->rvth->extract(d->bank,
		reinterpret_cast<const wchar_t*>(d->gcmFilename.utf16()),
		d->recryption_key, d->flags, d->progress_callback, d);
#else /* !_WIN32 */
	int ret = d->rvth->extract(d->bank,
		d->gcmFilename.toUtf8().constData(),
		d->recryption_key, d->flags, d->progress_callback, d);
#endif /* _WIN32 */

	if (ret == 0) {
		// Successfully extracted.
		emit finished(tr("Bank %1 extracted into %2 successfully.")
			.arg(d->bank+1).arg(d->gcmFilenameOnly), ret);
	} else if (ret == -ECANCELED) {
		// Operation cancelled.
		emit finished(tr("Extraction operation cancelled."), ret);
	} else {
		// An error occurred...
		emit finished(tr("ERROR extracting Bank %1 into %2")
			.arg(d->bank+1).arg(d->gcmFilenameOnly), ret);
	}
}

/**
 * Start an import process.
 *
 * The following properties must be set before calling this function:
 * - rvth
 * - bank
 * - gcmFilename
 */
void WorkerObject::doImport(void)
{
	// NOTE: Callback is set to use the private class.
	const QString fn(QLatin1String("doImport"));

	Q_D(WorkerObject);
	if (!d->rvth) {
		emit finished(tr("%1() ERROR: rvth object is not set.").arg(fn), -EINVAL);
		return;
	} else if (d->bank >= d->rvth->bankCount()) {
		if (d->bank == ~0U) {
			emit finished(tr("%1() ERROR: Bank number is not set.").arg(fn), -EINVAL);
		} else {
			emit finished(tr("%1() ERROR: Bank number %2 is out of range.")
				.arg(fn).arg(d->bank+1), -ERANGE);
		}
		return;
	} else if (d->gcmFilename.isEmpty()) {
		emit finished(tr("%1() ERROR: gcmFilename is not set.").arg(fn), -EINVAL);
		return;
	}

	d->cancel = false;
#ifdef _WIN32
	int ret = d->rvth->import(d->bank,
		reinterpret_cast<const wchar_t*>(d->gcmFilename.utf16()),
		d->progress_callback, d);
#else /* !_WIN32 */
	int ret = d->rvth->import(d->bank,
		d->gcmFilename.toUtf8().constData(),
		d->progress_callback, d);
#endif /* _WIN32 */

	if (ret == 0) {
		// Successfully extracted.
		emit finished(tr("%1 imported into Bank %2 successfully.")
			.arg(d->gcmFilenameOnly).arg(d->bank+1), ret);
	} else if (ret == -ECANCELED) {
		// Operation cancelled.
		emit finished(tr("Import operation cancelled."), ret);
	} else {
		// An error occurred...
		emit finished(tr("ERROR importing %1 into Bank %2")
			.arg(d->gcmFilenameOnly).arg(d->bank+1), ret);
	}
}
