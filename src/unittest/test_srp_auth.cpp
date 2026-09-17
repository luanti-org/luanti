// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "catch.h"

#include <cstring>
#include <string>
#include <vector>

#include "database/database-dummy.h"
#include "util/auth.h"
#include "util/srp.h"
#include "util/string.h"
#include "filesys.h"

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SRP auth: verifier encode/decode roundtrip")
{
	// Use the same low-level helper as the network code so we know
	// the wire format we are validating (auth.cpp:gen_srp_v).
	std::string verifier, salt;
	generate_srp_verifier_and_salt("Alice", "CorrectHorseBatteryStaple", &verifier, &salt);

	// Sanity: the raw verifier and salt must be non-empty
	REQUIRE(!verifier.empty());
	REQUIRE(!salt.empty());

	// encode_srp_verifier / decode_srp_verifier_and_salt must roundtrip
	std::string encoded = encode_srp_verifier(verifier, salt);
	REQUIRE(!encoded.empty());

	std::string decoded_verifier, decoded_salt;
	REQUIRE(decode_srp_verifier_and_salt(encoded, &decoded_verifier, &decoded_salt));
	REQUIRE(decoded_verifier == verifier);
	REQUIRE(decoded_salt == salt);

	// A malformed blob should be rejected
	REQUIRE(!decode_srp_verifier_and_salt("not a valid blob", &decoded_verifier, &decoded_salt));
	// Wrong version tag should be rejected
	std::string bad_version = "#2#" + std::string("a") + "#" + std::string("a");
	REQUIRE(!decode_srp_verifier_and_salt(bad_version, &decoded_verifier, &decoded_salt));
}

TEST_CASE("SRP auth: database stores and recalls encoded verifier")
{
	AuthDatabaseDummy auth_db;
	const std::string name = "Bob";
	const std::string encoded = get_encoded_srp_verifier(name, "s3cret");

	AuthEntry entry;
	entry.name = name;
	entry.password = encoded;
	entry.last_login = 1234;
	REQUIRE(auth_db.createAuth(entry));

	AuthEntry loaded;
	REQUIRE(auth_db.getAuth(name, loaded));
	REQUIRE(loaded.name == name);
	REQUIRE(loaded.password == encoded);

	// Update the password (simulating a sudo password change)
	loaded.password = get_encoded_srp_verifier(name, "new-pass");
	loaded.last_login = 5678;
	REQUIRE(auth_db.saveAuth(loaded));

	AuthEntry reloaded;
	REQUIRE(auth_db.getAuth(name, reloaded));
	REQUIRE(reloaded.password == loaded.password);
	REQUIRE(reloaded.last_login == 5678u);

	REQUIRE(auth_db.deleteAuth(name));
	REQUIRE(!auth_db.getAuth(name, reloaded));
}

TEST_CASE("SRP auth: first SRP registration stores a usable verifier")
{
	AuthDatabaseDummy auth_db;
	// Simulates Client::startAuth(AUTH_MECHANISM_FIRST_SRP) on the
	// client side: it generates salt+verifier from the password
	// using the same parameters as the server and sends them to
	// the server. The server then calls createAuth(enc_pwd).
	const std::string name = "Carol";
	const std::string password = "carolpass";

	std::string verifier, salt;
	generate_srp_verifier_and_salt(name, password, &verifier, &salt);
	std::string enc_pwd = encode_srp_verifier(verifier, salt);

	AuthEntry entry;
	entry.name = name;
	entry.password = enc_pwd;
	entry.last_login = 1;
	REQUIRE(auth_db.createAuth(entry));

	// Server-side reload must produce a verifier that the same
	// client can use to log in with the same password.
	AuthEntry loaded;
	REQUIRE(auth_db.getAuth(name, loaded));

	std::string db_verifier, db_salt;
	REQUIRE(decode_srp_verifier_and_salt(loaded.password, &db_verifier, &db_salt));
	REQUIRE(db_verifier == verifier);
	REQUIRE(db_salt == salt);
}

TEST_CASE("SRP auth: login handshake succeeds with the right password")
{
	AuthDatabaseDummy auth_db;
	// We reuse the verifyer's data the server would have stored
	// to drive a real SRP handshake end to end.
	const std::string name = "Dave";
	const std::string password = "davepass";

	std::string verifier, salt;
	generate_srp_verifier_and_salt(name, password, &verifier, &salt);
	std::string enc_pwd = encode_srp_verifier(verifier, salt);
	AuthEntry new_entry;
	new_entry.name = name;
	new_entry.password = enc_pwd;
	REQUIRE(auth_db.createAuth(new_entry));

	// --- Client side ---
	std::string client_name = name;
	SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		client_name.c_str(), lowercase(client_name).c_str(),
		(const unsigned char *) password.c_str(), password.size(),
		nullptr, nullptr);
	REQUIRE(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	SRP_Result r = srp_user_start_authentication(usr, nullptr, nullptr, 0,
		&bytes_A, &len_A);
	REQUIRE((int)r == (int)SRP_OK);
	REQUIRE((bytes_A != nullptr && len_A > 0));

	// --- Server side ---
	AuthEntry entry;
	REQUIRE(auth_db.getAuth(name, entry));
	std::string db_verifier, db_salt;
	REQUIRE(decode_srp_verifier_and_salt(entry.password, &db_verifier, &db_salt));

	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		client_name.c_str(),
		(const unsigned char *) db_salt.c_str(), db_salt.size(),
		(const unsigned char *) db_verifier.c_str(), db_verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	REQUIRE(ver);
	REQUIRE((bytes_B != nullptr && len_B > 0));

	// --- Client side: process challenge ---
	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) db_salt.c_str(), db_salt.size(),
		bytes_B, len_B,
		&bytes_M, &len_M);
	REQUIRE((bytes_M != nullptr && len_M > 0));

	// --- Server side: verify M, produce HAMK ---
	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	REQUIRE(bytes_HAMK != nullptr);
	REQUIRE(srp_verifier_is_authenticated(ver));

	// --- Client side: verify HAMK matches ---
	srp_user_verify_session(usr, bytes_HAMK);
	REQUIRE(srp_user_is_authenticated(usr));

	// Session keys must match between client and server.
	size_t server_key_len = 0, client_key_len = 0;
	const unsigned char *server_key = srp_verifier_get_session_key(ver, &server_key_len);
	const unsigned char *client_key = srp_user_get_session_key(usr, &client_key_len);
	REQUIRE(client_key_len == server_key_len);
	REQUIRE(server_key_len > 0);
	REQUIRE(std::memcmp(server_key, client_key, server_key_len) == 0);

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}

TEST_CASE("SRP auth: login handshake fails with the wrong password")
{
	AuthDatabaseDummy auth_db;
	const std::string name = "Eve";
	const std::string real_password = "evepass";
	const std::string wrong_password = "not-the-password";

	std::string verifier, salt;
	generate_srp_verifier_and_salt(name, real_password, &verifier, &salt);
	std::string enc_pwd = encode_srp_verifier(verifier, salt);
	AuthEntry new_entry;
	new_entry.name = name;
	new_entry.password = enc_pwd;
	REQUIRE(auth_db.createAuth(new_entry));

	// Client with the WRONG password
	SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(), lowercase(name).c_str(),
		(const unsigned char *) wrong_password.c_str(), wrong_password.size(),
		nullptr, nullptr);
	REQUIRE(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	SRP_Result r = srp_user_start_authentication(usr, nullptr, nullptr, 0,
		&bytes_A, &len_A);
	REQUIRE((int)r == (int)SRP_OK);

	// Server using the stored (correct) verifier
	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	REQUIRE(ver);

	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) salt.c_str(), salt.size(),
		bytes_B, len_B, &bytes_M, &len_M);
	REQUIRE((bytes_M != nullptr && len_M > 0));

	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	REQUIRE(bytes_HAMK == nullptr);
	REQUIRE(!srp_verifier_is_authenticated(ver));

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}

TEST_CASE("SRP auth: session key matches between client and server")
{
	// Sanity check: the session key derived by both parties is the
	// same and non-empty for any password (we just want to make sure
	// the matching logic is symmetric across the SRP code).
	const std::string name = "Frank";
	const std::string password = "frankpass";

	std::string verifier, salt;
	generate_srp_verifier_and_salt(name, password, &verifier, &salt);

	SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(), lowercase(name).c_str(),
		(const unsigned char *) password.c_str(), password.size(),
		nullptr, nullptr);
	REQUIRE(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	REQUIRE((int)srp_user_start_authentication(usr, nullptr, nullptr, 0, &bytes_A, &len_A) == (int)SRP_OK);

	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	REQUIRE(ver);

	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) salt.c_str(), salt.size(),
		bytes_B, len_B, &bytes_M, &len_M);
	REQUIRE(bytes_M);

	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	REQUIRE(bytes_HAMK);
	srp_user_verify_session(usr, bytes_HAMK);

	size_t k1 = 0, k2 = 0;
	const unsigned char *sk1 = srp_user_get_session_key(usr, &k1);
	const unsigned char *sk2 = srp_verifier_get_session_key(ver, &k2);
	REQUIRE(k1 == k2);
	REQUIRE(k1 > 0);
	REQUIRE(std::memcmp(sk1, sk2, k1) == 0);

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}
