/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * rsaw_generic.c: RSA encryption wrapper functions. (generic version)     *
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

#include "rsaw.h"
#include "byteswap.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

/** BEGIN: Generic Big Number functions from Wiimm's ISO Tools v3.01 (r7464) **/
// https://wit.wiimm.de/

typedef uint8_t u8;
typedef uint32_t u32;

//
///////////////////////////////////////////////////////////////////////////////
///////////////			bn helpers			///////////////
///////////////////////////////////////////////////////////////////////////////

#define bn_zero(a,b)      memset(a,0,b)
#define bn_copy(a,b,c)    memcpy(a,b,c)
#define bn_compare(a,b,c) memcmp(a,b,c)

///////////////////////////////////////////////////////////////////////////////
// calc a = a mod N, given n = size of a,N in bytes

static void bn_sub_modulus ( u8 *a, const u8 *N, const u32 n )
{
    u32 i;
    u32 dig;
    u8 c;

    c = 0;
    for (i = n - 1; i < n; i--) {
	dig = N[i] + c;
	c = (a[i] < dig);
	a[i] -= dig;
    }
}

///////////////////////////////////////////////////////////////////////////////
// calc d = (a + b) mod N, given n = size of d,a,b,N in bytes

static void bn_add ( u8 *d, const u8 *a, const u8 *b, const u8 *N, const u32 n )
{
    u32 i;
    u32 dig;
    u8 c;

    c = 0;
    for (i = n - 1; i < n; i--)
    {
	dig = a[i] + b[i] + c;
	c = (dig >= 0x100);
	d[i] = dig;
    }

    if (c)
	bn_sub_modulus(d, N, n);

    if (bn_compare(d, N, n) >= 0)
	bn_sub_modulus(d, N, n);
}

///////////////////////////////////////////////////////////////////////////////
// calc d = (a * b) mod N, given n = size of d,a,b,N in bytes

static void bn_mul ( u8 *d, const u8 *a, const u8 *b, const u8 *N, const u32 n )
{
    u32 i;
    u8 mask;

    bn_zero(d, n);

    for (i = 0; i < n; i++)
	for (mask = 0x80; mask != 0; mask >>= 1)
	{
	    bn_add(d, d, d, N, n);
	    if ((a[i] & mask) != 0)
		bn_add(d, d, b, N, n);
	}
}

///////////////////////////////////////////////////////////////////////////////
// calc d = (a ^ e) mod N, given n = size of d,a,N and en = size of e in bytes

static void bn_exp(u8 *d, const u8 *a, const u8 *N, const u32 n, const u8 *e, const u32 en)
{
    u8 t[512];
    u32 i;
    u8 mask;

    bn_zero(d, n);
    d[n-1] = 1;
    for (i = 0; i < en; i++)
	for ( mask = 0x80; mask != 0; mask >>= 1 )
	{
	    bn_mul(t, d, d, N, n);
	    if ((e[i] & mask) != 0)
		bn_mul(d, t, a, N, n);
	    else
		bn_copy(d, t, n);
	}
}

/** END: Generic Big Number functions from Wiimm's ISO Tools v3.01 (r7464) **/

/**
 * Decrypt an RSA signature.
 * @param buf		[out] Output buffer. (Must be `size` bytes.)
 * @param modulus	[in] Public key modulus. (Must be `size` bytes.)
 * @param exponent	[in] Public key exponent.
 * @param sig		[in] Signature. (Must be `size` bytes.)
 * @param size		[in] Signature size. (256 for RSA-2048; 512 for RSA-4096.)
 * @return 0 on success; negative POSIX error code on error.
 */
int rsaw_decrypt_signature(uint8_t *buf, const uint8_t *modulus,
	uint32_t exponent, const uint8_t *sig, size_t size)
{
	uint32_t exponent_be;

	assert(buf != NULL);
	assert(modulus != NULL);
	assert(exponent != 0);
	assert(sig != NULL);
	assert(size == 256 || size == 512);

	if (!buf || !modulus || exponent == 0 || !sig || (size != 256 && size != 512)) {
		// Invalid parameters.
		return -EINVAL;
	}

	// bn_exp() requires big-endian data.
	exponent_be = cpu_to_be32(exponent);

	// Calculate the exponent.
	bn_exp(buf, sig, modulus, size, (const uint8_t*)&exponent_be, sizeof(exponent_be));
	return 0;
}
