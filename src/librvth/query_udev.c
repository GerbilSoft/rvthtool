/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query_udev.c: Query storage devices. (udev version)                     *
 *                                                                         *
 * Copyright (c) 2018-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "config.librvth.h"
#include "query.h"

// TODO: stdboolx.h
#include "../libwiicrypto/common.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libudev.h>

#ifdef HAVE_PTHREADS
#  include <pthread.h>
#endif /* HAVE_PTHREADS */

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
 * Get RvtH_QueryEntry data for the specified udev device.
 * @param dev	[in] udev device node
 * @param pErr	[out,opt] Pointer to store positive POSIX error code in on error. (0 on success)
 * @return Allocated RvtH_QueryEntry if it's an RVT-H Reader; NULL if not.
 */
static RvtH_QueryEntry *rvth_parse_udev_device(struct udev_device *dev, int *pErr)
{
	RvtH_QueryEntry *entry;

	const char *s_devnode;
	struct udev_device *usb_dev, *scsi_dev;

	const char *s_blk_size, *s_usb_serial;
	unsigned int hw_serial;

	if (pErr) {
		// No error initially.
		*pErr = 0;
	}

	// udev_device_get_devnode() returns the path to the device node itself in /dev.
	s_devnode = udev_device_get_devnode(dev);
	if (!s_devnode) {
		// No device node...
		return NULL;
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
		return NULL;
	}
	
	// Check if the VID/PID matches Nintendo RVT-H Reader.
	if (!is_vid_pid_correct(usb_dev)) {
		return NULL;
	}

	// Get the serial number.
	s_usb_serial = udev_device_get_sysattr_value(usb_dev, "serial");
	if (!s_usb_serial) {
		// No serial number...
		return NULL;
	}
	hw_serial = (unsigned int)strtoul(s_usb_serial, NULL, 10);

	// Is the serial number valid?
	// FIXME: This might not be accurate...
	if (hw_serial < 10000000 || hw_serial > 29999999) {
		// Not a valid serial number.
		return NULL;
	}

	// Create an RvtHQueryEntry.
	entry = malloc(sizeof(*entry));
	if (!entry) {
		if (pErr) {
			*pErr = errno;
			if (*pErr == 0) {
				*pErr = ENOMEM;
			}
		}
		return NULL;
	}
	entry->next = NULL;

	// Block device size.
	// NOTE: Returns number of LBAs.
	// Assuming blocks are 512 bytes.
	// TODO: Get the actual LBA size.
	s_blk_size = udev_device_get_sysattr_value(dev, "size");

	// Copy the strings.
	entry->device_name = strdup(s_devnode);
	entry->usb_vendor = strdup_null(udev_device_get_sysattr_value(usb_dev, "manufacturer"));
	entry->usb_product = strdup_null(udev_device_get_sysattr_value(usb_dev, "product"));
	entry->usb_serial = rvth_create_full_serial_number(hw_serial);
	entry->hdd_vendor = strdup_null(udev_device_get_sysattr_value(scsi_dev, "vendor"));
	entry->hdd_model = strdup_null(udev_device_get_sysattr_value(scsi_dev, "model"));
	entry->hdd_fwver = strdup_null(udev_device_get_sysattr_value(scsi_dev, "rev"));
#ifdef RVTH_QUERY_ENABLE_HDD_SERIAL
	// TODO: SCSI device serial number?
	entry->hdd_serial = strdup_null(udev_device_get_sysattr_value(scsi_dev, "serial"));
#endif /* RVTH_QUERY_ENABLE_HDD_SERIAL */
	entry->size = (s_blk_size ? strtoull(s_blk_size, NULL, 10) * 512ULL : 0);

	// Can we access the device?
	entry->is_readable = !access(s_devnode, R_OK);
	if (entry->is_readable) {
		// Device is readable.
		entry->not_readable_error = 0;
		entry->is_writable = !access(s_devnode, W_OK);
	} else {
		// Not readable.
		entry->is_readable = false;
		entry->is_writable = false;
		entry->not_readable_error = (int)errno;
	}

	// NOTE: STORAGE_DEVICE_DESCRIPTOR has a serial number value
	// for the HDD itself, but the RVT-H Reader USB bridge
	// doesn't support this query.
	return entry;
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

	if (pErr) {
		// No error initially.
		*pErr = 0;
	}

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
		RvtH_QueryEntry *entry;
		struct udev_device *dev;
		const char *path;
		int err;

		// Get the filename of the /sys entry for the device
		// and create a udev_device object (dev) representing it.
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		// Attempt to get an RvtH_QueryEntry from this device node.
		entry = rvth_parse_udev_device(dev, &err);
		if (!entry) {
			// Not an RVT-H Reader.
			udev_device_unref(dev);
			if (err == 0)
				continue;

			// An error occurred.
			if (pErr) {
				*pErr = err;
			}
			rvth_query_free(list_head);
			list_head = NULL;
			list_tail = NULL;
			break;
		}

		// RvtH_QueryEntry obtained. Add it to the list.
		if (!list_head) {
			// New list head.
			list_head = entry;
			list_tail = list_head;
		} else {
			// Add an entry to the current list.
			list_tail->next = entry;
		}

		// Next device.
		udev_device_unref(dev);
	}

	// Free the enumerator object.
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
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

/** Listen for new devices **/

#ifdef HAVE_PTHREADS

/** pthreads is available; we can listen for devices. */

// NOTE: libudev uses an fd event model.
// We'll use a separate thread that calls a callback function.
// The callback function must handle GUI dispatch in a thread-safe manner.

// Reference: https://github.com/robertalks/udev-examples/blob/master/udev_example3.c

struct _RvtH_ListenForDevices {
	RvtH_DeviceCallback callback;
	void *userdata;

	struct udev *udev;
	struct udev_monitor *mon;
	int fd;

	pthread_t thread_id;
	bool stop;
};

/**
 * Main function for the udev thread.
 * @param listener_arg RvtH_ListenForDevices
 */
static void *rvth_listener_thread(void *listener_arg)
{
	RvtH_ListenForDevices *const listener = (RvtH_ListenForDevices*)listener_arg;
	const int fd = listener->fd;

	// pollfd
	struct pollfd pfds[1] = {{fd, POLLIN, 0}};

	while (!listener->stop) {
		// FIXME: Improve error handling.
		int ret = poll(pfds, 1, 500);
		if (ret <= 0) {
			// No events, or an error occurred.
			// TODO: Improve error handling.
			continue;
		}

		if (!(pfds[0].revents & POLLIN)) {
			// No incoming data...
			continue;
		}

		struct udev_device *const dev = udev_monitor_receive_device(listener->mon);
		if (!dev) {
			continue;
		}

		// Check if this is "add" or "remove".
		const char *action = udev_device_get_action(dev);
		if (!action) {
			continue;
		}

		if (!strcmp(action, "add")) {
			// Device added.
			RvtH_QueryEntry *const entry = rvth_parse_udev_device(dev, NULL);
			if (entry) {
				listener->callback(listener, entry, RVTH_LISTEN_CONNECTED, listener->userdata);
				rvth_query_free(entry);
			}
		} else if (!strcmp(action, "remove")) {
			// Deivce removed.
			// NOTE: Can't get the correct parent USB device for
			// the block device on removal. The caller will have
			// to verify if this device node is in its list.
			// TODO: Monitor USB devices too?
			RvtH_QueryEntry *const entry = calloc(1, sizeof(*entry));
			if (entry) {
				entry->device_name = udev_device_get_devnode(dev);
				listener->callback(listener, entry, RVTH_LISTEN_DISCONNECTED, listener->userdata);
				free(entry);
			}
		}
	}

	// TODO
	return NULL;
}

/**
 * Listen for new and/or removed devices.
 * @param callback Callback function
 * @param userdata User data
 * @return Listener object, or nullptr on error.
 */
RvtH_ListenForDevices *rvth_listen_for_devices(RvtH_DeviceCallback callback, void *userdata)
{
	int ret;

	assert(callback != NULL);
	if (!callback) {
		// No point in listening with no callback.
		return NULL;
	}

	RvtH_ListenForDevices *const listener = calloc(1, sizeof(RvtH_ListenForDevices));
	if (!listener) {
		return NULL;
	}

	listener->udev = udev_new();
	if (!listener->udev) {
		free(listener);
		return NULL;
	}

	// udev monitor doesn't allow us to filter on specific USB IDs.
	// We'll filter on block devices.
	listener->mon = udev_monitor_new_from_netlink(listener->udev, "udev");
	if (!listener->mon) {
		udev_unref(listener->udev);
		free(listener);
		return NULL;
	}

	udev_monitor_filter_add_match_subsystem_devtype(listener->mon, "block", NULL);
	udev_monitor_enable_receiving(listener->mon);
	listener->fd = udev_monitor_get_fd(listener->mon);
	if (listener->fd < 0) {
		udev_monitor_unref(listener->mon);
		udev_unref(listener->udev);
		free(listener);
		return NULL;
	}

	// udev is set up. Create the thread.
	ret = pthread_create(&listener->thread_id, NULL, rvth_listener_thread, listener);
	if (ret != 0) {
		// Error creating the thread.
		udev_monitor_unref(listener->mon);
		udev_unref(listener->udev);
		free(listener);
		return NULL;
	}

	// Thread is listening.
	listener->callback = callback;
	listener->userdata = userdata;
	return listener;
}

/**
 * Stop listening for new and/or removed devices.
 * @param listener RvtH_ListenForDevices
 */
void rvth_listener_stop(RvtH_ListenForDevices *listener)
{
	// TODO: Kill the thread?
	listener->stop = true;
}
#else /* !HAVE_PTHREADS */

/** pthreads is unavailable. No device listening. */

/**
 * Listen for new and/or removed devices.
 * @param callback Callback function
 * @param userdata User data
 * @return Listener object, or nullptr on error.
 */
RvtH_ListenForDevices *rvth_listen_for_devices(RvtH_DeviceCallback callback, void *userdata)
{
	((void)callback);
	((void)userdata);
	return NULL;
}

/**
 * Stop listening for new and/or removed devices.
 * @param listener RvtH_ListenForDevices
 */
void rvth_listener_stop(RvtH_ListenForDevices *listener)
{
	((void)listener);
}

#endif /* HAVE_PTHREADS */