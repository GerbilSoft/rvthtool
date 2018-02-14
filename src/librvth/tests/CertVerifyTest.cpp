/***************************************************************************
 * RVT-H Tool (librvth/tests)                                              *
 * CertVerifyTest.cpp: Certificate verification test.                      *
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

// Google Test
#include "gtest/gtest.h"

#include "librvth/cert_store.h"
#include "librvth/cert.h"

// C includes. (C++ namespace)
#include <cassert>

// C++ includes.
#include <string>
using std::string;

#if defined(_MSC_VER) && _MSC_VER < 1700
# define final sealed
#endif

namespace LibRvth { namespace Tests {

class CertVerifyTest : public ::testing::TestWithParam<RVL_Cert_Issuer>
{
	protected:
		CertVerifyTest() { }

	public:
		/** Test case parameters. **/

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
		//RVL_CERT_ISSUER_DEBUG_CA,	// BROKEN
		RVL_CERT_ISSUER_DEBUG_TICKET,
		RVL_CERT_ISSUER_DEBUG_TMD,
		RVL_CERT_ISSUER_DEBUG_DEV,

		RVL_CERT_ISSUER_RETAIL_CA,
		RVL_CERT_ISSUER_RETAIL_TICKET,
		RVL_CERT_ISSUER_RETAIL_TMD
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
	fprintf(stderr, "librvth test suite: Certification verification tests.\n\n");
	fflush(nullptr);

	// coverity[fun_call_w_exception]: uncaught exceptions cause nonzero exit anyway, so don't warn.
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
