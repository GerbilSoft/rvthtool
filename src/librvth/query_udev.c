/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query_udev.c: Query storage devices. (udev version)                     *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "query.h"

// TODO: stdboolx.h
#include "../libwiicrypto/common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libudev.h>

// strdup() wrapper that returns NULL if the input is NULL.
static inline char *strdup_null(const char *s)
{
	return (s ? strdup(s) : NULL);
}

/**
 * Check if a USB device has the correct VID/PID.
 * @param usb_dev USB device.
 * @return True if it's correct; false if not.
 */
static bool is_vid_pid_correct(struct udev_device *usb_dev)
{
	const char *s_vid, *s_pid;
	unsigned int vid, pid;

	// Check if the VID/PID matches Nintendo RVT-H Reader.
	s_vid = udev_device_get_sysattr_value(usb_dev, "idVendor");
	s_pid = udev_device_get_sysattr_value(usb_dev, "idProduct");
	if (!s_vid || !s_pid) {
		// Unable to get either VID or PID.
		return false;
	}

	vid = strtoul(s_vid, NULL, 16);
	pid = strtoul(s_pid, NULL, 16);
	if (vid != RVTH_READER_VID || pid != RVTH_READER_PID) {
		// Incorrect VID or PID.
		return false;
	}

	// Correct VID/PID.
	return true;
}

/**
 * Scan all USB devices for RVT-H Readers.
 * @param pErr	[out,opt] Pointer to store positive POSIX error code in on error. (0 on success)
 * @return List of matching devices, or NULL if none were found.
 */
RvtH_QueryEntry *rvth_query_devices(int *pErr)
{
	RvtH_QueryEntry *list_head = NULL;
	RvtH_QueryEntry *list_tail = NULL;

	// Reference: http://www.signal11.us/oss/udev/
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;

	// Create the udev object.
	udev = udev_new();
	if (!udev) {
		// Unable to create a udev object.
		if (pErr) {
			*pErr = ENOMEM;
		}
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
		struct udev_device *dev;

		const char *path;
		const char *s_devnode;

		const char *s_blk_size;
		const char *s_usb_serial;
		unsigned int hw_serial;

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
		if (!is_vid_pid_correct(usb_dev)) {
			udev_device_unref(dev);
			continue;
		}

		// Get the serial number.
		s_usb_serial = udev_device_get_sysattr_value(usb_dev, "serial");
		if (!s_usb_serial) {
			// No serial number...
			udev_device_unref(dev);
			continue;
		}
		hw_serial = (unsigned int)strtoul(s_usb_serial, NULL, 10);

		// Is the serial number valid?
		// - Wired:    10xxxxxx
		// - Wireless: 20xxxxxx
		if (hw_serial < 10000000 || hw_serial > 29999999) {
			// Not a valid serial number.
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
		list_tail->usb_vendor = strdup_null(udev_device_get_sysattr_value(usb_dev, "manufacturer"));
		list_tail->usb_product = strdup_null(udev_device_get_sysattr_value(usb_dev, "product"));
		list_tail->usb_serial = rvth_create_full_serial_number(hw_serial);
		list_tail->hdd_vendor = strdup_null(udev_device_get_sysattr_value(scsi_dev, "vendor"));
		list_tail->hdd_model = strdup_null(udev_device_get_sysattr_value(scsi_dev, "model"));
		list_tail->hdd_fwver = strdup_null(udev_device_get_sysattr_value(scsi_dev, "rev"));
#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
		// TODO: SCSI device serial number?
		list_tail->hdd_serial = strdup_null(udev_device_get_sysattr_value(scsi_dev, "serial"));
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */
		list_tail->size = (s_blk_size ? strtoull(s_blk_size, NULL, 10) * 512ULL : 0);

		// NOTE: STORAGE_DEVICE_DESCRIPTOR has a serial number value
		// for the HDD itself, but the RVT-H Reader USB bridge
		// doesn't support this query.

		udev_device_unref(dev);
	}

	// Free the enumerator object.
	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	if (pErr) {
		*pErr = 0;
	}
	return list_head;
}

/**
 * Get the serial number for the specified RVT-H Reader device.
 * @param filename	[in] RVT-H Reader device filename.
 * @param pErr		[out,opt] Pointer to store positive POSIX error code in on error. (0 on success)
 * @return Allocated serial number string, or nullptr on error.
 */
TCHAR *rvth_get_device_serial_number(const TCHAR *filename, int *pErr)
{
	// Reference: http://www.signal11.us/oss/udev/
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	TCHAR *s_full_serial = NULL;

	// Create the udev object.
	udev = udev_new();
	if (!udev) {
		// Unable to create a udev object.
		if (pErr) {
			*pErr = ENOMEM;
		}
		return NULL;
	}

	// TODO: Get the device directly based on `filename`
	// instead of enumerating devices.

	// Create a list of the devices in the 'block' subsystem.
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	// Go over the list of devices and find matching USB devices.
	udev_list_entry_foreach(dev_list_entry, devices) {
		struct udev_device *usb_dev;
		struct udev_device *dev;

		const char *path;
		const char *s_devnode;

		const char *s_usb_serial;
		unsigned int hw_serial;

		// Get the filename of the /sys entry for the device
		// and create a udev_device object (dev) representing it.
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		// udev_device_get_devnode() returns the path to the device node itself in /dev.
		s_devnode = udev_device_get_devnode(dev);
		if (!s_devnode || _tcscmp(filename, s_devnode) != 0) {
			// No device node, or wrong filename.
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
		usb_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
		if (!usb_dev) {
			// Parent USB device is missing.
			// Not an external HDD.
			udev_device_unref(dev);
			continue;
		}
	
		// Check if the VID/PID matches Nintendo RVT-H Reader.
		if (!is_vid_pid_correct(usb_dev)) {
			udev_device_unref(dev);
			continue;
		}

		// Get the serial number.
		s_usb_serial = udev_device_get_sysattr_value(usb_dev, "serial");
		if (!s_usb_serial) {
			// No serial number...
			udev_device_unref(dev);
			continue;
		}
		hw_serial = (unsigned int)strtoul(s_usb_serial, NULL, 10);

		// Is the serial number valid?
		// - Wired:    10xxxxxx
		// - Wireless: 20xxxxxx
		if (hw_serial < 10000000 || hw_serial > 29999999) {
			// Not a valid serial number.
			udev_device_unref(dev);
			continue;
		}

		// Create the full serial number.
		s_full_serial = rvth_create_full_serial_number(hw_serial);
		udev_device_unref(dev);
		break;
	}

	// Free the enumerator object.
	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	if (pErr) {
		if (s_full_serial) {
			// Full serial number obtained.
			*pErr = 0;
		} else {
			// Device not found.
			*pErr = ENOENT;
		}
	}
	return s_full_serial;
}
