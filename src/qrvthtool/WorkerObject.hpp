/***************************************************************************
 * RVT-H Tool (qrvthtool)                                                  *
 * WorkerObject.hpp: Worker object for extract/import/etc.                 *
 *                                                                         *
 * Copyright (c) 2018-2019 by David Korth.                                 *
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

#ifndef __RVTHTOOL_QRVTHTOOL_WORKEROBJECT_HPP__
#define __RVTHTOOL_QRVTHTOOL_WORKEROBJECT_HPP__

// librvth
#include "librvth/rvth.hpp"

// Qt includes.
#include <QtCore/QObject>
class QLabel;
class QProgressBar;

class WorkerObjectPrivate;
class WorkerObject : public QObject
{
	Q_OBJECT
	typedef QObject super;

	Q_PROPERTY(RvtH* rvth READ rvth WRITE setRvtH)
	Q_PROPERTY(unsigned int bank READ bank WRITE setBank)
	Q_PROPERTY(QString gcmFilename READ gcmFilename WRITE setGcmFilename)
	Q_PROPERTY(int recryptionKey READ recryptionKey WRITE setRecryptionKey)
	
	public:
		explicit WorkerObject(QObject *parent = nullptr);

	protected:
		WorkerObjectPrivate *const d_ptr;
		Q_DECLARE_PRIVATE(WorkerObject)
	private:
		Q_DISABLE_COPY(WorkerObject)

	public:
		/** Properties **/

		/**
		 * Get the RVT-H object.
		 * @return RVT-H object.
		 */
		RvtH *rvth(void) const;

		/**
		 * Set the RVT-H object.
		 * @param rvth RVT-H object.
		 */
		void setRvtH(RvtH *rvth);

		/**
		 * Get the RVT-H bank number.
		 * @return RVT-H bank number. (~0 if not set)
		 */
		unsigned int bank(void) const;

		/**
		 * Set the RVT-H bank number.
		 * @param bank Bank number.
		 */
		void setBank(unsigned int bank);

		/**
		 * Get the GCM filename.
		 * For extract: This will be the destination GCM.
		 * For import: This will be the source GCM.
		 * @return GCM filename.
		 */
		QString gcmFilename(void) const;

		/**
		 * Set the GCM filename.
		 * For extract: This will be the destination GCM.
		 * For import: This will be the source GCM.
		 * @param filename GCM filename.
		 */
		void setGcmFilename(QString filename);

		/**
		 * Get the recryption key.
		 * @return Recryption key. (-1 for no recryption)
		 */
		int recryptionKey(void) const;

		/**
		 * Set the recryption key.
		 * @param recryption_key Recryption key. (-1 for no recryption)
		 */
		void setRecryptionKey(int recryption_key);

		/**
		 * Get the flags. (operation-specific)
		 * @return Flags.
		 */
		unsigned int flags(void) const;

		/**
		 * Set the flags. (operation-specific)
		 * @param flags Flags.
		 */
		void setFlags(unsigned int flags);

	signals:
		/** Signals **/

		/**
		 * Update the status bar.
		 * @param text Status bar text.
		 * @param progress_value Progress bar value. (If -1, ignore this.)
		 * @param progress_max Progress bar maximum. (If -1, ignore this.)
		 */
		void updateStatus(const QString &text, int progress_value, int progress_max);

		/**
		 * Process is finished.
		 * @param text Status bar text.
		 * @param err Error code. (0 on success)
		 */
		void finished(const QString &text, int err);

	public slots:
		/** Worker functions **/

		/**
		 * Cancel the current process.
		 */
		void cancel(void);

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
		void doExtract(void);

		/**
		 * Start an import process.
		 *
		 * The following properties must be set before calling this function:
		 * - rvth
		 * - bank
		 * - gcmFilename
		 */
		void doImport(void);
};

#endif /* __RVTHTOOL_QRVTHTOOL_WORKEROBJECT_HPP__ */
