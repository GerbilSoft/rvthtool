# RVT-H Reverse-Engineering Notes

These notes were written using a full dump of an RVT-H disk using the
front-panel USB port and an API trace of rvtwriter.exe 1.0.0.4.

## Test System

According to rvtwriter, this RVT-H has:

Bank 1: Empty
Bank 2: THE LAST STORY / 2011.11.25 10:18:15
Bank 3: ^ (dual-layer)
Bank 4: THE LAST STORY / 2011.11.28 10:30:09
Bank 5: ^ (dual-layer)
Bank 6: Empty
Bank 7: Empty
Bank 8: Empty

## NHCD Bank Table

At 0x60000000:

60000000  4e 48 43 44 00 00 00 01  00 00 00 08 00 00 00 00  |NHCD............|
60000010  00 2f f0 00 00 00 00 00  00 00 00 00 00 00 00 00  |./..............|
60000020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
60000400  4e 4e 32 4c 30 30 30 30  30 30 30 30 30 30 30 30  |NN2L000000000000|
60000410  30 30 32 30 31 31 31 31  32 35 31 30 31 38 31 35  |0020111125101815|
60000420  00 bc 4a 09 00 ee 00 00  00 00 00 00 00 00 00 00  |..J.............|
60000430  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
60000800  4e 4e 32 4c 30 30 30 30  30 30 30 30 30 30 30 30  |NN2L000000000000|
60000810  30 30 32 30 31 31 31 31  32 38 31 30 33 30 30 39  |0020111128103009|
60000820  01 d4 de 09 00 ee 00 00  00 00 00 00 00 00 00 00  |................|
60000830  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|

$60000000: Control struct.
typedef struct _NHCD_Header_t {
	char magic[4];		// [0x000] "NHCD"
	uint32_t x004;		// [0x004] 0x00000001
	uint32_t x008;		// [0x008] 0x00000008
	uint32_t x00C;		// [0x00C] 0x00000000
	uint32_t x010;		// [0x010] 0x002FF000
	uint8_t unk[492];	// [0x014] Unknown
} NHCD_Header_t;

$60000200: Bank 1. Each bank is 0x200 bytes.
typedef struct _NHCD_Bank_t {
	char type[4];		// [0x000] Type. (See below.)
	char all_zero[14];	// [0x004] All ASCII zeroes. ('0')
	char mdate[8];		// [0x012] Date stamp, in ASCII. ('20180112')
	char mtime[6];		// [0x01A] Time stamp, in ASCII. ('222720')
	uint32_t lba_start;	// [0x020] Starting LBA. (512-byte sectors)
	uint32_t lba_len;	// [0x024] Length, in 512-byte sectors.
	uint8_t unk[472];	// [0x028] Unknown
} NHCD_Bank_t;

Bank types:
- "GC1L": GameCube single-layer disc.
- "NN1L": Wii single-layer disc.
- "NN2L": Wii dual-layer disc.

Bank starting offsets are always the same. Length may differ depending on if
the game is GameCube, Wii single-layer, or Wii dual-layer.

Each bank is 0x8C4A00 sectors. (4,707,319,808 bytes)
- This matches the size of RVT-R and DVD-R discs.

The dual-layer games installed on this system take up 0xEE0000 sectors.
- 7,985,954,816 bytes
- These images are unencrypted, so encrypted DL images are probably larger.
- DVD-R DL discs are 0xFE9F00U sectors. (8,543,666,176 bytes)
- Retail Wii DL discs are 0xFDA700 sectors. (8,511,160,320 bytes)

Single-layer GCN disc image: 0x2B82C0 sectors (1,459,978,240 bytes)
- May be the same as NR discs.

The standard NHCD layout is as follows:
- Bank 1: 0x060001200 (LBA 0x00300009)
- Bank 2: 0x178941200 (LBA 0x00BC4A09)
- Bank 3: 0x291281200 (LBA 0x01489409)
- Bank 4: 0x3A9BC1200 (LBA 0x01D4DE09)
- Bank 5: 0x4C2501200 (LBA 0x02612809)
- Bank 6: 0x5DAE41200 (LBA 0x02ED7209)
- Bank 7: 0x6F3781200 (LBA 0x0379BC09)
- Bank 8: 0x80C0C1200 (LBA 0x04060609)
