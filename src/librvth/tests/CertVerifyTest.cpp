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

#if defined(_MSC_VER) && _MSC_VER < 1700
# define final sealed
#endif

namespace LibRvth { namespace Tests {

// Parameters for CtrKeyScrambler tests.
struct CertVerifyTest_mode
{
	RVL_Cert_Issuer cert_id;	// Certificate being tested.
	RVL_Cert_Issuer parent_id;	// Parent certificate.

	CertVerifyTest_mode(RVL_Cert_Issuer cert_id, RVL_Cert_Issuer parent_id)
		: cert_id(cert_id)
		, parent_id(parent_id)
	{ }
};

class CertVerifyTest : public ::testing::TestWithParam<CertVerifyTest_mode>
{
	protected:
		CertVerifyTest() { }

		void SetUp(void) final;
		void TearDown(void) final;
};

/**
 * SetUp() function.
 * Run before each test.
 */
void CertVerifyTest::SetUp(void)
{ }

/**
 * TearDown() function.
 * Run after each test.
 */
void CertVerifyTest::TearDown(void)
{ }

/**
 * Run a certificate verification test.
 */
TEST_P(CertVerifyTest, certVerifyTest)
{
	const CertVerifyTest_mode &mode = GetParam();

	// Get the certificate.
	const RVL_Cert *const cert = cert_get(mode.cert_id);
	const unsigned int cert_size = cert_get_size(mode.cert_id);
	ASSERT_TRUE(cert != nullptr);
	ASSERT_NE(0U, cert_size);

	// Verify the certificate.
	ASSERT_EQ(0, cert_verify(reinterpret_cast<const uint8_t*>(cert), cert_size));
}

/** Certificate verification tests. **/

INSTANTIATE_TEST_CASE_P(certVerifyTest, CertVerifyTest,
	::testing::Values(
		CertVerifyTest_mode(RVL_CERT_ISSUER_RETAIL_TICKET, RVL_CERT_ISSUER_RETAIL_CA),
		CertVerifyTest_mode(RVL_CERT_ISSUER_RETAIL_TMD, RVL_CERT_ISSUER_RETAIL_CA)
	));
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
