# RVT-H Tool

This is an open-source tool for managing RVT-H Reader consoles.

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)<br>
[![Travis Build Status](https://travis-ci.org/GerbilSoft/rvthtool.svg?branch=master)](https://travis-ci.org/GerbilSoft/rvthtool)
[![AppVeyor Build status](https://ci.appveyor.com/api/projects/status/l83tx6d16gqr4ov2?svg=true)](https://ci.appveyor.com/project/GerbilSoft/rvthtool/branch/master)

## Current Features

* Lists all disc images currently installed on the RVT-H system.
* Lists "deleted" images that aren't accessible on the RVT-H but are still
  present on the HDD.
* Extracts disc images from the RVT-H into an image file.
* Allows deletion and undeletion of banks on an RVT-H system.

## Planned Features

* Installation of GameCube and Wii disc images. Wii disc images must be
  either unencrypted or debug-encrypted in order to run correctly.
  * Support for both CISO and WBFS formats for importing.
* Automatic re-signing of retail Wii disc images to allow them to run on
  the RVT-H system. (Update partitions will be removed, since retail updates
  won't work properly on RVT-H.)
* Standalone disc image re-signing to convert e.g. retail to debug, debug
  to retail, unencrypted to debug, etc. Conversion to retail will result
  in a fakesigned image.
  * Support for both CISO and WBFS formats for converting retail to debug
    and vice-versa, but not encrypted to unencrypted.
* RVT-H device scanning to determine which device names are associated with
  connected RVT-H Readers.

A future version will also add a GUI.

## Usage

The following commands assume `/dev/sdb` is the RVT-H device.

Full unencrypted RVT-H disk image dumps taken from the front-panel USB port
are also supported.

Direct device access on Windows is possible by specifying `\\.\PhysicalDriveN`,
where N is the physical disk number. Disk Management will show the physical disk
number in the bottom pane.

**WARNING:** Disk Management will prompt to initialize the RVT-H device. DO NOT
ALLOW IT TO INITIALIZE THE DRIVE; this may cause data loss.

List disc images: `sudo ./rvthtool list /dev/sdb`

Extract disc image: `sudo ./rvthtool extract /dev/sdb 1 disc.gcm`
* Replace `1` with the bank number.

Delete a bank: `sudo ./rvthtool delete /dev/sdb 1`
* Replace `1` with the bank number.

Undelete a bank: `sudo ./rvthtool undelete /dev/sdb 1`
* Replace `1` with the bank number.

## Encryption

The RVT-H's internal hard drive is not encrypted. This allows `rvthtool` to
operate on an RVT-H system that's connected directly using USB, as well as
disk image dumps from both the USB interface and from a direct HDD dump.

Disc images on the RVT-H may or may not be encrypted:
* GameCube: Not encrypted.
* Wii: May be encrypted using the RVT-R debug key, or not encrypted.

Wii disc images encrypted using the RVT-R debug key are currently playable
on the [Dolphin emulator](https://dolphin-emu.org/) and on retail consoles
with a USB loader, if the image is re-encrypted and fakesigned using the
retail keys.

Unencrypted Wii disc images are not currently usable on Dolphin or retail
consoles. I have been working on adding unencrypted image support to Dolphin
but have not been successful yet.

### Note about Debug IOS

RVT-R disc images typically include debug versions of IOS. These will not
install on retail consoles, since they're encrypted with debug keys.
Do **NOT** attempt to install them by re-encrypting them with the retail
keys. Doing so will most likely result in a brick, especially if the 128 MB
mode IOS WADs are used.
