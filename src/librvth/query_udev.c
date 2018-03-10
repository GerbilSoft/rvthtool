/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query_udev.c: Query storage devices. (udev version)                     *
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

#include <libudev.h>

// strdup() wrapper that returns NULL if the input is NULL.
static inline char *strdup_null(const char *s)
{
	return (s ? strdup(s) : NULL);
}

/**
 * Scan all USB devices for RVT-H Readers.
 * @return List of matching devices, or NULL if none were found.
 */
RvtH_QueryEntry *rvth_query_devices(void)
{
	RvtH_QueryEntry *list_head = NULL;
	RvtH_QueryEntry *list_tail = NULL;

	// Reference: http://www.signal11.us/oss/udev/
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;

	// Create the udev object.
	udev = udev_new();
	if (!udev) {
		// Unable to create a udev object.
		return NULL;
	}

	// Create a list of the devices in the 'block' subsystem.
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	// Go over the list of devices and find matching USB devices.
	udev_list_entry_foreach(dev_list_entry, devices) {
		struct udev_device *usb_dev, *scsi_dev;

		const char *path;
		const char *s_devnode;
		const char *s_vid, *s_pid;
		unsigned int vid, pid;

		const char *s_blk_size;

		// Get the filename of the /sys entry for the device
		// and create a udev_device object (dev) representing it.
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		// udev_device_get_devnode() returns the path to the device node itself in /dev.
		s_devnode = udev_device_get_devnode(dev);
		if (!s_devnode) {
			// No device node...
			udev_device_unref(dev);
			continue;
		}

		// The device pointed to by dev contains information about the
		// block device. In order to get information about the USB device,
		// get the parent device with the subsystem/devtype pair of
		// "usb"/"usb_device". This will be several levels up the tree,
		// but the function will find it.
		// NOTE: This device is NOT referenced, and is cleaned up when
		// dev is cleaned up.
		scsi_dev = udev_device_get_parent_with_subsystem_devtype(dev, "scsi", "scsi_device");
		usb_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
		if (!scsi_dev || !usb_dev) {
			// Parent SCSI and/or USB devices are missing.
			// - No SCSI device: Not an HDD.
			// - No USB device: Not an external HDD.
			udev_device_unref(dev);
			continue;
		}
	
		// Check if the VID/PID matches Nintendo RVT-H Reader.
		s_vid = udev_device_get_sysattr_value(usb_dev, "idVendor");
		s_pid = udev_device_get_sysattr_value(usb_dev, "idProduct");
		if (!s_vid || !s_pid) {
			// Unable to get either VID or PID.
			udev_device_unref(dev);
			continue;
		}
		vid = strtoul(s_vid, NULL, 16);
		pid = strtoul(s_pid, NULL, 16);
		if (vid != RVTH_READER_VID || pid != RVTH_READER_PID) {
			// Incorrect VID or PID.
			udev_device_unref(dev);
			continue;
		}

		// Create a list entry.
		if (!list_head) {
			// New list head.
			list_head = malloc(sizeof(*list_head));
			if (!list_head) {
				// malloc() failed.
				udev_device_unref(dev);
				break;
			}
			list_tail = list_head;
			list_tail->next = NULL;
		} else {
			// Add an entry to the current list.
			list_tail->next = malloc(sizeof(*list_tail));
			if (!list_tail->next) {
				// malloc() failed.
				udev_device_unref(dev);
				rvth_query_free(list_head);
				list_head = NULL;
				list_tail = NULL;
				break;
			}
			list_tail->next = NULL;
		}

		// Block device size.
		// NOTE: Returns number of LBAs.
		// Assuming blocks are 512 bytes.
		// TODO: Get the actual LBA size.
		s_blk_size = udev_device_get_sysattr_value(dev, "size");

		// Copy the strings.
		list_tail->device_name = strdup(s_devnode);
		list_tail->serial_number = strdup_null(udev_device_get_sysattr_value(usb_dev, "serial"));
		list_tail->fw_version = strdup_null(udev_device_get_sysattr_value(scsi_dev, "rev"));
		list_tail->hdd_vendor = strdup_null(udev_device_get_sysattr_value(scsi_dev, "vendor"));
		list_tail->hdd_model = strdup_null(udev_device_get_sysattr_value(scsi_dev, "model"));
		list_tail->size = (s_blk_size ? strtoull(s_blk_size, NULL, 10) * 512ULL : 0);

		udev_device_unref(dev);
	}

	// Free the enumerator object.
	udev_enumerate_unref(enumerate);

	udev_unref(udev);
	return list_head;
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
		free((char*)devs->serial_number);
		free((char*)devs->fw_version);
		free((char*)devs->hdd_vendor);
		free((char*)devs->hdd_model);
		free(devs);
		devs = next;
	}
}
