/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query.c: Query storage devices. (common code)                           *
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

#include "query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Create a full serial number string.
 * This includes the check digit.
 *
 * TODO: Figure out if there's a way to determine HMA (wireless)
 * vs. HUA (wired). Both wireless and wired systems have been
 * seen with serial numbers 20xxxxxx.
 *
 * @param serial Serial number as provided by the RVT-H Reader.
 * @return Allocated serial number string.
 */
TCHAR *rvth_create_full_serial_number(unsigned int serial)
{
	TCHAR buf[16];
	unsigned int serial_tmp = serial;
	unsigned int even = 0, odd = 0;
	int i;

	if (serial < 10000000 || serial > 29999999) {
		// Serial number is out of range.
		// Print it as-is.
		_sntprintf(buf, sizeof(buf), _T("%08u"), serial);
		return _tcsdup(buf);
	}

	// Calculate the check digit.
	// Reference: https://www.3dbrew.org/wiki/Serials
	// - Add even digits together.
	// - Add odd digits together.
	// - Calculate: ((even * 3) + odd) % 10
	for (i = 0; i < 8; i += 2) {
		// The first digit we're processing here is actually
		// the last digit in the serial number, so it's
		// considered an even digit.
		even += (serial_tmp % 10);
		serial_tmp /= 10;
		odd += (serial_tmp % 10);
		serial_tmp /= 10;
	}
	even *= 3;
	serial_tmp = (even + odd) % 10;
	if (serial_tmp != 0) {
		// Subtract from 10.
		serial_tmp = 10 - serial_tmp;
	}

	_sntprintf(buf, sizeof(buf), _T("%08u%01u"), serial, serial_tmp);
	return _tcsdup(buf);
}

/**
 * Free a list of queried devices.
 */
void rvth_query_free(RvtH_QueryEntry *devs)
{
	RvtH_QueryEntry *next;
	while (devs) {
		next = devs->next;
		free((char*)devs->device_name);
		free((char*)devs->usb_vendor);
		free((char*)devs->usb_product);
		free((char*)devs->usb_serial);
		free((char*)devs->hdd_vendor);
		free((char*)devs->hdd_model);
#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
		free((char*)devs->hdd_serial);
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */
		free((char*)devs->hdd_fwver);
		free(devs);
		devs = next;
	}
}
