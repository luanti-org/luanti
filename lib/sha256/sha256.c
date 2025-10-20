/* libcrypto/sha/sha256.c */
/* ====================================================================
 * Copyright (c) 1998-2011 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "my_sha256.h"

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__attribute__)
#define __attribute__(a)
#endif

#include "cmake_config.h" /* HAVE_ENDIAN_H */

/** endian.h **/
/*
 * Public domain
 * endian.h compatibility shim
 */

#if defined(_WIN32)

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define PDP_ENDIAN 3412

/*
 * Use GCC and Visual Studio compiler defines to determine endian.
 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif

#elif defined(HAVE_ENDIAN_H)
#include <endian.h>

#elif defined(__MACH__) && defined(__APPLE__)
#include <machine/endian.h>

#elif defined(__sun) || defined(_AIX) || defined(__hpux)
#include <arpa/nameser_compat.h>
#include <sys/types.h>

#elif defined(__sgi)
#include <standards.h>
#include <sys/endian.h>

#endif

#ifndef __STRICT_ALIGNMENT
#define __STRICT_ALIGNMENT
#if defined(__i386) || defined(__i386__) || defined(__x86_64) ||               \
    defined(__x86_64__) || defined(__s390__) || defined(__s390x__) ||          \
    defined(__aarch64__) ||                                                    \
    ((defined(__arm__) || defined(__arm)) && __ARM_ARCH >= 6)
#undef __STRICT_ALIGNMENT
#endif
#endif

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define be16toh(x) OSSwapBigToHostInt16((x))
#define htobe16(x) OSSwapHostToBigInt16((x))
#define le32toh(x) OSSwapLittleToHostInt32((x))
#define be32toh(x) OSSwapBigToHostInt32((x))
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#endif /* __APPLE__ */

#if defined(_WIN32) && !defined(HAVE_ENDIAN_H)
#include <winsock2.h>

#define be16toh(x) ntohs((x))
#define htobe16(x) htons((x))
#define le32toh(x) (x)
#define be32toh(x) ntohl((x))
#define htole32(x) (x)
#define htobe32(x) ntohl((x))
#endif /* _WIN32 && !HAVE_ENDIAN_H */

#ifdef __linux__
#if !defined(betoh16)
#define betoh16(x) be16toh(x)
#endif
#if !defined(betoh32)
#define betoh32(x) be32toh(x)
#endif
#endif /* __linux__ */

#if defined(__FreeBSD__)
#if !defined(HAVE_ENDIAN_H)
#include <sys/endian.h>
#endif
#if !defined(betoh16)
#define betoh16(x) be16toh(x)
#endif
#if !defined(betoh32)
#define betoh32(x) be32toh(x)
#endif
#endif

#if defined(__NetBSD__)
#if !defined(betoh16)
#define betoh16(x) be16toh(x)
#endif
#if !defined(betoh32)
#define betoh32(x) be32toh(x)
#endif
#endif

#if defined(__sun)
#include <sys/byteorder.h>
#define be16toh(x) BE_16(x)
#define htobe16(x) BE_16(x)
#define le32toh(x) LE_32(x)
#define be32toh(x) BE_32(x)
#define htole32(x) LE_32(x)
#define htobe32(x) BE_32(x)
#endif
/** **/

/** libcrypto/crypto_internal.h **/
#define CTASSERT(x) \
    extern char _ctassert[(x) ? 1 : -1] __attribute__((__unused__))

static inline uint32_t
crypto_load_be32toh(const uint8_t *src)
{
	uint32_t v;

	memcpy(&v, src, sizeof(v));

	return be32toh(v);
}

static inline void
crypto_store_htobe32(uint8_t *dst, uint32_t v)
{
	v = htobe32(v);
	memcpy(dst, &v, sizeof(v));
}

static inline uint32_t
crypto_ror_u32(uint32_t v, size_t shift)
{
	return (v << (32 - shift)) | (v >> shift);
}
/** **/

/** libcrypto/hidden/crypto_namespace.h **/
# define LCRYPTO_UNUSED(x)
# define LCRYPTO_USED(x)
# define LCRYPTO_ALIAS1(pre,x)
# define LCRYPTO_ALIAS(x)
/** **/

/* Ensure that SHA_LONG and uint32_t are equivalent. */
CTASSERT(sizeof(SHA_LONG) == sizeof(uint32_t));

static void sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num);

static const SHA_LONG K256[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

static inline SHA_LONG
Sigma0(SHA_LONG x)
{
	return crypto_ror_u32(x, 2) ^ crypto_ror_u32(x, 13) ^
	    crypto_ror_u32(x, 22);
}

static inline SHA_LONG
Sigma1(SHA_LONG x)
{
	return crypto_ror_u32(x, 6) ^ crypto_ror_u32(x, 11) ^
	    crypto_ror_u32(x, 25);
}

static inline SHA_LONG
sigma0(SHA_LONG x)
{
	return crypto_ror_u32(x, 7) ^ crypto_ror_u32(x, 18) ^ (x >> 3);
}

static inline SHA_LONG
sigma1(SHA_LONG x)
{
	return crypto_ror_u32(x, 17) ^ crypto_ror_u32(x, 19) ^ (x >> 10);
}

static inline SHA_LONG
Ch(SHA_LONG x, SHA_LONG y, SHA_LONG z)
{
	return (x & y) ^ (~x & z);
}

static inline SHA_LONG
Maj(SHA_LONG x, SHA_LONG y, SHA_LONG z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline void
sha256_msg_schedule_update(SHA_LONG *W0, SHA_LONG W1,
    SHA_LONG W9, SHA_LONG W14)
{
	*W0 = sigma1(W14) + W9 + sigma0(W1) + *W0;
}

static inline void
sha256_round(SHA_LONG *a, SHA_LONG *b, SHA_LONG *c, SHA_LONG *d,
    SHA_LONG *e, SHA_LONG *f, SHA_LONG *g, SHA_LONG *h,
    SHA_LONG Kt, SHA_LONG Wt)
{
	SHA_LONG T1, T2;

	T1 = *h + Sigma1(*e) + Ch(*e, *f, *g) + Kt + Wt;
	T2 = Sigma0(*a) + Maj(*a, *b, *c);

	*h = *g;
	*g = *f;
	*f = *e;
	*e = *d + T1;
	*d = *c;
	*c = *b;
	*b = *a;
	*a = T1 + T2;
}

static void
sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num)
{
	const uint8_t *in = _in;
	const SHA_LONG *in32;
	SHA_LONG a, b, c, d, e, f, g, h;
	SHA_LONG X[16];
	int i;

	while (num--) {
		a = ctx->h[0];
		b = ctx->h[1];
		c = ctx->h[2];
		d = ctx->h[3];
		e = ctx->h[4];
		f = ctx->h[5];
		g = ctx->h[6];
		h = ctx->h[7];

		if ((size_t)in % 4 == 0) {
			/* Input is 32 bit aligned. */
			in32 = (const SHA_LONG *)in;
			X[0] = be32toh(in32[0]);
			X[1] = be32toh(in32[1]);
			X[2] = be32toh(in32[2]);
			X[3] = be32toh(in32[3]);
			X[4] = be32toh(in32[4]);
			X[5] = be32toh(in32[5]);
			X[6] = be32toh(in32[6]);
			X[7] = be32toh(in32[7]);
			X[8] = be32toh(in32[8]);
			X[9] = be32toh(in32[9]);
			X[10] = be32toh(in32[10]);
			X[11] = be32toh(in32[11]);
			X[12] = be32toh(in32[12]);
			X[13] = be32toh(in32[13]);
			X[14] = be32toh(in32[14]);
			X[15] = be32toh(in32[15]);
		} else {
			/* Input is not 32 bit aligned. */
			X[0] = crypto_load_be32toh(&in[0 * 4]);
			X[1] = crypto_load_be32toh(&in[1 * 4]);
			X[2] = crypto_load_be32toh(&in[2 * 4]);
			X[3] = crypto_load_be32toh(&in[3 * 4]);
			X[4] = crypto_load_be32toh(&in[4 * 4]);
			X[5] = crypto_load_be32toh(&in[5 * 4]);
			X[6] = crypto_load_be32toh(&in[6 * 4]);
			X[7] = crypto_load_be32toh(&in[7 * 4]);
			X[8] = crypto_load_be32toh(&in[8 * 4]);
			X[9] = crypto_load_be32toh(&in[9 * 4]);
			X[10] = crypto_load_be32toh(&in[10 * 4]);
			X[11] = crypto_load_be32toh(&in[11 * 4]);
			X[12] = crypto_load_be32toh(&in[12 * 4]);
			X[13] = crypto_load_be32toh(&in[13 * 4]);
			X[14] = crypto_load_be32toh(&in[14 * 4]);
			X[15] = crypto_load_be32toh(&in[15 * 4]);
		}
		in += SHA256_CBLOCK;

		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[0], X[0]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[1], X[1]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[2], X[2]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[3], X[3]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[4], X[4]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[5], X[5]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[6], X[6]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[7], X[7]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[8], X[8]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[9], X[9]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[10], X[10]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[11], X[11]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[12], X[12]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[13], X[13]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[14], X[14]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[15], X[15]);

		for (i = 16; i < 64; i += 16) {
			sha256_msg_schedule_update(&X[0], X[1], X[9], X[14]);
			sha256_msg_schedule_update(&X[1], X[2], X[10], X[15]);
			sha256_msg_schedule_update(&X[2], X[3], X[11], X[0]);
			sha256_msg_schedule_update(&X[3], X[4], X[12], X[1]);
			sha256_msg_schedule_update(&X[4], X[5], X[13], X[2]);
			sha256_msg_schedule_update(&X[5], X[6], X[14], X[3]);
			sha256_msg_schedule_update(&X[6], X[7], X[15], X[4]);
			sha256_msg_schedule_update(&X[7], X[8], X[0], X[5]);
			sha256_msg_schedule_update(&X[8], X[9], X[1], X[6]);
			sha256_msg_schedule_update(&X[9], X[10], X[2], X[7]);
			sha256_msg_schedule_update(&X[10], X[11], X[3], X[8]);
			sha256_msg_schedule_update(&X[11], X[12], X[4], X[9]);
			sha256_msg_schedule_update(&X[12], X[13], X[5], X[10]);
			sha256_msg_schedule_update(&X[13], X[14], X[6], X[11]);
			sha256_msg_schedule_update(&X[14], X[15], X[7], X[12]);
			sha256_msg_schedule_update(&X[15], X[0], X[8], X[13]);

			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 0], X[0]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 1], X[1]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 2], X[2]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 3], X[3]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 4], X[4]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 5], X[5]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 6], X[6]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 7], X[7]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 8], X[8]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 9], X[9]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 10], X[10]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 11], X[11]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 12], X[12]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 13], X[13]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 14], X[14]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 15], X[15]);
		}

		ctx->h[0] += a;
		ctx->h[1] += b;
		ctx->h[2] += c;
		ctx->h[3] += d;
		ctx->h[4] += e;
		ctx->h[5] += f;
		ctx->h[6] += g;
		ctx->h[7] += h;
	}
}

int
SHA256_Init(SHA256_CTX *c)
{
	memset(c, 0, sizeof(*c));

	c->h[0] = 0x6a09e667UL;
	c->h[1] = 0xbb67ae85UL;
	c->h[2] = 0x3c6ef372UL;
	c->h[3] = 0xa54ff53aUL;
	c->h[4] = 0x510e527fUL;
	c->h[5] = 0x9b05688cUL;
	c->h[6] = 0x1f83d9abUL;
	c->h[7] = 0x5be0cd19UL;

	c->md_len = SHA256_DIGEST_LENGTH;

	return 1;
}
LCRYPTO_ALIAS(SHA256_Init);

int
SHA256_Update(SHA256_CTX *c, const void *data_, size_t len)
{
	const unsigned char *data = data_;
	unsigned char *p;
	SHA_LONG l;
	size_t n;

	if (len == 0)
		return 1;

	l = (c->Nl + (((SHA_LONG)len) << 3)) & 0xffffffffUL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh += (SHA_LONG)(len >> 29);	/* might cause compiler warning on 16-bit */
	c->Nl = l;

	n = c->num;
	if (n != 0) {
		p = (unsigned char *)c->data;

		if (len >= SHA_CBLOCK || len + n >= SHA_CBLOCK) {
			memcpy(p + n, data, SHA_CBLOCK - n);
			sha256_block_data_order(c, p, 1);
			n = SHA_CBLOCK - n;
			data += n;
			len -= n;
			c->num = 0;
			memset(p, 0, SHA_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy(p + n, data, len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/SHA_CBLOCK;
	if (n > 0) {
		sha256_block_data_order(c, data, n);
		n *= SHA_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		p = (unsigned char *)c->data;
		c->num = (unsigned int)len;
		memcpy(p, data, len);
	}
	return 1;
}
LCRYPTO_ALIAS(SHA256_Update);

void
SHA256_Transform(SHA256_CTX *c, const unsigned char *data)
{
	sha256_block_data_order(c, data, 1);
}
LCRYPTO_ALIAS(SHA256_Transform);

int
SHA256_Final(unsigned char *md, SHA256_CTX *c)
{
	unsigned char *p = (unsigned char *)c->data;
	size_t n = c->num;
	unsigned int nn;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (SHA_CBLOCK - 8)) {
		memset(p + n, 0, SHA_CBLOCK - n);
		n = 0;
		sha256_block_data_order(c, p, 1);
	}

	memset(p + n, 0, SHA_CBLOCK - 8 - n);
	c->data[SHA_LBLOCK - 2] = htobe32(c->Nh);
	c->data[SHA_LBLOCK - 1] = htobe32(c->Nl);

	sha256_block_data_order(c, p, 1);
	c->num = 0;
	memset(p, 0, SHA_CBLOCK);

	/*
	 * Note that FIPS180-2 discusses "Truncation of the Hash Function Output."
	 * default: case below covers for it. It's not clear however if it's
	 * permitted to truncate to amount of bytes not divisible by 4. I bet not,
	 * but if it is, then default: case shall be extended. For reference.
	 * Idea behind separate cases for pre-defined lengths is to let the
	 * compiler decide if it's appropriate to unroll small loops.
	 */
	switch (c->md_len) {
	case SHA256_DIGEST_LENGTH:
		for (nn = 0; nn < SHA256_DIGEST_LENGTH / 4; nn++) {
			crypto_store_htobe32(md, c->h[nn]);
			md += 4;
		}
		break;

	default:
		if (c->md_len > SHA256_DIGEST_LENGTH)
			return 0;
		for (nn = 0; nn < c->md_len / 4; nn++) {
			crypto_store_htobe32(md, c->h[nn]);
			md += 4;
		}
		break;
	}

	return 1;
}
LCRYPTO_ALIAS(SHA256_Final);

unsigned char *
SHA256(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA256_CTX c;
	static unsigned char m[SHA256_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	SHA256_Init(&c);
	SHA256_Update(&c, d, n);
	SHA256_Final(md, &c);

	memset(&c, 0, sizeof(c));

	return (md);
}
LCRYPTO_ALIAS(SHA256);
