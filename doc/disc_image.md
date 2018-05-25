# RVT-H Reader Disc Image Format

This document is Copyright (C) 2018 by David Korth.<br>
Licensed under the GNU Free Documentation License v1.3.

Reference for general Wii disc structure information:
http://wiibrew.org/wiki/Wii_Disc

This document covers the format differences between a standard encrypted
Wii disc (both retail and debug) and an unencrypted disc image as found on
RVT-H Reader systems.

## Disc Header

This is "The Last Story" v2340.

```
00000000  53 4c 53 50 30 31 00 00  00 00 00 00 00 00 00 00  |SLSP01..........|
00000010  00 00 00 00 00 00 00 00  5d 1c 9e a3 00 00 00 00  |........].......|
00000020  54 48 45 20 4c 41 53 54  20 53 54 4f 52 59 00 00  |THE LAST STORY..|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060  01 01 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

Deconstructing the various disc header fields:

| Address | Length |        Data         |     Value      |          Description          |
|:-------:|:------:|---------------------|----------------|-------------------------------|
|   0x000 |    6   | `53 4c 53 50 30 31` | SLSP01         | Game ID                       |
|   0x006 |    1   | `00`                | 0              | Disc number                   |
|   0x007 |    1   | `00`                | 0 (v1.00)      | Disc version                  |
|   0x008 |    1   | `00`                | 0              | Audio streaming               |
|   0x009 |    1   | `00`                | 0              | Streaming buffer size         |
|   0x00A |   14   | All `00`            | N/A            | Unused                        |
|   0x018 |    4   | `5d 1c 9e a3`       | 0x5d1c9ea3     | Wii magic number (0x5d1c9ea3) |
|   0x01C |    4   | `00 00 00 00`       | 0x00000000     | GCN magic number (0xc2339f3d) |
|   0x020 |   64   | `THE LAST STORY`    | THE LAST STORY | Title                         |
|   0x060 |    1   | `01`                | 1              | Disable hash verification     |
|   0x061 |    1   | `01`                | 1              | Disable encryption and H3     |

The important fields here are at 0x060 and 0x061. These are normally set to 0
on retail and debug discs in the headers, and 1 within encrypted partitions.
In unencrypted images, these are set to 1 in the disc header.

## Partition Table

```
00040000  00 00 00 02 00 01 00 08  00 00 00 00 00 00 00 00  |................|
00040010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00040020  00 01 40 00 00 00 00 01  00 20 e0 00 00 00 00 00  |..@...... ......|
```

NOTE: Partition offsets are right-shifted by two.

This partition table indicates that we have two partitions:
* Partition 0: Update @ 0x50000
* Partition 1: Game @ 0x838000 (VERIFY)

Game partitions are normally located 0xF800000 on encrypted retail and
debug discs.

Note that the update partition on this disc is garbage. It consists of
incrementing DWORD values instead of an actual partition. Debug discs
usually have two IOS WADs in the update partition: one for 64 MB MEM2
systems (RVT-R Reader), and one for 128 MB MEM2 systems (RVT-H Reader,
NDEV).

## Partition Header

Partition header, excluding the ticket and TMD:

```
008382a0              00 00 02 08  00 00 00 b0 00 00 0c 40       |..........@|
008382b0  00 00 01 38 00 00 00 00  00 00 20 00 00 00 00 00  |...8...... .....|
```

Addresses are relative to the start of the partition. (0x838000)

Values in bold are different from encrypted discs.

| Address | Length |        Data         |     Value      |          Description          |
|:-------:|:------:|---------------------|----------------|-------------------------------|
|   0x000 |  0x2A4 | See below.          | See below.     | Ticket                        |
|   0x2A4 |    4   | `00 00 02 08`       | 0x208          | TMD size                      |
|   0x2A8 |    4   | `00 00 00 b0`       | 0x2C0          | TMD offset                    |
|   0x2AC |    4   | `00 00 0c 40`       | 0xC40          | Certificate chain size        |
|   0x2B0 |    4   | `00 00 01 38`       | 0x4E0          | Certificate chain offset >> 2 |
|   0x2B4 |    4   | `00 00 00 00`       | **0**          | H3 table offset >> 2          |
|   0x2B8 |    4   | `00 00 20 00`       | **0x8000**     | Data offset >> 2              |
|   0x2BC |    4   | `00 00 00 00`       | **0**          | Data size >> 2                |
|   0x2C0 | varies | See below.          | See below.     | TMD                           |

Three fields are significantly different in the unencrypted image:
* H3 table offset: 0 - there is no H3 table in unencrypted images. In
  encrypted images, this is usually 0x8000, or the sector after the TMD.
* Data offset: 0x8000 - this is usually 0x20000 in encrypted images.
  The difference is the missing H3 table.
* Data size: 0 - In encrypted images, this is set to the size of the encrypted
  data area, not counting the partition header. In unencrypted images, this
  is 0. I'm not sure if this is intentional or if there's a bug in the
  mastering tool.

### Ticket

($838000-$83813F is the RSA signature; this is left out of the hexdump.)

```
00838140  52 6f 6f 74 2d 43 41 30  30 30 30 30 30 30 32 2d  |Root-CA00000002-|
00838150  58 53 30 30 30 30 30 30  30 36 00 00 00 00 00 00  |XS00000006......|
00838160  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838170  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838180  01 d5 a2 3c e8 e9 df 8c  0a a5 aa e1 76 e1 24 7c  |...<........v.$||
00838190  90 97 a6 fe 2a d3 8c b4  de 74 32 de 5b 84 01 ac  |....*....t2.[...|
008381a0  b2 13 71 9d 4b 9c eb d4  13 c2 27 66 c5 5f 83 97  |..q.K.....'f._..|
008381b0  c4 83 a3 9e 3b fc e8 a9  b5 10 3a 7b 00 00 00 94  |....;.....:{....|
008381c0  c1 50 6c 34 36 50 20 74  72 0f 9d 07 20 3e 30 00  |.Pl46P tr... >0.|
008381d0  00 01 e5 89 b2 45 95 eb  00 00 00 00 00 01 00 00  |.....E..........|
008381e0  53 4c 53 50 ff ff 00 00  00 00 00 00 00 00 00 00  |SLSP............|
008381f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838210  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838220  00 04 ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  |................|
00838230  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  |................|
00838240  ff ff 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838250  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838270  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838280  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838290  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
008382a0  00 00 00 00                                       |....|
```

Addresses are relative to the start of the ticket. (0x838000)

Values in bold are different from encrypted discs.

| Address | Length |             Data             |     Value      |          Description          |
|:-------:|:------:|------------------------------|----------------|-------------------------------|
|   0x000 |  0x13F | RSA signature                | See hexdump    | RSA signature                 |
|   0x140 |   0x40 | `Root-CA00000002-XS00000006` | Dev Ticket     | Signature issuer              |
|   0x180 |   0x3C | ECDH data                    | See hexdump    | ECDH data for one-time keys   |
|   0x1BC |   0x03 | `00 00 00`                   | N/A            | Padding                       |
|   0x1BF |   0x10 | Encrypted title key          | See hexdump    | Encrypted title key           |
|   0x1CF |   0x01 | `00`                         | 0x00           | Unknown                       |
|   0x1D0 |   0x08 | `00 01 e5 89 b2 45 95 eb     | See hexdump    | Ticket ID                     |
|   0x1D8 |   0x04 | `00 00 00 00`                | 0x00000000     | Console ID                    |
|   0x1DC |   0x08 | `00 01 00 00 53 4c 53 50     | 00010000-SLSP  | Title ID / AES-CBC IV         |
|   0x1E4 |   0x02 | `ff ff`                      | 0xFFFF         | Unknown                       |
|   0x1E6 |   0x02 | `00 00`                      | 0x0000         | Ticket version                |
|   0x1E8 |   0x04 | `00 00 00 00`                | 0x00000000     | Permitted titles mask         |
|   0x1EC |   0x04 | `00 00 00 00`                | 0x00000000     | Permit mask                   |
|   0x1F0 |   0x01 | `00`                         | NOT allowed    | Title Export allowed?         |
|   0x1F1 |   0x01 | `00`                         | Dev Common     | Common key index              |
|   0x1F2 |   0x30 | Unknown; last byte is `04`   | **0x04**       | Unknown                       |
|   0x222 |   0x40 | Content access permissions   | See hexdump    | 1 bit per content             |
|   0x262 |   0x02 | `00 00`                      | N/A            | Padding                       |
|   0x264 |   0x04 | `00 00 00 00`                | Disabled       | Enable time limit?            |
|   0x268 |   0x04 | `00 00 00 00`                | None           | Time limit, in seconds        |
|   0x26C |   0x38 | Time limits                  | See hexdump    | More time limits              |

* 0x1F2: The last byte of this field is 0x04. For retail titles, this is
  usually 0, except for VC, in which case it's 1.

### TMD

($8382C0-$838400 is the RSA signature; this is left out of the hexdump.)

```
00838400  52 6f 6f 74 2d 43 41 30  30 30 30 30 30 30 32 2d  |Root-CA00000002-|
00838410  43 50 30 30 30 30 30 30  30 37 00 00 00 00 00 00  |CP00000007......|
00838420  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838430  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838440  00 00 00 00 00 00 00 01  00 00 00 38 00 01 00 00  |...........8....|
00838450  53 4c 53 50 00 00 00 01  30 31 00 00 00 00 00 00  |SLSP....01......|
00838460  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838470  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838480  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00838490  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 01  |................|
008384a0  00 00 00 00 
                      00 00 00 00  00 00 00 03 00 00 00 00  |................|
008384b0  00 00 00 04 93 4d b1 0d  ac fe 4f 41 0d 83 e8 e6  |.....M....OA....|
008384c0  45 67 67 b6 8b d9 1a 7a                           |Egg....z|
```

Addresses are relative to the start of the TMD. (0x2C0)

| Address | Length |             Data             |     Value      |          Description          |
|:-------:|:------:|------------------------------|----------------|-------------------------------|
|   0x000 |  0x13F | RSA signature                | See hexdump    | RSA signature                 |
|   0x140 |   0x40 | `Root-CA00000002-XS00000006` | Dev Ticket     | Signature issuer              |
|   0x180 |   0x01 | `00`                         | 0              | Version                       |
|   0x181 |   0x01 | `00`                         | 0              | ca_crl_version                |
|   0x182 |   0x01 | `00`                         | 0              | signer_crl_version            |
|   0x183 |   0x01 | `00`                         | 0              | Padding modulo 64             |
|   0x184 |   0x08 | `00 00 00 01 00 00 00 38`    | IOS56          | System version (IOS)          |
|   0x18C |   0x08 | `00 01 00 00 53 4c 53 50`    | 00010000-SLSP  | Title ID                      |
|   0x194 |   0x04 | `00 00 00 01`                | 1              | Title type                    |
|   0x198 |   0x02 | `30 31`                      | "01"           | Group ID                      |
|   0x19A |   0x3E | Reserved                     | See hexdump    | Reserved                      |
|   0x1D8 |   0x04 | `00 00 00 00`                | None           | Access rights                 |
|   0x1DC |   0x02 | `00 00`                      | v0             | Title version                 |
|   0x1DE |   0x02 | `00 01`                      | 1              | Number of contents            |
|   0x1E0 |   0x02 | `00 00`                      | 0              | Boot index                    |
|   0x1E2 |   0x02 | `00 00`                      | 0              | Padding modulo 64             |

At 0x1E4 is the first (and only) content entry for this partition:

| Address | Length |             Data             |     Value      |          Description          |
|:-------:|:------:|------------------------------|----------------|-------------------------------|
|   0x1E4 |    4   | `00 00 00 00`                | 0              | Content ID                    |
|   0x1E8 |    2   | `00 00`                      | 0              | Index                         |
|   0x1EA |    4   | `00 03`                      | 3              | Type                          |
|   0x1EC |    8   | `00 00 00 00 00 00 00 04`    | 4              | Size                          |
|   0x1E4 |   20   | See hexdump                  | See hexdump    | SHA-1 hash (H4)               |

SHA-1 hash: 934db10dacfe4f410d83e8e6456767b68bd91a7a

The TMD's SHA-1 hash is an H4 hash, and on retail and debug-encrypted discs,
this is a hash of the H3 table. Unencrypted images don't have an H3 table.
The coverage of this hash is currently unknown.

## Partition Contents

Partition disc header:

```
00840000  53 4c 53 50 30 31 00 00  00 00 00 00 00 00 00 00  |SLSP01..........|
00840010  00 00 00 00 00 00 00 00  5d 1c 9e a3 00 00 00 00  |........].......|
00840020  54 48 45 20 4c 41 53 54  20 53 54 4f 52 59 00 00  |THE LAST STORY..|
00840030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00840040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00840050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00840060  01 01 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

This is identical to the main disc header.

Boot block: (boot.bin)

```
00840420  00 00 ff c0 00 22 54 c0  00 01 95 56 00 01 95 56  |....."T....V...V|
00840430  80 39 aa a0 00 24 00 00  76 ba ff f8 00 00 00 00  |.9...$..v.......|
```

| Address | Length |             Data             |     Value      |          Description          |
|:-------:|:------:|------------------------------|----------------|-------------------------------|
|   0x420 |    4   | `00 00 ff c0`                | 0x00FFC0       | main.dol offset               |
|   0x424 |    4   | `00 22 54 c0`                | 0x2254C0       | FST offset                    |
|   0x428 |    4   | `00 01 95 56`                | 0x019556       | FST size                      |
|   0x42C |    4   | `00 01 95 56`                | 0x019556       | Max FST size                  |
|   0x430 |    4   | `80 39 aa a0`                | 0x8039aaa0     | ?                             |
|   0x434 |    4   | `00 24 00 00`                | 0x00240000     | ?                             |
|   0x438 |    4   | `76 ba ff f8`                | 0x76bafff8     | ?                             |
|   0x43C |    4   | `00 00 00 00`                | 0              | ?                             |

BI2.bin:

```
00840440  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00840450  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 01  |................|
00840460  00 00 00 01 00 00 00 05  00 00 00 00 08 00 00 00  |................|
00840470  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
```

| Address | Length |             Data             |     Value      |            Description            |
|:-------:|:------:|------------------------------|----------------|-----------------------------------|
|   0x440 |    4   | `00 00 00 00`                | 0              | Debug monitor size                |
|   0x444 |    4   | `00 00 00 00`                | 0              | Simulated memory size             |
|   0x448 |    4   | `00 00 00 00`                | 0              | Command line arguments offset     |
|   0x44C |    4   | `00 00 00 00`                | 0 (no)         | Debug flag                        |
|   0x450 |    4   | `00 00 00 00`                | 0              | Target resident kernel location   |
|   0x454 |    4   | `00 00 00 00`                | 0              | Size of target resident kernel    |
|   0x458 |    4   | `00 00 00 00`                | 0              | Region code (GCN only)            |
|   0x45C |    4   | `00 00 00 01`                | 1              | Number of DOL text/data sections? |
|   0x460 |    4   | `00 00 00 01`                | 1              | ?                                 |
|   0x464 |    4   | `00 00 00 05`                | 5              | ?                                 |
|   0x468 |    4   | `00 00 00 00`                | 0              | ?                                 |
|   0x46C |    4   | `08 00 00 00`                | 0x08000000     | ?                                 |
|   0x470 |    4   | `00 00 00 00`                | 0              | ?                                 |
|   0x474 |    4   | `00 00 00 00`                | 0              | ?                                 |
|   0x478 |    4   | `00 00 00 00`                | 0              | ?                                 |
|   0x47C |    4   | `00 00 00 00`                | 0              | ?                                 |
