/***************************************************************************
 * RVT-H Tool (librvth)                                                    *
 * query_win32.c: Query storage devices. (Win32 version)                   *
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

#include "query.h"

// C includes.
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Windows includes.
#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
//#include <devioctl.h>
//#include <ntddstor.h>
#include <devguid.h>
#include <cfgmgr32.h>

// _tcsdup() wrapper that returns NULL if the input is NULL.
static inline TCHAR *_tcsdup_null(const TCHAR *s)
{
	return (s ? _tcsdup(s) : NULL);
}

/**
 * Scan all USB devices for RVT-H Readers.
 * @return List of matching devices, or NULL if none were found.
 */
RvtH_QueryEntry *rvth_query_devices(void)
{
	RvtH_QueryEntry *list_head = NULL;
	RvtH_QueryEntry *list_tail = NULL;

	HDEVINFO hDevInfoSet = NULL;
	SP_DEVINFO_DATA devInfoData;
	int devIndex = 0;

	/* Temporary string buffers */

	// Instance IDs.
	TCHAR s_usbInstanceID[MAX_DEVICE_ID_LEN];	// USB drive
	TCHAR s_diskInstanceID[MAX_DEVICE_ID_LEN];	// Storage device

	// Pathnames.
	TCHAR s_devicePath[MAX_PATH];	// Storage device path

	// Attributes.
	// TODO: Buffer sizes?
	TCHAR s_usb_vendor[32];
	TCHAR s_usb_product[128];
	TCHAR s_serial_number[32];
	TCHAR s_fw_version[16];
	TCHAR s_hdd_vendor[32];
	TCHAR s_hdd_model[64];
	uint64_t hdd_size;

	// Get all USB devices and find one with a matching vendor/device ID.
	// Once we find it, get GUID_DEVINTERFACE_DISK for that ID.
	// References:
	// - https://docs.microsoft.com/en-us/windows/desktop/api/setupapi/nf-setupapi-setupdigetclassdevsw
	// - https://social.msdn.microsoft.com/Forums/vstudio/en-US/76315dfa-6764-4feb-a3e2-1f173fc5bdfd/how-to-get-usb-vendor-and-product-id-programmatically-in-c?forum=vcgeneral
	hDevInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL,
		NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (!hDevInfoSet || hDevInfoSet == INVALID_HANDLE_VALUE) {
		// SetupDiGetClassDevs() failed.
		return NULL;
	}

	memset(&devInfoData, 0, sizeof(devInfoData));
	devInfoData.cbSize = sizeof(devInfoData);
	while (SetupDiEnumDeviceInfo(hDevInfoSet, devIndex, &devInfoData)) {
		DEVINST dnDevInstParent;
		TCHAR *p;
		int ret;

		// USB ID
		uint16_t vid = 0, pid = 0;

		// DeviceIoControl()
		HANDLE hDrive;
		STORAGE_DEVICE_NUMBER sdn;
		DWORD cbBytesReturned;
		BOOL bRet;

		// Increment the device index here.
		devIndex++;

		// Get the parent instance ID.
		// References:
		// - https://stackoverflow.com/questions/3098696/get-information-about-disk-drives-result-on-windows7-32-bit-system
		// - https://stackoverflow.com/a/3100268
		ret = CM_Get_Parent(&dnDevInstParent, devInfoData.DevInst, 0);
		if (ret != CR_SUCCESS)
			continue;
		ret = CM_Get_Device_ID(dnDevInstParent, s_usbInstanceID, _countof(s_usbInstanceID), 0);
		if (ret != CR_SUCCESS)
			continue;

		// Format: "USB\\VID_%04X&PID_%04X"
		// NOTE: Needs to be converted to uppercase, since "Vid" and "Pid" might show up.
		p = s_usbInstanceID;
		while (*p != _T('\0')) {
			*p = _totupper(*p);
			p++;
		}

		// Try sscanf().
		ret = _stscanf(s_usbInstanceID, _T("USB\\VID_%04hX&PID_%04hX"), &vid, &pid);
		if (ret != 2)
			continue;

		// Does the USB device ID match?
		if (vid != RVTH_READER_VID || pid != RVTH_READER_PID) {
			// Not an RVT-H Reader.
			continue;
		}

		// Get this device's instance path.
		// It works as a stand-in for \\\\.\\\PhysicalDrive#
		ret = CM_Get_Device_ID(devInfoData.DevInst, s_diskInstanceID, _countof(s_diskInstanceID), 0);
		if (ret != CR_SUCCESS)
			continue;

		// Use the disk instance ID to create the actual path.
		// NOTE: Needs some adjustments: [FOR COMMIT, use actual RVT-H path]
		// - Disk instance ID:   USBSTOR\DISK&VEN_&PROD_PATRIOT_MEMORY&REV_PMAP\070B7AAA9AF3D977&0
		// - Actual path: \\\\?\\usbstor#disk&ven_&prod_patriot_memory&rev_pmap#070b7aaa9af3d977&0#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}
		// Adjustments required:
		// - Replace backslashes in instance ID with #
		// - Append GUID_DEVINTERFACE_DISK [TODO: Convert from the GUID at runtime?]

		p = s_diskInstanceID;
		while (*p != _T('\0')) {
			if (*p == _T('\\')) {
				*p = _T('#');
			}
			p++;
		}
		_sntprintf(s_devicePath, _countof(s_devicePath),
			_T("\\\\?\\%s#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}"), s_diskInstanceID);

		// Open the drive to retrieve information.
		// NOTE: Might require administrative privileges.
		// References:
		// - https://stackoverflow.com/questions/5583553/name-mapping-physicaldrive-to-scsi
		// - https://stackoverflow.com/a/5584978
		hDrive = CreateFile(s_devicePath,
			GENERIC_READ,		// dwDesiredAccess
			FILE_SHARE_READ,	// dwShareMode
			NULL,			// lpSecurityAttributes
			OPEN_EXISTING,		// dwCreationDisposition
			FILE_ATTRIBUTE_NORMAL,	// dwFlagsAndAttributes
			NULL);			// hTemplateFile
		if (!hDrive || hDrive == INVALID_HANDLE_VALUE)
			continue;

		bRet = DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL, 0,			// no input buffer
			(LPVOID)&sdn, sizeof(sdn),	// output buffer
			&cbBytesReturned,		// # bytes returned
			NULL);				// synchronous I/O
		if (!bRet || sdn.DeviceType != FILE_DEVICE_DISK) {
			// Unable to get the device information,
			// or this isn't a disk device.
			CloseHandle(hDrive);
			continue;
		}

		// Create a list entry.
		if (!list_head) {
			// New list head.
			list_head = malloc(sizeof(*list_head));
			if (!list_head) {
				// malloc() failed.
				CloseHandle(hDrive);
				break;
			}
			list_tail = list_head;
			list_tail->next = NULL;
		} else {
			// Add an entry to the current list.
			list_tail->next = malloc(sizeof(*list_tail));
			if (!list_tail->next) {
				// malloc() failed.
				CloseHandle(hDrive);
				rvth_query_free(list_head);
				list_head = NULL;
				list_tail = NULL;
				break;
			}
			list_tail->next = NULL;
		}

		// Device name.
		_sntprintf(s_devicePath, _countof(s_devicePath),
			_T("\\\\.\\PhysicalDrive%u"), sdn.DeviceNumber);
		list_tail->device_name = _tcsdup(s_devicePath);

		// Get more drive information.
		// TODO: Also get USB information.
		// References:
		// - https://stackoverflow.com/questions/327718/how-to-list-physical-disks
		// - https://stackoverflow.com/a/18183115

		// TODO
		list_tail->usb_vendor = NULL;
		list_tail->usb_product = NULL;
		list_tail->serial_number = NULL;
		list_tail->fw_version = NULL;
		list_tail->hdd_vendor = NULL;
		list_tail->hdd_model = NULL;
		list_tail->size = 0;

		CloseHandle(hDrive);
	}

	SetupDiDestroyDeviceInfoList(hDevInfoSet);
	return list_head;
}
