/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query.h: Query storage devices.                                         *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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

#ifndef __RVTHTOOL_LIBRVTH_QUERY_H__
#define __RVTHTOOL_LIBRVTH_QUERY_H__

#include <stdint.h>
#include "librvth/tcharx.h"

#ifdef __cplusplus
extern "C" {
#endif

// RVT-H Reader USB VID/PID.
#define RVTH_READER_VID 0x057e
#define RVTH_READER_PID 0x0304

// NOTE: STORAGE_DEVICE_DESCRIPTOR has a serial number value
// for the HDD itself, but the RVT-H Reader USB bridge
// doesn't support this query. The code is still present,
// but disabled because it isn't useful.
//#define RVTH_QUERY_ENABLE_HDD_SERIAL 1

/**
 * Scanned device entry.
 * This is a singly-linked list.
 */
typedef struct _RvtH_QueryEntry {
	struct _RvtH_QueryEntry *next;

	const TCHAR *device_name;	// Device name, e.g. "/dev/sdc" or "\\.\PhysicalDrive3".

	const TCHAR *usb_vendor;	// USB vendor name.
	const TCHAR *usb_product;	// USB product name.
	const TCHAR *usb_serial;	// USB serial number, in ASCII.

	const TCHAR *hdd_vendor;	// HDD vendor.
	const TCHAR *hdd_model;		// HDD model number.
	const TCHAR *hdd_fwver;		// HDD firmware version.

#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
	const TCHAR *hdd_serial;	// HDD serial number, in ASCII.
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */

	uint64_t size;			// HDD size, in bytes.
} RvtH_QueryEntry;

/**
 * Create a full serial number string.
 *
 * Converts 10xxxxxx to "HUA10xxxxxxY" and 20xxxxxx to "HMA20xxxxxxY".
 * This includes the check digit.
 *
 * @param serial Serial number as provided by the RVT-H Reader.
 * @return Allocated serial number string.
 */
char *rvth_create_full_serial_number(unsigned int serial);

/**
 * Scan all USB devices for RVT-H Readers.
 * @param pErr	[out,opt] Pointer to store positive POSIX error code in on error. (0 on success)
 * @return List of matching devices, or NULL if none were found.
 */
RvtH_QueryEntry *rvth_query_devices(int *pErr);

/**
 * Free a list of queried devices.
 */
void rvth_query_free(RvtH_QueryEntry *devs);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_QUERY_H__ */
