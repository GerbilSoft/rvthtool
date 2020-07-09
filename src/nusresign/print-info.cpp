/***************************************************************************
 * RVT-H Tool: WAD Resigner                                                *
 * print-info.c: Print WAD information.                                    *
 *                                                                         *
 * Copyright (c) 2018-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "print-info.hpp"

// libwiicrypto
#include "libwiicrypto/aesw.h"
#include "libwiicrypto/byteswap.h"
#include "libwiicrypto/cert.h"
#include "libwiicrypto/sig_tools.h"
#include "libwiicrypto/wiiu_structs.h"

// Nettle
#include <nettle/sha1.h>
#include <nettle/sha2.h>

// C includes.
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ includes.
#include <memory>
#include <string>
using std::tstring;
using std::unique_ptr;

// Buffer size for verifying contents.
#define READ_BUFFER_SIZE (1024*1024)

/**
 * Is an issuer retail or debug?
 * @param issuer RVL_Cert_Issuer
 * @return "Retail", "Debug", or "Unknown".
 */
static const char *issuer_type(RVL_Cert_Issuer issuer)
{
	switch (issuer) {
		default:
		case RVL_CERT_ISSUER_UNKNOWN:
			return "Unknown";

		// NOTE: CTR certs are needed here because
		// CTR and WUP share certificates.
		case RVL_CERT_ISSUER_DPKI_ROOT:
		case CTR_CERT_ISSUER_DPKI_CA:
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case CTR_CERT_ISSUER_DPKI_TMD:
		case WUP_CERT_ISSUER_DPKI_CA:
		case WUP_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TMD:
		case WUP_CERT_ISSUER_DPKI_SP:
			return "Debug";

		case RVL_CERT_ISSUER_PPKI_ROOT:
		case CTR_CERT_ISSUER_PPKI_CA:
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case CTR_CERT_ISSUER_PPKI_TMD:
		case WUP_CERT_ISSUER_PPKI_CA:
		case WUP_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TMD:
			return "Retail";
	}
}

/**
 * Verify a content entry.
 * @param nus_dir	[in] NUS directory.
 * @param title_key	[in] Decrypted title key.
 * @param entry		[in] Content entry.
 * @return 0 if the content is verified; 1 if not; negative POSIX error code on error.
 */
static int verify_content(const TCHAR *nus_dir, const uint8_t title_key[16], const WUP_Content_Entry *entry)
{
	// Construct the filenames.
	// FIXME: Content ID or content index?
	// Assuming content ID for filename, content index for IV.
	TCHAR cidbuf[16];
	_sntprintf(cidbuf, ARRAY_SIZE(cidbuf), _T("%08x"), be32_to_cpu(entry->content_id));
	tstring sf_app = nus_dir;
	sf_app += DIR_SEP_CHR;
	sf_app += cidbuf;
	sf_app += _T(".app");

	const bool hasH3 = !!(entry->type & cpu_to_be16(0x0002));
	tstring sf_h3;
	if (hasH3) {
		// H3 file is present.
		// TODO: How is it checked?
		sf_h3 = nus_dir;
		sf_h3 += DIR_SEP_CHR;
		sf_h3 += cidbuf;
		sf_h3 += _T(".h3");
	}

	FILE *f_content = _tfopen(sf_app.c_str(), _T("rb"));
	if (!f_content) {
		// Error opening the content file.
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		printf("- *** ERROR opening %s.app: %s\n", cidbuf, strerror(err));
		return -err;
	}

	// H3 table depends on the size of the contents.
	// One H3 hash == 256 MB data
	unique_ptr<uint8_t[]> hash_h3;
	size_t hash_h3_len = 0;
	if (hasH3) {
		FILE *f_h3 = _tfopen(sf_h3.c_str(), _T("rb"));
		if (!f_h3) {
			// Error opening the H3 file.
			fclose(f_content);
			int err = errno;
			if (err == 0) {
				err = EIO;
			}
			printf("- *** ERROR opening %s.h3: %s\n", cidbuf, strerror(err));
			return -err;
		}

		// Get the size.
		// Should be at least SHA1_DIGEST_SIZE and a multiple of SHA1_DIGEST_SIZE.
		// Maximum of 256*20 bytes for the H3 file, or 64 GB of coverage.
		fseeko(f_h3, 0, SEEK_END);
		hash_h3_len = ftello(f_h3);
		if (hash_h3_len == 0 || hash_h3_len % SHA1_DIGEST_SIZE != 0 || hash_h3_len > (SHA1_DIGEST_SIZE * 256)) {
			// Invalid size.
			fclose(f_h3);
			fclose(f_content);
			printf("- *** ERROR reading %s.h3: Size is incorrect\n", cidbuf);
			return -EIO;
		}

		rewind(f_h3);
		hash_h3.reset(new uint8_t[hash_h3_len]);
		errno = 0;
		size_t size = fread(hash_h3.get(), 1, hash_h3_len, f_h3);
		if (size != hash_h3_len) {
			// Read error.
			int err = errno;
			if (errno == 0) {
				err = EIO;
			}
			fclose(f_content);
			printf("- *** ERROR reading %s.h3: %s\n", cidbuf, strerror(err));
			return -err;
		}
	}

	AesCtx *const aesw = aesw_new();
	if (!aesw) {
		// Error initializing AES...
		fclose(f_content);
		return -ENOMEM;
	}

	// IV is the 2-byte content index, followed by zeroes.
	uint8_t iv[16];
	memcpy(iv, &entry->index, 2);
	memset(&iv[2], 0, 14);
	aesw_set_key(aesw, title_key, 16);
	aesw_set_iv(aesw, iv, sizeof(iv));

	// Read buffer.
	unique_ptr<uint8_t[]> buf(new uint8_t[READ_BUFFER_SIZE]);

	// Read the content, decrypt it, and hash it.
	// TODO: Verify size; check fseeko() errors.
	int64_t data_sz = be64_to_cpu(entry->size);

	int ret = 0;
	if (!hasH3) {
		// No H3 table. A single SHA-1 is used for the whole content.
		struct sha1_ctx sha1;
		sha1_init(&sha1);
		for (; data_sz >= READ_BUFFER_SIZE; data_sz -= READ_BUFFER_SIZE) {
			errno = 0;
			size_t size = fread(buf.get(), 1, READ_BUFFER_SIZE, f_content);
			if (size != READ_BUFFER_SIZE) {
				ret = errno;
				if (ret == 0) {
					ret = -EIO;
				}
				goto end;
			}

			// Decrypt the data.
			aesw_decrypt(aesw, buf.get(), READ_BUFFER_SIZE);

			// Update the SHA-1.
			sha1_update(&sha1, READ_BUFFER_SIZE, buf.get());
		}

		// Remaining data.
		if (data_sz > 0) {
			// NOTE: AES works on 16-byte blocks, so we have to
			// read and decrypt the full 16-byte block. The SHA-1
			// is only taken for the actual used data, though.
			uint32_t data_sz_align = ALIGN_BYTES(16, data_sz);

			errno = 0;
			size_t size = fread(buf.get(), 1, data_sz_align, f_content);
			if (size != data_sz_align) {
				ret = errno;
				if (ret == 0) {
					ret = -EIO;
				}
				goto end;
			}

			// Decrypt the data.
			aesw_decrypt(aesw, buf.get(), data_sz_align);

			// Update the SHA-1.
			// NOTE: Only uses the actual content, not the
			// aligned data required for decryption.
			sha1_update(&sha1, data_sz, buf.get());
		}

		// Finalize the SHA-1 and compare it.
		uint8_t digest[SHA1_DIGEST_SIZE];
		sha1_digest(&sha1, sizeof(digest), digest);
		fputs("- Expected SHA-1: ", stdout);
		for (size_t size = 0; size < sizeof(entry->sha1_hash); size++) {
			printf("%02x", entry->sha1_hash[size]);
		}
		putchar('\n');
		printf("- Actual SHA-1:   ");
		for (size_t size = 0; size < sizeof(digest); size++) {
			printf("%02x", digest[size]);
		}
		if (!memcmp(digest, entry->sha1_hash, SHA1_DIGEST_SIZE)) {
			fputs(" [OK]\n", stdout);
		} else {
			fputs(" [ERROR]\n", stdout);
			ret = 1;
		}
	} else {
		// Has an H3 table. Content is encrypted in 64 KB blocks.
		// H3 hash is the hash of all H2 tables for every 256 MB block.
		// IV starts in hashes[block number % 16].
		// TODO: Verify that the content is a multiple of 64 KB?
		struct EncBlock {
			// One hash block covers a 1 MB superblock.
			struct {
				// 16 H0 hashes, each of which covers one 64 KB block.
				// For every megabyte of data, all 64 KB blocks have the same H0 hashes.
				uint8_t h0[16][SHA1_DIGEST_SIZE];
				// 16 H1 hashes, each of which covers the H0 table for a given 1 MB block.
				// For every 16 MB of data, all 64 KB blocks have the same H1 hashes.
				uint8_t h1[16][SHA1_DIGEST_SIZE];
				// 16 H2 hashes, each of which covers the H1 table for a given 16 MB block.
				// For every 256 MB of data, all 64 KB blocks have the same H2 hashes.
				uint8_t h2[16][SHA1_DIGEST_SIZE];

				// Unused
				uint8_t unused[64];
			} hashes;
			uint8_t data[0xFC00];
		};

		// H0 == hash of a single block
		// H1 == hash of 16 H0 hashes
		// H2 == hash of 16 H1 hashes
		// H3 == hash of all H2 hashes
		// H4 == hash of the H3 hash, stored in the content entry

		// Were any bad hashes found?
		unsigned int bad_hash[4] = {0, 0, 0, 0};

		#define ENC_BLOCK_SIZE 0x10000
		#define DEC_BLOCK_SIZE 0xFC00

		// Zero IV for hashes.
		uint8_t zero_iv[16];
		memset(zero_iv, 0, sizeof(zero_iv));

		EncBlock *const block = reinterpret_cast<EncBlock*>(buf.get());
		unsigned int block_number = 0;
		for (; data_sz >= ENC_BLOCK_SIZE; data_sz -= ENC_BLOCK_SIZE, block_number++) {
			sha1_ctx sha1;
			uint8_t digest[SHA1_DIGEST_SIZE];

			errno = 0;
			size_t size = fread(block, 1, sizeof(*block), f_content);
			if (size != sizeof(*block)) {
				ret = errno;
				if (ret == 0) {
					ret = -EIO;
				}
				goto end;
			}

			// Decrypt the hashes. (zero IV)
			aesw_set_iv(aesw, zero_iv, sizeof(zero_iv));
			aesw_decrypt(aesw, reinterpret_cast<uint8_t*>(&block->hashes), sizeof(block->hashes));

			// Decrypt the data.
			// IV is one of the decrypted hashes.
			const uint8_t *const pHashH0_expected = block->hashes.h0[block_number % 16];
			aesw_set_iv(aesw, pHashH0_expected, 16);
			aesw_decrypt(aesw, block->data, sizeof(block->data));

			// Verify the H0 hash.
			sha1_init(&sha1);
			sha1_update(&sha1, sizeof(block->data), block->data);
			sha1_digest(&sha1, sizeof(digest), digest);
			if (memcmp(digest, pHashH0_expected, sizeof(digest)) != 0) {
				// TODO: Print an error here?
				//fprintf(stderr, "- *** ERROR: H0 hash bad at block %u\n", block_number);
				bad_hash[0]++;
				ret = 1;
			}

			if (block_number % 16 == 0) {
				// Verify the H1 hash. (New H0 table)
				// TODO: Verify that the other identical H0 hash tables match.
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(block->hashes.h0), &block->hashes.h0[0][0]);
				sha1_digest(&sha1, sizeof(digest), digest);

				unsigned int h1_idx = (block_number / 16) % 16;
				if (memcmp(digest, block->hashes.h1[h1_idx], sizeof(digest))) {
					bad_hash[1]++;
				}
			}

			if (block_number % (16*16) == 0) {
				// Verify the H2 hash. (New H1 table)
				// TODO: Verify that the other identical H1 hash tables match.
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(block->hashes.h1), &block->hashes.h1[0][0]);
				sha1_digest(&sha1, sizeof(digest), digest);

				unsigned int h2_idx = (block_number / (16*16)) % 16;
				if (memcmp(digest, block->hashes.h2[h2_idx], sizeof(digest))) {
					bad_hash[2]++;
				}
			}

			if (block_number % (16*16*16) == 0) {
				// Verify the H3 hash. (New H2 table)
				// TODO: Verify that the other identical H2 hash tables match.
				sha1_init(&sha1);
				sha1_update(&sha1, sizeof(block->hashes.h2), &block->hashes.h2[0][0]);
				sha1_digest(&sha1, sizeof(digest), digest);

				unsigned int h3_byte_pos = (block_number / (16*16*16)) * SHA1_DIGEST_SIZE;
				if (h3_byte_pos + SHA1_DIGEST_SIZE > hash_h3_len) {
					// Out of bounds...
					bad_hash[3]++;
				} else {
					if (memcmp(digest, &hash_h3[h3_byte_pos], sizeof(digest))) {
						bad_hash[3]++;
					}
				}
			}
		}

		bool showH4status = true;
		for (unsigned int i = 0; i < 3; i++) {
			if (bad_hash[i] != 0) {
				printf("- ERROR: %u H%u hash(es) were incorrect.\n", bad_hash[i], i);
				showH4status = false;
				ret = 1;
			}
		}

		// Verify the H4 SHA-1, which is stored in the content entry.
		uint8_t digest[SHA1_DIGEST_SIZE];
		sha1_ctx sha1_h4;
		sha1_init(&sha1_h4);
		sha1_update(&sha1_h4, hash_h3_len, hash_h3.get());
		sha1_digest(&sha1_h4, sizeof(digest), digest);
		fputs("- Expected SHA-1: ", stdout);
		for (size_t size = 0; size < sizeof(entry->sha1_hash); size++) {
			printf("%02x", entry->sha1_hash[size]);
		}
		fputs(" (H4)\n", stdout);
		printf("- Actual SHA-1:   ");
		for (size_t size = 0; size < sizeof(digest); size++) {
			printf("%02x", digest[size]);
		}
		if (showH4status) {
			if (!memcmp(digest, entry->sha1_hash, SHA1_DIGEST_SIZE)) {
				fputs(" [OK] (H4)\n", stdout);
			} else {
				fputs(" [ERROR] (H4)\n", stdout);
				ret = 1;
			}
		} else {
			fputs(" (H4)\n", stdout);
		}
	}

end:
	fclose(f_content);
	aesw_free(aesw);
	return ret;
}

/**
 * 'info' command.
 * @param nus_dir	[in] NUS directory.
 * @param verify	[in] If true, verify the contents. [TODO]
 * @return 0 on success; negative POSIX error code or positive ID code on error.
 */
int print_nus_info(const TCHAR *nus_dir, bool verify)
{
	// Construct the filenames.
	tstring sf_tik = nus_dir;
	sf_tik += DIR_SEP_CHR;
	sf_tik += _T("title.tik");

	tstring sf_tmd = nus_dir;
	sf_tmd += DIR_SEP_CHR;
	sf_tmd += _T("title.tmd");

	// Open the ticket and TMD.
	FILE *f_tik = _tfopen(sf_tik.c_str(), _T("rb"));
	if (!f_tik) {
		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR opening ticket file: %s\n", strerror(err));
		return -err;
	}

	FILE *f_tmd = _tfopen(sf_tmd.c_str(), _T("rb+"));
	if (!f_tmd) {
		fclose(f_tik);

		int err = errno;
		if (err == 0) {
			err = EIO;
		}
		fprintf(stderr, "*** ERROR opening TMD file: %s\n", strerror(err));
		return -err;
	}

	// Get the ticket and TMD sizes.
	fseeko(f_tik, 0, SEEK_END);
	const size_t tik_size = ftello(f_tik);
	if (tik_size < sizeof(WUP_Ticket)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading ticket file: Too small.\n");
		return -EIO;
	} else if (tik_size > (64*1024)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading ticket file: Too big.\n");
		return -EIO;
	}
	fseeko(f_tmd, 0, SEEK_END);
	const size_t tmd_size = ftello(f_tmd);
	if (tmd_size < (sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable))) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading TMD file: Too small.\n");
		return -EIO;
	} else if (tmd_size > (128*1024)) {
		fclose(f_tik);
		fclose(f_tmd);
		fprintf(stderr, "*** ERROR reading TMD file: Too big.\n");
		return -EIO;
	}
	rewind(f_tik);
	rewind(f_tmd);

	// Read the ticket and TMD.
	// TODO: Check for errors?
	unique_ptr<uint8_t[]> tik_data(new uint8_t[tik_size]);
	fread(tik_data.get(), 1, tik_size, f_tik);
	fclose(f_tik);
	WUP_Ticket *const pTicket = reinterpret_cast<WUP_Ticket*>(tik_data.get());

	unique_ptr<uint8_t[]> tmd_data(new uint8_t[tmd_size]);
	fread(tmd_data.get(), 1, tmd_size, f_tmd);
	fclose(f_tmd);
	WUP_TMD_Header *const pTmdHeader = reinterpret_cast<WUP_TMD_Header*>(tmd_data.get());

	// NOTE: Using TMD for most information.
	_tprintf(_T("%s:\n"), nus_dir);
	// FIXME: Get the actual Wii U application type.
	//printf("Type: %08x\n", be32_to_cpu(pTmdHeader->rvl.title_type));
	printf("- Title ID:      %08X-%08X\n", be32_to_cpu(pTmdHeader->rvl.title_id.hi), be32_to_cpu(pTmdHeader->rvl.title_id.lo));

	// TODO: Figure out where the game ID is stored.

	// Title version (TODO: Wii U changes?)
	uint16_t title_version = be16_to_cpu(pTmdHeader->rvl.title_version);
	printf("- Title version: %u.%u (v%u)\n",
		title_version >> 8, title_version & 0xFF, title_version);

	// OS version
	// TODO: Error message if not an OS title?
	uint8_t os_version = 0;
	if (be32_to_cpu(pTmdHeader->rvl.sys_version.hi) == 0x00050010) {
		const uint32_t ios_tid_lo = be32_to_cpu(pTmdHeader->rvl.sys_version.lo);
		const uint32_t tid24 = (ios_tid_lo >> 8);
		if (tid24 == 0x100040 || tid24 == 0x100080) {
			os_version = (uint8_t)ios_tid_lo;
		}
	}
	printf("- OS version:    OSv%u\n", os_version);

	// Determine the encryption key in use.
	// NOTE: May use CTR since CTR and WUP have the same certificates.
	RVL_Cert_Issuer issuer_ticket = cert_get_issuer_from_name(pTicket->issuer);
	RVL_AES_Keys_e encKey;
	const char *s_encKey;
	switch (issuer_ticket) {
		default:	// TODO: Show an error instead?
		case CTR_CERT_ISSUER_PPKI_TICKET:
		case WUP_CERT_ISSUER_PPKI_TICKET:
			encKey = WUP_KEY_RETAIL;
			s_encKey = "Retail";
			break;
		case CTR_CERT_ISSUER_DPKI_TICKET:
		case WUP_CERT_ISSUER_DPKI_TICKET:
			encKey = WUP_KEY_DEBUG;
			s_encKey = "Debug";
			break;
	}
	printf("- Encryption:    %s\n", s_encKey);

	// Check the ticket issuer and signature.
	// NOTE: The ticket might have a certificate chain appended,
	// so we'll only check the actual ticket data.
	const char *const s_issuer_ticket = issuer_type(issuer_ticket);
	RVL_SigStatus_e sig_status_ticket = sig_verify(tik_data.get(), sizeof(WUP_Ticket));
	printf("- Ticket Signature: %s%s\n",
		s_issuer_ticket, RVL_SigStatus_toString_stsAppend(sig_status_ticket));

	// Check the TMD issuer and signature.
	// NOTE: TMD signature only covers the TMD header.
	// We'll need to check the content info hash afterwards.
	const char *const s_issuer_tmd = issuer_type(cert_get_issuer_from_name(pTmdHeader->rvl.issuer));
	RVL_SigStatus_e sig_status_tmd = sig_verify(tmd_data.get(), sizeof(WUP_TMD_Header));
	if (sig_status_tmd == RVL_SigStatus_OK) {
		// Verify the content info hash.
		const WUP_TMD_ContentInfoTable *const cinfotbl =
			reinterpret_cast<const WUP_TMD_ContentInfoTable*>(&tmd_data[sizeof(WUP_TMD_Header)]);
		struct sha256_ctx sha256;
		uint8_t digest[SHA256_DIGEST_SIZE];
		sha256_init(&sha256);
		sha256_update(&sha256, sizeof(*cinfotbl), reinterpret_cast<const uint8_t*>(cinfotbl));
		sha256_digest(&sha256, sizeof(digest), digest);

		if (!memcmp(digest, pTmdHeader->contentInfo_sha256, sizeof(digest))) {
			// Check the hashes of each content info section.
			const WUP_ContentInfo *cinfo = cinfotbl->info;
			const WUP_ContentInfo *const cinfo_end = &cinfo[WUP_CONTENTINFO_ENTRIES];

			size_t cstart = sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable);
			for (; cinfo < cinfo_end; cinfo++) {
				// Check the number of commands and hash that many entries.
				const unsigned int indexOffset = be16_to_cpu(cinfo->indexOffset);
				const unsigned int commandCount = be16_to_cpu(cinfo->commandCount);
				if (indexOffset == 0 && commandCount == 0) {
					// End of table.
					break;
				}

				const size_t pos = cstart + (indexOffset * sizeof(WUP_Content_Entry));
				const size_t len = (commandCount * sizeof(WUP_Content_Entry));
				if (pos + len > tmd_size) {
					// Out of bounds.
					sig_status_tmd = RVL_SigStatus_Invalid;
					break;
				}

				// Hash the contents.
				sha256_init(&sha256);
				sha256_update(&sha256, len, &tmd_data[pos]);
				sha256_digest(&sha256, sizeof(digest), digest);
				if (memcmp(digest, cinfo->sha256, sizeof(digest)) != 0) {
					// Content hashes are invalid.
					// TODO: Show a specific error?
					sig_status_tmd = RVL_SigStatus_Invalid;
					break;
				}
			}
		} else {
			// Content info table hash is invalid.
			// TODO: Show a specific error?
			sig_status_tmd = RVL_SigStatus_Invalid;
		}
	}

	printf("- TMD Signature:    %s%s\n",
		s_issuer_tmd, RVL_SigStatus_toString_stsAppend(sig_status_tmd));

	putchar('\n');

	// Decrypted title key for contents verification.
	uint8_t title_key[16];
	if (verify) {
		AesCtx *const aesw = aesw_new();
		if (aesw) {
			// IV is the 64-bit title ID, followed by zeroes.
			uint8_t iv[16];
			memcpy(iv, &pTicket->title_id, 8);
			memset(&iv[8], 0, 8);

			// Decrypt the title key.
			memcpy(title_key, pTicket->enc_title_key, sizeof(title_key));
			aesw_set_key(aesw, RVL_AES_Keys[encKey], 16);
			aesw_set_iv(aesw, iv, sizeof(iv));
			aesw_decrypt(aesw, title_key, sizeof(title_key));
		} else {
			// TODO: Print a warning message indicating we can't decrypt.
			verify = false;
		}
	}

	// Print the TMD contents info.
	// TODO: Show the separate contents tables?
	// We're lumping everything together right now.
	const uint16_t boot_index = be16_to_cpu(pTmdHeader->rvl.boot_index);
	const WUP_TMD_ContentInfoTable *const cinfotbl =
		reinterpret_cast<const WUP_TMD_ContentInfoTable*>(&tmd_data[sizeof(WUP_TMD_Header)]);
	const WUP_ContentInfo *cinfo = cinfotbl->info;
	const WUP_ContentInfo *const cinfo_end = &cinfo[WUP_CONTENTINFO_ENTRIES];

	size_t cstart = sizeof(WUP_TMD_Header) + sizeof(WUP_TMD_ContentInfoTable);
	int ret = 0;
	for (; cinfo < cinfo_end; cinfo++) {
		const unsigned int indexOffset = be16_to_cpu(cinfo->indexOffset);
		const unsigned int commandCount = be16_to_cpu(cinfo->commandCount);
		if (indexOffset == 0 && commandCount == 0) {
			// End of table.
			break;
		}

		const size_t pos = cstart + (indexOffset * sizeof(WUP_Content_Entry));
		const size_t len = (commandCount * sizeof(WUP_Content_Entry));
		if (pos + len > tmd_size) {
			// Out of bounds.
			continue;
		}

		// Print the entries.
		const WUP_Content_Entry *p =
			reinterpret_cast<const WUP_Content_Entry*>(&tmd_data[pos]);
		const WUP_Content_Entry *const p_end = &p[commandCount];
		for (; p < p_end; p++) {
			// TODO: Show the actual table index, or just the
			// index field in the entry?
			uint16_t content_index = be16_to_cpu(p->index);
			printf("#%d: ID=%08x, type=%04X, size=%u",
				be16_to_cpu(p->index),
				be32_to_cpu(p->content_id),
				be16_to_cpu(p->type),
				(uint32_t)be64_to_cpu(p->size));
			if (content_index == boot_index) {
				fputs(", bootable", stdout);
			}
			putchar('\n');

			if (verify) {
				// Verify the content.
				// TODO: Only decrypt the title key once?
				// TODO: Return failure if any contents fail.
				int vret = verify_content(nus_dir, title_key, p);
				if (vret != 0) {
					ret = 1;
				}
			}
		}
	}
	putchar('\n');

	return ret;
}
