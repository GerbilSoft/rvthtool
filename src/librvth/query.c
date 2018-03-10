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

#include <stdlib.h>

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
		free((char*)devs->serial_number);
		free((char*)devs->fw_version);
		free((char*)devs->hdd_vendor);
		free((char*)devs->hdd_model);
		free(devs);
		devs = next;
	}
}
