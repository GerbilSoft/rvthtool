/***************************************************************************
 * RVT-H Tool (libwiicrypto/tests)                                         *
 * CertVerifyTest.cpp: Certificate verification test.                      *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// Google Test
#include "gtest/gtest.h"

#include "libwiicrypto/cert_store.h"
#include "libwiicrypto/cert.h"

// C includes. (C++ namespace)
#include <cassert>

// C++ includes.
#include <string>
using std::string;

#if defined(_MSC_VER) && _MSC_VER < 1700
# define final sealed
#endif

namespace LibWiiCrypto { namespace Tests {

class CertVerifyTest : public ::testing::TestWithParam<RVL_Cert_Issuer>
{
protected:
	CertVerifyTest() = default;

public:
	/** Test case parameters **/

	/**
	 * Test case suffix generator.
	 * @param info Test parameter information.
	 * @return Test case suffix.
	 */
	static string test_case_suffix_generator(const ::testing::TestParamInfo<RVL_Cert_Issuer> &info);
};

/**
 * Formatting function for ImageDecoderTest.
 */
inline ::std::ostream& operator<<(::std::ostream& os, const RVL_Cert_Issuer &cert_id)
{
	assert(cert_id > RVL_CERT_ISSUER_UNKNOWN);
	assert(cert_id < RVL_CERT_ISSUER_MAX);
	if (cert_id > RVL_CERT_ISSUER_UNKNOWN && cert_id < RVL_CERT_ISSUER_MAX) {
		return os << RVL_Cert_Issuers[cert_id];
	}
	return os << "(unknown)";
};

/**
 * Run a certificate verification test.
 */
TEST_P(CertVerifyTest, certVerifyTest)
{
	const RVL_Cert_Issuer cert_id = GetParam();

	// Get the certificate.
	const RVL_Cert *const cert = cert_get(cert_id);
	const unsigned int cert_size = cert_get_size(cert_id);
	ASSERT_TRUE(cert != nullptr);
	ASSERT_NE(0U, cert_size);

	// Verify the certificate.
	ASSERT_EQ(0, cert_verify(reinterpret_cast<const uint8_t*>(cert), cert_size));
}

/**
 * Test case suffix generator.
 * @param info Test parameter information.
 * @return Test case suffix.
 */
string CertVerifyTest::test_case_suffix_generator(const ::testing::TestParamInfo<RVL_Cert_Issuer> &info)
{
	string suffix;
	const RVL_Cert_Issuer cert_id = info.param;

	// TODO: Print the user-friendly name instead of the certificate name?
	assert(cert_id > RVL_CERT_ISSUER_UNKNOWN);
	assert(cert_id < RVL_CERT_ISSUER_MAX);
	if (cert_id > RVL_CERT_ISSUER_UNKNOWN && cert_id < RVL_CERT_ISSUER_MAX) {
		suffix = RVL_Cert_Issuers[cert_id];
	} else {
		suffix = "unknown";
	}

	// Replace all non-alphanumeric characters with '_'.
	// See gtest-param-util.h::IsValidParamName().
	for (int i = (int)suffix.size()-1; i >= 0; i--) {
		char chr = suffix[i];
		if (!isalnum(chr) && chr != '_') {
			suffix[i] = '_';
		}
	}

	return suffix;
}

/** Certificate verification tests. **/

INSTANTIATE_TEST_CASE_P(certVerifyTest, CertVerifyTest,
	::testing::Values(
		RVL_CERT_ISSUER_DPKI_CA,
		RVL_CERT_ISSUER_DPKI_TICKET,
		RVL_CERT_ISSUER_DPKI_TMD,
		RVL_CERT_ISSUER_DPKI_MS,
		RVL_CERT_ISSUER_DPKI_XS04,
		RVL_CERT_ISSUER_DPKI_CP05,

		RVL_CERT_ISSUER_PPKI_CA,
		RVL_CERT_ISSUER_PPKI_TICKET,
		RVL_CERT_ISSUER_PPKI_TMD,

		CTR_CERT_ISSUER_DPKI_CA,
		CTR_CERT_ISSUER_DPKI_TICKET,
		CTR_CERT_ISSUER_DPKI_TMD,

		CTR_CERT_ISSUER_PPKI_CA,
		CTR_CERT_ISSUER_PPKI_TICKET,
		CTR_CERT_ISSUER_PPKI_TMD,

		//WUP_CERT_ISSUER_DPKI_CA,	// same as 3DS
		WUP_CERT_ISSUER_DPKI_TICKET,
		WUP_CERT_ISSUER_DPKI_TMD,
		WUP_CERT_ISSUER_DPKI_SP

		//WUP_CERT_ISSUER_PPKI_CA,	// same as 3DS
		//WUP_CERT_ISSUER_PPKI_TICKET,	// same as 3DS
		//WUP_CERT_ISSUER_PPKI_TMD	// same as 3DS
	), CertVerifyTest::test_case_suffix_generator);
} }

#ifdef _MSC_VER
# define RVTH_CDECL __cdecl
#else
# define RVTH_CDECL
#endif

/**
 * Test suite main function.
 */
int RVTH_CDECL main(int argc, char *argv[])
{
	fprintf(stderr, "libwiicrypto test suite: Certification verification tests.\n\n");
	fflush(nullptr);

	// coverity[fun_call_w_exception]: uncaught exceptions cause nonzero exit anyway, so don't warn.
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
