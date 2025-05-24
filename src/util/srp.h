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

/*
 *
 * Purpose:       This is a direct implementation of the Secure Remote Password
 *                Protocol version 6a as described by
 *                http://srp.stanford.edu/design.html
 *
 * Author:        tom.cocagne@gmail.com (Tom Cocagne)
 *
 * Dependencies:  LibGMP
 *
 * Usage:         Refer to test_srp.c for a demonstration
 *
 * Notes:
 *    This library allows multiple combinations of hashing algorithms and
 *    prime number constants. For authentication to succeed, the hash and
 *    prime number constants must match between
 *    srp_create_salted_verification_key(), srp_user_new(),
 *    and srp_verifier_new(). A recommended approach is to determine the
 *    desired level of security for an application and globally define the
 *    hash and prime number constants to the predetermined values.
 *
 *    As one might suspect, more bits means more security. As one might also
 *    suspect, more bits also means more processing time. The test_srp.c
 *    program can be easily modified to profile various combinations of
 *    hash & prime number pairings.
 */

#pragma once

#include <cstddef>

struct SRPVerifier;
struct SRPUser;

typedef enum {
	SRP_NG_1024,
	SRP_NG_2048,
	SRP_NG_4096,
	SRP_NG_8192,
	SRP_NG_CUSTOM
} SRP_NGType;

typedef enum {
	/*SRP_SHA1,*/
	/*SRP_SHA224,*/
	SRP_SHA256,
	/*SRP_SHA384,
	SRP_SHA512*/
} SRP_HashAlgorithm;

typedef enum {
	SRP_ERR,
	SRP_OK,
} SRP_Result;


/* Out: bytes_v, len_v
 *
 * The caller is responsible for freeing the memory allocated for bytes_v
 *
 * The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type.
 * If provided, they must contain ASCII text of the hexidecimal notation.
 *
 * If bytes_s == NULL, it is filled with random data.
 * The caller is responsible for freeing.
 *
 * Returns SRP_OK on success, and SRP_ERR on error.
 * bytes_s might be in this case invalid, don't free it.
 */
SRP_Result srp_create_salted_verification_key(SRP_HashAlgorithm alg,
	SRP_NGType ng_type, const char *username_for_verifier,
	const unsigned char *password, size_t len_password,
	unsigned char **bytes_s,  size_t *len_s,
	unsigned char **bytes_v, size_t *len_v,
	const char *n_hex, const char *g_hex);

/* Out: bytes_B, len_B.
 *
 * On failure, bytes_B will be set to NULL and len_B will be set to 0
 *
 * The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type
 *
 * If bytes_b == NULL, random data is used for b.
 *
 * Returns pointer to SRPVerifier on success, and NULL on error.
 */
struct SRPVerifier* srp_verifier_new(SRP_HashAlgorithm alg, SRP_NGType ng_type,
	const char *username,
	const unsigned char *bytes_s, size_t len_s,
	const unsigned char *bytes_v, size_t len_v,
	const unsigned char *bytes_A, size_t len_A,
	const unsigned char *bytes_b, size_t len_b,
	unsigned char** bytes_B, size_t *len_B,
	const char* n_hex, const char* g_hex);

void srp_verifier_delete(struct SRPVerifier *ver);

// srp_verifier_verify_session must have been called before
int srp_verifier_is_authenticated(struct SRPVerifier *ver);

const char *srp_verifier_get_username(struct SRPVerifier *ver);

/* key_length may be null */
const unsigned char *srp_verifier_get_session_key(
	struct SRPVerifier *ver, size_t *key_length);

size_t srp_verifier_get_session_key_length(struct SRPVerifier *ver);

/* Verifies session, on success, it writes bytes_HAMK.
 * user_M must be exactly srp_verifier_get_session_key_length() bytes in size
 */
void srp_verifier_verify_session(
	struct SRPVerifier *ver, const unsigned char *user_M, unsigned char **bytes_HAMK);

/*******************************************************************************/

/* The n_hex and g_hex parameters should be 0 unless SRP_NG_CUSTOM is used for ng_type */
struct SRPUser *srp_user_new(SRP_HashAlgorithm alg, SRP_NGType ng_type,
	const char *username, const char *username_for_verifier,
	const unsigned char *bytes_password, size_t len_password, const char *n_hex,
	const char *g_hex);

void srp_user_delete(struct SRPUser *usr);

int srp_user_is_authenticated(struct SRPUser *usr);

const char *srp_user_get_username(struct SRPUser *usr);

/* key_length may be null */
const unsigned char *srp_user_get_session_key(struct SRPUser *usr, size_t *key_length);

size_t srp_user_get_session_key_length(struct SRPUser *usr);

/* Output: username, bytes_A, len_A.
 * If you don't want it get written, set username to NULL.
 * If bytes_a == NULL, random data is used for a. */
SRP_Result srp_user_start_authentication(struct SRPUser* usr, char **username,
	const unsigned char *bytes_a, size_t len_a,
	unsigned char **bytes_A, size_t* len_A);

/* Output: bytes_M, len_M  (len_M may be null and will always be
 *                          srp_user_get_session_key_length() bytes in size) */
void srp_user_process_challenge(struct SRPUser *usr,
	const unsigned char *bytes_s, size_t len_s,
	const unsigned char *bytes_B, size_t len_B,
	unsigned char **bytes_M, size_t *len_M);

/* bytes_HAMK must be exactly srp_user_get_session_key_length() bytes in size */
void srp_user_verify_session(struct SRPUser *usr, const unsigned char *bytes_HAMK);
