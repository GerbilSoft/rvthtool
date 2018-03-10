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

#ifdef __cplusplus
extern "C" {
#endif

// RVT-H Reader USB VID/PID.
#define RVTH_READER_VID 0x057e
#define RVTH_READER_PID 0x0304

/**
 * Scanned device entry.
 * This is a singly-linked list.
 */
typedef struct _RvtH_QueryEntry {
	struct _RvtH_QueryEntry *next;

	const char *device_name;	// Device name, e.g. "/dev/sdc" or "\\.\PhysicalDrive3".

	const char *usb_vendor;		// USB vendor name.
	const char *usb_product;	// USB product name.
	const char *serial_number;	// Serial number, in ASCII.
	const char *fw_version;		// Firmware version. (FIXME: HDD or RVT-H board?)

	const char *hdd_vendor;		// HDD vendor.
	const char *hdd_model;		// HDD model number.
	uint64_t size;			// HDD size, in bytes.
} RvtH_QueryEntry;

/**
 * Scan all USB devices for RVT-H Readers.
 * @return List of matching devices, or NULL if none were found.
 */
RvtH_QueryEntry *rvth_query_devices(void);

/**
 * Free a list of queried devices.
 */
void rvth_query_free(RvtH_QueryEntry *devs);

#ifdef __cplusplus
}
#endif

#endif /* __RVTHTOOL_LIBRVTH_QUERY_H__ */
