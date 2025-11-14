/*
 * Secure Remote Password 6a implementation
 * https://github.com/est31/csrp-gmp
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2010, 2013 Tom Cocagne, 2015 est31 <MTest31@outlook.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#include <cstddef>

#ifdef WIN32
	#include <windows.h>
	#include <wincrypt.h>
#else
	#include <ctime>

#endif

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <config.h>

#if USE_SYSTEM_GMP
	#include <gmp.h>
#else
	#include <mini-gmp.h>
#endif

#include "my_sha256.h"
#include "porting.h"

#include "srp.h"
//#define CSRP_USE_SHA1
#define CSRP_USE_SHA256

#define CSRP_MAX_HASH (SHA256_DIGEST_LENGTH)

#define srp_dbg_data(data, datalen, prevtext) ;
/*void srp_dbg_data(unsigned char * data, size_t datalen, char * prevtext)
{
	printf(prevtext);
	size_t i;
	for (i = 0; i < datalen; i++)
	{
		printf("%02X", data[i]);
	}
	printf("\n");
}*/

typedef struct {
	mpz_t N;
	mpz_t g;
} NGConstant;

struct NGHex {
	const char *n_hex;
	const char *g_hex;
};

/* All constants here were pulled from Appendix A of RFC 5054 */
static struct NGHex global_Ng_constants[] = {
	{/* 1024 */
		"EEAF0AB9ADB38DD69C33F80AFA8FC5E86072618775FF3C0B9EA2314C"
		"9C256576D674DF7496EA81D3383B4813D692C6E0E0D5D8E250B98BE4"
		"8E495C1D6089DAD15DC7D7B46154D6B6CE8EF4AD69B15D4982559B29"
		"7BCF1885C529F566660E57EC68EDBC3C05726CC02FD4CBF4976EAA9A"
		"FD5138FE8376435B9FC61D2FC0EB06E3",
		"2"},
	{/* 2048 */
		"AC6BDB41324A9A9BF166DE5E1389582FAF72B6651987EE07FC319294"
		"3DB56050A37329CBB4A099ED8193E0757767A13DD52312AB4B03310D"
		"CD7F48A9DA04FD50E8083969EDB767B0CF6095179A163AB3661A05FB"
		"D5FAAAE82918A9962F0B93B855F97993EC975EEAA80D740ADBF4FF74"
		"7359D041D5C33EA71D281E446B14773BCA97B43A23FB801676BD207A"
		"436C6481F1D2B9078717461A5B9D32E688F87748544523B524B0D57D"
		"5EA77A2775D2ECFA032CFBDBF52FB3786160279004E57AE6AF874E73"
		"03CE53299CCC041C7BC308D82A5698F3A8D0C38271AE35F8E9DBFBB6"
		"94B5C803D89F7AE435DE236D525F54759B65E372FCD68EF20FA7111F"
		"9E4AFF73",
		"2"},
	{/* 4096 */
		"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
		"8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
		"302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
		"A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
		"49286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8"
		"FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C"
		"180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
		"3995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D"
		"04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7D"
		"B3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D226"
		"1AD2EE6BF12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFC"
		"E0FD108E4B82D120A92108011A723C12A787E6D788719A10BDBA5B26"
		"99C327186AF4E23C1A946834B6150BDA2583E9CA2AD44CE8DBBBC2DB"
		"04DE8EF92E8EFC141FBECAA6287C59474E6BC05D99B2964FA090C3A2"
		"233BA186515BE7ED1F612970CEE2D7AFB81BDD762170481CD0069127"
		"D5B05AA993B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934063199"
		"FFFFFFFFFFFFFFFF",
		"5"},
	{/* 8192 */
		"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
		"8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
		"302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
		"A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
		"49286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8"
		"FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C"
		"180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
		"3995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D"
		"04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7D"
		"B3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D226"
		"1AD2EE6BF12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFC"
		"E0FD108E4B82D120A92108011A723C12A787E6D788719A10BDBA5B26"
		"99C327186AF4E23C1A946834B6150BDA2583E9CA2AD44CE8DBBBC2DB"
		"04DE8EF92E8EFC141FBECAA6287C59474E6BC05D99B2964FA090C3A2"
		"233BA186515BE7ED1F612970CEE2D7AFB81BDD762170481CD0069127"
		"D5B05AA993B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492"
		"36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BDF8FF9406"
		"AD9E530EE5DB382F413001AEB06A53ED9027D831179727B0865A8918"
		"DA3EDBEBCF9B14ED44CE6CBACED4BB1BDB7F1447E6CC254B33205151"
		"2BD7AF426FB8F401378CD2BF5983CA01C64B92ECF032EA15D1721D03"
		"F482D7CE6E74FEF6D55E702F46980C82B5A84031900B1C9E59E7C97F"
		"BEC7E8F323A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA"
		"CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE32806A1D58B"
		"B7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55CDA56C9EC2EF29632"
		"387FE8D76E3C0468043E8F663F4860EE12BF2D5B0B7474D6E694F91E"
		"6DBE115974A3926F12FEE5E438777CB6A932DF8CD8BEC4D073B931BA"
		"3BC832B68D9DD300741FA7BF8AFC47ED2576F6936BA424663AAB639C"
		"5AE4F5683423B4742BF1C978238F16CBE39D652DE3FDB8BEFC848AD9"
		"22222E04A4037C0713EB57A81A23F0C73473FC646CEA306B4BCBC886"
		"2F8385DDFA9D4B7FA2C087E879683303ED5BDD3A062B3CF5B3A278A6"
		"6D2A13F83F44F82DDF310EE074AB6A364597E899A0255DC164F31CC5"
		"0846851DF9AB48195DED7EA1B1D510BD7EE74D73FAF36BC31ECFA268"
		"359046F4EB879F924009438B481C6CD7889A002ED5EE382BC9190DA6"
		"FC026E479558E4475677E9AA9E3050E2765694DFC81F56E880B96E71"
		"60C980DD98EDD3DFFFFFFFFFFFFFFFFF",
		"13"},
	{0, 0} /* null sentinel */
};

static void delete_ng(NGConstant *ng)
{
	if (ng) {
		mpz_clear(ng->N);
		mpz_clear(ng->g);
		free(ng);
	}
}

static NGConstant *new_ng(SRP_NGType ng_type, const char *n_hex, const char *g_hex)
{
	NGConstant *ng = (NGConstant *)malloc(sizeof(NGConstant));

	if (!ng) return 0;

	mpz_init(ng->N);
	mpz_init(ng->g);

	if (ng_type != SRP_NG_CUSTOM) {
		n_hex = global_Ng_constants[ng_type].n_hex;
		g_hex = global_Ng_constants[ng_type].g_hex;
	}

	int rv = 0;
	rv = mpz_set_str(ng->N, n_hex, 16);
	rv = rv | mpz_set_str(ng->g, g_hex, 16);

	if (rv) {
		delete_ng(ng);
		return 0;
	}

	return ng;
}

typedef union {
	// SHA_CTX sha;
	SHA256_CTX sha256;
	// SHA512_CTX sha512;
} HashCTX;

struct SRPVerifier {
	SRP_HashAlgorithm hash_alg;
	NGConstant *ng;

	char *username;
	unsigned char *bytes_B;
	int authenticated;

	unsigned char M[CSRP_MAX_HASH];
	unsigned char H_AMK[CSRP_MAX_HASH];
	unsigned char session_key[CSRP_MAX_HASH];
};

struct SRPUser {
	SRP_HashAlgorithm hash_alg;
	NGConstant *ng;

	mpz_t a;
	mpz_t A;
	mpz_t S;

	unsigned char *bytes_A;
	int authenticated;

	char *username;
	char *username_verifier;
	unsigned char *password;
	size_t password_len;

	unsigned char M[CSRP_MAX_HASH];
	unsigned char H_AMK[CSRP_MAX_HASH];
	unsigned char session_key[CSRP_MAX_HASH];
};

static int hash_init(SRP_HashAlgorithm alg, HashCTX *c)
{
	switch (alg) {
#ifdef CSRP_USE_SHA1
		case SRP_SHA1: return SHA1_Init(&c->sha);
#endif
		/*
		case SRP_SHA224: return SHA224_Init(&c->sha256);
		*/
#ifdef CSRP_USE_SHA256
		case SRP_SHA256: return SHA256_Init(&c->sha256);
#endif
		/*
		case SRP_SHA384: return SHA384_Init(&c->sha512);
		case SRP_SHA512: return SHA512_Init(&c->sha512);
		*/
		default: return -1;
	};
}
static int hash_update( SRP_HashAlgorithm alg, HashCTX *c, const void *data, size_t len )
{
	switch (alg) {
#ifdef CSRP_USE_SHA1
		case SRP_SHA1: return SHA1_Update(&c->sha, data, len);
#endif
		/*
		case SRP_SHA224: return SHA224_Update(&c->sha256, data, len);
		*/
#ifdef CSRP_USE_SHA256
		case SRP_SHA256: return SHA256_Update(&c->sha256, data, len);
#endif
		/*
		case SRP_SHA384: return SHA384_Update(&c->sha512, data, len);
		case SRP_SHA512: return SHA512_Update(&c->sha512, data, len);
		*/
		default: return -1;
	};
}
static int hash_final( SRP_HashAlgorithm alg, HashCTX *c, unsigned char *md )
{
	switch (alg) {
#ifdef CSRP_USE_SHA1
		case SRP_SHA1: return SHA1_Final(md, &c->sha);
#endif
		/*
		case SRP_SHA224: return SHA224_Final(md, &c->sha256);
		*/
#ifdef CSRP_USE_SHA256
		case SRP_SHA256: return SHA256_Final(md, &c->sha256);
#endif
		/*
		case SRP_SHA384: return SHA384_Final(md, &c->sha512);
		case SRP_SHA512: return SHA512_Final(md, &c->sha512);
		*/
		default: return -1;
	};
}
static unsigned char *hash(SRP_HashAlgorithm alg, const unsigned char *d, size_t n, unsigned char *md)
{
	switch (alg) {
#ifdef CSRP_USE_SHA1
		case SRP_SHA1: return SHA1(d, n, md);
#endif
		/*
		case SRP_SHA224: return SHA224( d, n, md );
		*/
#ifdef CSRP_USE_SHA256
		case SRP_SHA256: return SHA256(d, n, md);
#endif
		/*
		case SRP_SHA384: return SHA384( d, n, md );
		case SRP_SHA512: return SHA512( d, n, md );
		*/
		default: return 0;
	};
}
static size_t hash_length(SRP_HashAlgorithm alg)
{
	switch (alg) {
#ifdef CSRP_USE_SHA1
		case SRP_SHA1: return SHA_DIGEST_LENGTH;
#endif
		/*
		case SRP_SHA224: return SHA224_DIGEST_LENGTH;
		*/
#ifdef CSRP_USE_SHA256
		case SRP_SHA256: return SHA256_DIGEST_LENGTH;
#endif
		/*
		case SRP_SHA384: return SHA384_DIGEST_LENGTH;
		case SRP_SHA512: return SHA512_DIGEST_LENGTH;
		*/
		default: return 0;
	};
}

inline static int mpz_num_bytes(const mpz_t op)
{
	return (mpz_sizeinbase(op, 2) + 7) / 8;
}

inline static void mpz_to_bin(const mpz_t op, unsigned char *to)
{
	mpz_export(to, NULL, 1, 1, 1, 0, op);
}

inline static void mpz_from_bin(const unsigned char *s, size_t len, mpz_t ret)
{
	mpz_import(ret, len, 1, 1, 1, 0, s);
}

// set op to (op1 * op2) mod d, using tmp for the calculation
inline static void mpz_mulm(
	mpz_t op, const mpz_t op1, const mpz_t op2, const mpz_t d, mpz_t tmp)
{
	mpz_mul(tmp, op1, op2);
	mpz_mod(op, tmp, d);
}

// set op to (op1 + op2) mod d, using tmp for the calculation
inline static void mpz_addm(
	mpz_t op, const mpz_t op1, const mpz_t op2, const mpz_t d, mpz_t tmp)
{
	mpz_add(tmp, op1, op2);
	mpz_mod(op, tmp, d);
}

// set op to (op1 - op2) mod d, using tmp for the calculation
inline static void mpz_subm(
	mpz_t op, const mpz_t op1, const mpz_t op2, const mpz_t d, mpz_t tmp)
{
	mpz_sub(tmp, op1, op2);
	mpz_mod(op, tmp, d);
}

static SRP_Result H_nn(
	mpz_t result, SRP_HashAlgorithm alg, const mpz_t N, const mpz_t n1, const mpz_t n2)
{
	unsigned char buff[CSRP_MAX_HASH];
	size_t len_N = mpz_num_bytes(N);
	size_t len_n1 = mpz_num_bytes(n1);
	size_t len_n2 = mpz_num_bytes(n2);
	size_t nbytes = len_N + len_N;
	unsigned char *bin = (unsigned char *)malloc(nbytes);
	if (!bin) return SRP_ERR;
	if (len_n1 > len_N || len_n2 > len_N) {
		free(bin);
		return SRP_ERR;
	}
	memset(bin, 0, nbytes);
	mpz_to_bin(n1, bin + (len_N - len_n1));
	mpz_to_bin(n2, bin + (len_N + len_N - len_n2));
	hash(alg, bin, nbytes, buff);
	free(bin);
	mpz_from_bin(buff, hash_length(alg), result);
	return SRP_OK;
}

static SRP_Result H_ns(mpz_t result, SRP_HashAlgorithm alg, const unsigned char *n,
	size_t len_n, const unsigned char *bytes, size_t len_bytes)
{
	unsigned char buff[CSRP_MAX_HASH];
	size_t nbytes = len_n + len_bytes;
	unsigned char *bin = (unsigned char *)malloc(nbytes);
	if (!bin) return SRP_ERR;
	memcpy(bin, n, len_n);
	memcpy(bin + len_n, bytes, len_bytes);
	hash(alg, bin, nbytes, buff);
	free(bin);
	mpz_from_bin(buff, hash_length(alg), result);
	return SRP_OK;
}

static int calculate_x(mpz_t result, SRP_HashAlgorithm alg, const unsigned char *salt,
	size_t salt_len, const char *username, const unsigned char *password,
	size_t password_len)
{
	unsigned char ucp_hash[CSRP_MAX_HASH];
	HashCTX ctx;
	hash_init(alg, &ctx);

	srp_dbg_data((char *)username, strlen(username), "Username for x: ");
	srp_dbg_data((char *)password, password_len, "Password for x: ");
	hash_update(alg, &ctx, username, strlen(username));
	hash_update(alg, &ctx, ":", 1);
	hash_update(alg, &ctx, password, password_len);

	hash_final(alg, &ctx, ucp_hash);

	return H_ns(result, alg, salt, salt_len, ucp_hash, hash_length(alg));
}

static SRP_Result update_hash_n(SRP_HashAlgorithm alg, HashCTX *ctx, const mpz_t n)
{
	size_t len = mpz_num_bytes(n);
	unsigned char *n_bytes = (unsigned char *)malloc(len);
	if (!n_bytes) return SRP_ERR;
	mpz_to_bin(n, n_bytes);
	hash_update(alg, ctx, n_bytes, len);
	free(n_bytes);
	return SRP_OK;
}

static SRP_Result hash_num(SRP_HashAlgorithm alg, const mpz_t n, unsigned char *dest)
{
	int nbytes = mpz_num_bytes(n);
	unsigned char *bin = (unsigned char *)malloc(nbytes);
	if (!bin) return SRP_ERR;
	mpz_to_bin(n, bin);
	hash(alg, bin, nbytes, dest);
	free(bin);
	return SRP_OK;
}

static SRP_Result calculate_M(SRP_HashAlgorithm alg, NGConstant *ng, unsigned char *dest,
	const char *I, const unsigned char *s_bytes, size_t s_len, const mpz_t A,
	const mpz_t B, const unsigned char *K)
{
	unsigned char H_N[CSRP_MAX_HASH];
	unsigned char H_g[CSRP_MAX_HASH];
	unsigned char H_I[CSRP_MAX_HASH];
	unsigned char H_xor[CSRP_MAX_HASH];
	HashCTX ctx;
	size_t i = 0;
	size_t hash_len = hash_length(alg);

	if (!hash_num(alg, ng->N, H_N)) return SRP_ERR;
	if (!hash_num(alg, ng->g, H_g)) return SRP_ERR;

	hash(alg, (const unsigned char *)I, strlen(I), H_I);

	for (i = 0; i < hash_len; i++)
		H_xor[i] = H_N[i] ^ H_g[i];

	hash_init(alg, &ctx);

	hash_update(alg, &ctx, H_xor, hash_len);
	hash_update(alg, &ctx, H_I, hash_len);
	hash_update(alg, &ctx, s_bytes, s_len);
	if (!update_hash_n(alg, &ctx, A)) return SRP_ERR;
	if (!update_hash_n(alg, &ctx, B)) return SRP_ERR;
	hash_update(alg, &ctx, K, hash_len);

	hash_final(alg, &ctx, dest);
	return SRP_OK;
}

static SRP_Result calculate_H_AMK(SRP_HashAlgorithm alg, unsigned char *dest,
	const mpz_t A, const unsigned char *M, const unsigned char *K)
{
	HashCTX ctx;

	hash_init(alg, &ctx);

	if (!update_hash_n(alg, &ctx, A)) return SRP_ERR;
	hash_update(alg, &ctx, M, hash_length(alg));
	hash_update(alg, &ctx, K, hash_length(alg));

	hash_final(alg, &ctx, dest);
	return SRP_OK;
}

static SRP_Result mpz_fill_random(mpz_t num)
{
	unsigned char random_buf[32];
	if (!porting::secure_rand_fill_buf(random_buf, sizeof(random_buf)))
		return SRP_ERR;
	mpz_from_bin(random_buf, sizeof(random_buf), num);
	return SRP_OK;
}

#define srp_dbg_num(num, text) ;
/*void srp_dbg_num(mpz_t num, char * prevtext)
{
	int len_num = mpz_num_bytes(num);
	char *bytes_num = (char*) malloc(len_num);
	mpz_to_bin(num, (unsigned char *) bytes_num);
	srp_dbg_data(bytes_num, len_num, prevtext);
	free(bytes_num);

}*/

/***********************************************************************************************************
 *
 *  Exported Functions
 *
 ***********************************************************************************************************/

SRP_Result srp_create_salted_verification_key( SRP_HashAlgorithm alg,
	SRP_NGType ng_type, const char *username_for_verifier,
	const unsigned char *password, size_t len_password,
	unsigned char **bytes_s,  size_t *len_s,
	unsigned char **bytes_v, size_t *len_v,
	const char *n_hex, const char *g_hex )
{
	SRP_Result ret = SRP_OK;

	mpz_t v; mpz_init(v);
	mpz_t x; mpz_init(x);

	NGConstant *ng = new_ng(ng_type, n_hex, g_hex);

	if (!ng) goto error_and_exit;

	if (*bytes_s == NULL) {
		size_t size_to_fill = 16;
		*len_s = size_to_fill;
		*bytes_s = (unsigned char *)malloc(size_to_fill);
		if (!*bytes_s) goto error_and_exit;
		if (!porting::secure_rand_fill_buf(*bytes_s, size_to_fill))
			goto error_and_exit;
	}

	if (!calculate_x(
			x, alg, *bytes_s, *len_s, username_for_verifier, password, len_password))
		goto error_and_exit;

	srp_dbg_num(x, "Server calculated x: ");

	mpz_powm(v, ng->g, x, ng->N);

	*len_v = mpz_num_bytes(v);

	*bytes_v = (unsigned char *)malloc(*len_v);

	if (!*bytes_v) goto error_and_exit;

	mpz_to_bin(v, *bytes_v);

cleanup_and_exit:
	delete_ng(ng);
	mpz_clear(v);
	mpz_clear(x);
	return ret;
error_and_exit:
	ret = SRP_ERR;
	goto cleanup_and_exit;
}


/* Out: bytes_B, len_B.
 *
 * On failure, bytes_B will be set to NULL and len_B will be set to 0
 */
struct SRPVerifier *srp_verifier_new(SRP_HashAlgorithm alg,
	SRP_NGType ng_type, const char *username,
	const unsigned char *bytes_s, size_t len_s,
	const unsigned char *bytes_v, size_t len_v,
	const unsigned char *bytes_A, size_t len_A,
	const unsigned char *bytes_b, size_t len_b,
	unsigned char **bytes_B, size_t *len_B,
	const char *n_hex, const char *g_hex )
{
	mpz_t v; mpz_init(v); mpz_from_bin(bytes_v, len_v, v);
	mpz_t A; mpz_init(A); mpz_from_bin(bytes_A, len_A, A);
	mpz_t u; mpz_init(u);
	mpz_t B; mpz_init(B);
	mpz_t S; mpz_init(S);
	mpz_t b; mpz_init(b);
	mpz_t k; mpz_init(k);
	mpz_t tmp1; mpz_init(tmp1);
	mpz_t tmp2; mpz_init(tmp2);
	mpz_t tmp3; mpz_init(tmp3);
	size_t ulen = strlen(username) + 1;
	NGConstant *ng = new_ng(ng_type, n_hex, g_hex);
	struct SRPVerifier *ver = 0;

	*len_B = 0;
	*bytes_B = 0;

	if (!ng) goto cleanup_and_exit;

	ver = (struct SRPVerifier *)malloc(sizeof(struct SRPVerifier));

	if (!ver) goto cleanup_and_exit;

	ver->username = (char *)malloc(ulen);
	ver->hash_alg = alg;
	ver->ng = ng;

	if (!ver->username) {
		free(ver);
		ver = 0;
		goto cleanup_and_exit;
	}

	memcpy(ver->username, username, ulen);

	ver->authenticated = 0;

	/* SRP-6a safety check */
	mpz_mod(tmp1, A, ng->N);
	if (mpz_sgn(tmp1) != 0) {
		if (bytes_b) {
			mpz_from_bin(bytes_b, len_b, b);
		} else {
			if (!mpz_fill_random(b)) goto ver_cleanup_and_exit;
		}

		if (!H_nn(k, alg, ng->N, ng->N, ng->g)) goto ver_cleanup_and_exit;

		/* B = kv + g^b */
		mpz_mulm(tmp1, k, v, ng->N, tmp3);
		mpz_powm(tmp2, ng->g, b, ng->N);
		mpz_addm(B, tmp1, tmp2, ng->N, tmp3);

		if (!H_nn(u, alg, ng->N, A, B)) goto ver_cleanup_and_exit;

		srp_dbg_num(u, "Server calculated u: ");

		/* S = (A *(v^u)) ^ b */
		mpz_powm(tmp1, v, u, ng->N);
		mpz_mulm(tmp2, A, tmp1, ng->N, tmp3);
		mpz_powm(S, tmp2, b, ng->N);

		if (!hash_num(alg, S, ver->session_key)) goto ver_cleanup_and_exit;

		if (!calculate_M(
				alg, ng, ver->M, username, bytes_s, len_s, A, B, ver->session_key)) {
			goto ver_cleanup_and_exit;
		}
		if (!calculate_H_AMK(alg, ver->H_AMK, A, ver->M, ver->session_key)) {
			goto ver_cleanup_and_exit;
		}

		*len_B = mpz_num_bytes(B);
		*bytes_B = (unsigned char *)malloc(*len_B);

		if (!*bytes_B) {
			*len_B = 0;
			goto ver_cleanup_and_exit;
		}

		mpz_to_bin(B, *bytes_B);

		ver->bytes_B = *bytes_B;
	} else {
		free(ver);
		ver = 0;
	}

cleanup_and_exit:
	mpz_clear(v);
	mpz_clear(A);
	mpz_clear(u);
	mpz_clear(k);
	mpz_clear(B);
	mpz_clear(S);
	mpz_clear(b);
	mpz_clear(tmp1);
	mpz_clear(tmp2);
	mpz_clear(tmp3);
	return ver;
ver_cleanup_and_exit:
	free(ver->username);
	free(ver);
	ver = 0;
	goto cleanup_and_exit;
}

void srp_verifier_delete(struct SRPVerifier *ver)
{
	if (ver) {
		delete_ng(ver->ng);
		free(ver->username);
		free(ver->bytes_B);
		memset(ver, 0, sizeof(*ver));
		free(ver);
	}
}

int srp_verifier_is_authenticated(struct SRPVerifier *ver)
{
	return ver->authenticated;
}

const char *srp_verifier_get_username(struct SRPVerifier *ver)
{
	return ver->username;
}

const unsigned char *srp_verifier_get_session_key(
	struct SRPVerifier *ver, size_t *key_length)
{
	if (key_length) *key_length = hash_length(ver->hash_alg);
	return ver->session_key;
}

size_t srp_verifier_get_session_key_length(struct SRPVerifier *ver)
{
	return hash_length(ver->hash_alg);
}

/* user_M must be exactly CSRP_MAX_HASH bytes in size */
void srp_verifier_verify_session(
	struct SRPVerifier *ver, const unsigned char *user_M, unsigned char **bytes_HAMK)
{
	if (memcmp(ver->M, user_M, hash_length(ver->hash_alg)) == 0) {
		ver->authenticated = 1;
		*bytes_HAMK = ver->H_AMK;
	} else
		*bytes_HAMK = NULL;
}

/*******************************************************************************/

struct SRPUser *srp_user_new(SRP_HashAlgorithm alg, SRP_NGType ng_type,
	const char *username, const char *username_for_verifier,
	const unsigned char *bytes_password, size_t len_password, const char *n_hex,
	const char *g_hex)
{
	struct SRPUser *usr = (struct SRPUser *)malloc(sizeof(struct SRPUser));
	size_t ulen = strlen(username) + 1;
	size_t uvlen = strlen(username_for_verifier) + 1;

	if (!usr) goto err_exit;

	usr->hash_alg = alg;
	usr->ng = new_ng(ng_type, n_hex, g_hex);

	mpz_init(usr->a);
	mpz_init(usr->A);
	mpz_init(usr->S);

	if (!usr->ng) goto err_exit;

	usr->username = (char *)malloc(ulen);
	usr->username_verifier = (char *)malloc(uvlen);
	usr->password = (unsigned char *)malloc(len_password);
	usr->password_len = len_password;

	if (!usr->username || !usr->password || !usr->username_verifier) goto err_exit;

	memcpy(usr->username, username, ulen);
	memcpy(usr->username_verifier, username_for_verifier, uvlen);
	memcpy(usr->password, bytes_password, len_password);

	usr->authenticated = 0;

	usr->bytes_A = 0;

	return usr;

err_exit:
	if (usr) {
		mpz_clear(usr->a);
		mpz_clear(usr->A);
		mpz_clear(usr->S);
		delete_ng(usr->ng);
		free(usr->username);
		free(usr->username_verifier);
		if (usr->password) {
			memset(usr->password, 0, usr->password_len);
			free(usr->password);
		}
		free(usr);
	}

	return 0;
}

void srp_user_delete(struct SRPUser *usr)
{
	if (usr) {
		mpz_clear(usr->a);
		mpz_clear(usr->A);
		mpz_clear(usr->S);

		delete_ng(usr->ng);

		memset(usr->password, 0, usr->password_len);

		free(usr->username);
		free(usr->username_verifier);
		free(usr->password);

		if (usr->bytes_A) free(usr->bytes_A);

		memset(usr, 0, sizeof(*usr));
		free(usr);
	}
}

int srp_user_is_authenticated(struct SRPUser *usr)
{
	return usr->authenticated;
}

const char *srp_user_get_username(struct SRPUser *usr)
{
	return usr->username;
}

const unsigned char *srp_user_get_session_key(struct SRPUser *usr, size_t *key_length)
{
	if (key_length) *key_length = hash_length(usr->hash_alg);
	return usr->session_key;
}

size_t srp_user_get_session_key_length(struct SRPUser *usr)
{
	return hash_length(usr->hash_alg);
}

/* Output: username, bytes_A, len_A */
SRP_Result srp_user_start_authentication(struct SRPUser *usr, char **username,
	const unsigned char *bytes_a, size_t len_a,
	unsigned char **bytes_A, size_t *len_A)
{
	if (bytes_a) {
		mpz_from_bin(bytes_a, len_a, usr->a);
	} else {
		if (!mpz_fill_random(usr->a)) goto error_and_exit;
	}

	mpz_powm(usr->A, usr->ng->g, usr->a, usr->ng->N);

	*len_A = mpz_num_bytes(usr->A);
	*bytes_A = (unsigned char *)malloc(*len_A);

	if (!*bytes_A) goto error_and_exit;

	mpz_to_bin(usr->A, *bytes_A);

	usr->bytes_A = *bytes_A;
	if (username) *username = usr->username;

	return SRP_OK;

error_and_exit:
	*len_A = 0;
	*bytes_A = 0;
	*username = 0;
	return SRP_ERR;
}

/* Output: bytes_M. Buffer length is CSRP_MAX_HASH */
void  srp_user_process_challenge(struct SRPUser *usr,
	const unsigned char *bytes_s, size_t len_s,
	const unsigned char *bytes_B, size_t len_B,
	unsigned char **bytes_M, size_t *len_M)
{
	mpz_t B; mpz_init(B); mpz_from_bin(bytes_B, len_B, B);
	mpz_t u; mpz_init(u);
	mpz_t x; mpz_init(x);
	mpz_t k; mpz_init(k);
	mpz_t v; mpz_init(v);
	mpz_t tmp1; mpz_init(tmp1);
	mpz_t tmp2; mpz_init(tmp2);
	mpz_t tmp3; mpz_init(tmp3);
	mpz_t tmp4; mpz_init(tmp4);

	*len_M = 0;
	*bytes_M = 0;

	if (!H_nn(u, usr->hash_alg, usr->ng->N, usr->A, B)) goto cleanup_and_exit;

	srp_dbg_num(u, "Client calculated u: ");

	if (!calculate_x(x, usr->hash_alg, bytes_s, len_s, usr->username_verifier,
			usr->password, usr->password_len))
		goto cleanup_and_exit;

	srp_dbg_num(x, "Client calculated x: ");

	if (!H_nn(k, usr->hash_alg, usr->ng->N, usr->ng->N, usr->ng->g))
		goto cleanup_and_exit;

	/* SRP-6a safety check */
	if (mpz_sgn(B) != 0 && mpz_sgn(u) != 0) {
		mpz_powm(v, usr->ng->g, x, usr->ng->N);

		srp_dbg_num(v, "Client calculated v: ");

		/* S = (B - k*(g^x)) ^ (a + ux) */
		mpz_mul(tmp1, u, x);
		mpz_add(tmp2, usr->a, tmp1);               /* tmp2 = (a + ux)      */
		mpz_powm(tmp1, usr->ng->g, x, usr->ng->N); /* tmp1 = g^x           */
		mpz_mulm(tmp3, k, tmp1, usr->ng->N, tmp4); /* tmp3 = k*(g^x)       */
		mpz_subm(tmp1, B, tmp3, usr->ng->N, tmp4); /* tmp1 = (B - K*(g^x)) */
		mpz_powm(usr->S, tmp1, tmp2, usr->ng->N);

		if (!hash_num(usr->hash_alg, usr->S, usr->session_key)) goto cleanup_and_exit;

		if (!calculate_M(usr->hash_alg, usr->ng, usr->M, usr->username, bytes_s, len_s,
				usr->A, B, usr->session_key))
			goto cleanup_and_exit;
		if (!calculate_H_AMK(usr->hash_alg, usr->H_AMK, usr->A, usr->M, usr->session_key))
			goto cleanup_and_exit;

		*bytes_M = usr->M;
		*len_M = hash_length(usr->hash_alg);
	} else {
		*bytes_M = NULL;
		*len_M = 0;
	}

cleanup_and_exit:
	mpz_clear(B);
	mpz_clear(u);
	mpz_clear(x);
	mpz_clear(k);
	mpz_clear(v);
	mpz_clear(tmp1);
	mpz_clear(tmp2);
	mpz_clear(tmp3);
	mpz_clear(tmp4);
}

void srp_user_verify_session(struct SRPUser *usr, const unsigned char *bytes_HAMK)
{
	if (memcmp(usr->H_AMK, bytes_HAMK, hash_length(usr->hash_alg)) == 0)
		usr->authenticated = 1;
}
