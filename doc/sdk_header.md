# GCM SDK Headers

This document is Copyright (C) 2018 by David Korth.<br>
Licensed under the GNU Free Documentation License v1.3.

GCM images produced by the official GameCube and Wii SDKs have a 32 KB header.
This header isn't present in images ripped using e.g. CleanRip, and it isn't
recognized by the Dolphin emulator. It's also not present on RVT-H Reader HDDs.
However, it **is** needed when using the official SDK tools, e.g. rvtwriter and
the NDEV optical drive emulator.

## Example Header

The following header is from an swupdate image from one of the Wii SDKs:

```
00000000  ff ff 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000820  00 00 00 00 00 00 00 00  00 00 00 00 00 00 e0 06  |................|
00000830  00 00 6b 4f 00 00 00 00  00 00 00 00 00 00 00 00  |..kO............|
00000840  00 00 00 00 01 00 00 00  00 00 00 00 00 00 00 00  |................|
00000850  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00008000  52 41 42 41 30 31 00 00  00 00 00 00 00 00 00 00  |RABA01..........|
```

The header is mostly zeroes, except for these important DWORD values:
* 0x0000: `FF FF 00 00`
* 0x082C: `00 00 E0 06`
* 0x0830: `00 00 6B 4F`

The value at 0x0830 appears to be a checksum of some sort. If this header is
prepended as-is to any generic Wii image and loaded onto an NDEV using ODEM,
the NDEV will show a "Wrong disk" error. However, zeroing out this DWORD gets
it working for all discs.

Images built using `makeGCM` from the GameCube SDK always have a checksum of
`0xAB0B`; however, the NDEV system locks up after displaying the disc info.
