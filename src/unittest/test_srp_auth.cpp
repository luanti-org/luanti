// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <cstring>
#include <string>
#include <vector>

#include "database/database-dummy.h"
#include "util/auth.h"
#include "util/srp.h"
#include "util/string.h"
#include "filesys.h"

namespace
{

class TestSrpAuth : public TestBase
{
public:
	TestSrpAuth() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSrpAuth"; }

	void runTests(IGameDef *gamedef);
	void runTestsForCurrentDB();

	void testVerifierEncodeDecodeRoundtrip();
	void testDbStoresAndRecallsEncodedVerifier();
	void testFirstSrpRegistration();
	void testSrpLoginSuccess();
	void testSrpLoginWrongPassword();
	void testSrpSessionKeyMatches();

private:
	AuthDatabase *auth_db = nullptr;
};

static TestSrpAuth g_test_instance;

void TestSrpAuth::runTests(IGameDef *gamedef)
{
	const std::string test_dir = getTestTempDirectory();

	rawstream << "-------- Dummy auth database + SRP" << std::endl;
	auth_db = new AuthDatabaseDummy();
	runTestsForCurrentDB();
	delete auth_db;
	auth_db = nullptr;
}

void TestSrpAuth::runTestsForCurrentDB()
{
	TEST(testVerifierEncodeDecodeRoundtrip);
	TEST(testDbStoresAndRecallsEncodedVerifier);
	TEST(testFirstSrpRegistration);
	TEST(testSrpLoginSuccess);
	TEST(testSrpLoginWrongPassword);
	TEST(testSrpSessionKeyMatches);
}

////////////////////////////////////////////////////////////////////////////////

void TestSrpAuth::testVerifierEncodeDecodeRoundtrip()
{
	// Use the same low-level helper as the network code so we know
	// the wire format we are validating (auth.cpp:gen_srp_v).
	std::string verifier, salt;
	generate_srp_verifier_and_salt("Alice", "CorrectHorseBatteryStaple", &verifier, &salt);

	// Sanity: the raw verifier and salt must be non-empty
	UASSERT(!verifier.empty());
	UASSERT(!salt.empty());

	// encode_srp_verifier / decode_srp_verifier_and_salt must roundtrip
	std::string encoded = encode_srp_verifier(verifier, salt);
	UASSERT(!encoded.empty());

	std::string decoded_verifier, decoded_salt;
	UASSERT(decode_srp_verifier_and_salt(encoded, &decoded_verifier, &decoded_salt));
	UASSERTEQ(std::string, decoded_verifier, verifier);
	UASSERTEQ(std::string, decoded_salt, salt);

	// A malformed blob should be rejected
	UASSERT(!decode_srp_verifier_and_salt("not a valid blob", &decoded_verifier, &decoded_salt));
	// Wrong version tag should be rejected
	std::string bad_version = "#2#" + std::string("a") + "#" + std::string("a");
	UASSERT(!decode_srp_verifier_and_salt(bad_version, &decoded_verifier, &decoded_salt));
}

void TestSrpAuth::testDbStoresAndRecallsEncodedVerifier()
{
	const std::string name = "Bob";
	const std::string encoded = get_encoded_srp_verifier(name, "s3cret");

	AuthEntry entry;
	entry.name = name;
	entry.password = encoded;
	entry.last_login = 1234;
	UASSERT(auth_db->createAuth(entry));

	AuthEntry loaded;
	UASSERT(auth_db->getAuth(name, loaded));
	UASSERTEQ(std::string, loaded.name, name);
	UASSERTEQ(std::string, loaded.password, encoded);

	// Update the password (simulating a sudo password change)
	loaded.password = get_encoded_srp_verifier(name, "new-pass");
	loaded.last_login = 5678;
	UASSERT(auth_db->saveAuth(loaded));

	AuthEntry reloaded;
	UASSERT(auth_db->getAuth(name, reloaded));
	UASSERTEQ(std::string, reloaded.password, loaded.password);
	UASSERTEQ(u64, reloaded.last_login, 5678);

	UASSERT(auth_db->deleteAuth(name));
	UASSERT(!auth_db->getAuth(name, reloaded));
}

void TestSrpAuth::testFirstSrpRegistration()
{
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
	UASSERT(auth_db->createAuth(entry));

	// Server-side reload must produce a verifier that the same
	// client can use to log in with the same password.
	AuthEntry loaded;
	UASSERT(auth_db->getAuth(name, loaded));

	std::string db_verifier, db_salt;
	UASSERT(decode_srp_verifier_and_salt(loaded.password, &db_verifier, &db_salt));
	UASSERTEQ(std::string, db_verifier, verifier);
	UASSERTEQ(std::string, db_salt, salt);
}

void TestSrpAuth::testSrpLoginSuccess()
{
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
	UASSERT(auth_db->createAuth(new_entry));

	// --- Client side ---
	std::string client_name = name;
	SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		client_name.c_str(), lowercase(client_name).c_str(),
		(const unsigned char *) password.c_str(), password.size(),
		nullptr, nullptr);
	UASSERT(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	SRP_Result r = srp_user_start_authentication(usr, nullptr, nullptr, 0,
		&bytes_A, &len_A);
	UASSERTEQ(int, r, (int)SRP_OK);
	UASSERT(bytes_A != nullptr && len_A > 0);

	// --- Server side ---
	AuthEntry entry;
	UASSERT(auth_db->getAuth(name, entry));
	std::string db_verifier, db_salt;
	UASSERT(decode_srp_verifier_and_salt(entry.password, &db_verifier, &db_salt));

	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		client_name.c_str(),
		(const unsigned char *) db_salt.c_str(), db_salt.size(),
		(const unsigned char *) db_verifier.c_str(), db_verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	UASSERT(ver);
	UASSERT(bytes_B != nullptr && len_B > 0);

	// --- Client side: process challenge ---
	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) db_salt.c_str(), db_salt.size(),
		bytes_B, len_B,
		&bytes_M, &len_M);
	UASSERT(bytes_M != nullptr && len_M > 0);

	// --- Server side: verify M, produce HAMK ---
	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	UASSERT(bytes_HAMK != nullptr);
	UASSERT(srp_verifier_is_authenticated(ver));

	// --- Client side: verify HAMK matches ---
	srp_user_verify_session(usr, bytes_HAMK);
	UASSERT(srp_user_is_authenticated(usr));

	// Session keys must match between client and server.
	size_t server_key_len = 0, client_key_len = 0;
	const unsigned char *server_key = srp_verifier_get_session_key(ver, &server_key_len);
	const unsigned char *client_key = srp_user_get_session_key(usr, &client_key_len);
	UASSERTEQ(size_t, client_key_len, server_key_len);
	UASSERT(server_key_len > 0);
	UASSERT(std::memcmp(server_key, client_key, server_key_len) == 0);

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}

void TestSrpAuth::testSrpLoginWrongPassword()
{
	const std::string name = "Eve";
	const std::string real_password = "evepass";
	const std::string wrong_password = "not-the-password";

	std::string verifier, salt;
	generate_srp_verifier_and_salt(name, real_password, &verifier, &salt);
	std::string enc_pwd = encode_srp_verifier(verifier, salt);
	AuthEntry new_entry;
	new_entry.name = name;
	new_entry.password = enc_pwd;
	UASSERT(auth_db->createAuth(new_entry));

	// Client with the WRONG password
	SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(), lowercase(name).c_str(),
		(const unsigned char *) wrong_password.c_str(), wrong_password.size(),
		nullptr, nullptr);
	UASSERT(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	SRP_Result r = srp_user_start_authentication(usr, nullptr, nullptr, 0,
		&bytes_A, &len_A);
	UASSERTEQ(int, r, (int)SRP_OK);

	// Server using the stored (correct) verifier
	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	UASSERT(ver);

	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) salt.c_str(), salt.size(),
		bytes_B, len_B, &bytes_M, &len_M);
	UASSERT(bytes_M != nullptr && len_M > 0);

	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	UASSERT(bytes_HAMK == nullptr);
	UASSERT(!srp_verifier_is_authenticated(ver));

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}

void TestSrpAuth::testSrpSessionKeyMatches()
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
	UASSERT(usr);

	unsigned char *bytes_A = nullptr;
	size_t len_A = 0;
	UASSERTEQ(int, (int)srp_user_start_authentication(usr, nullptr, nullptr, 0, &bytes_A, &len_A), (int)SRP_OK);

	unsigned char *bytes_B = nullptr;
	size_t len_B = 0;
	SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		name.c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		bytes_A, len_A, nullptr, 0,
		&bytes_B, &len_B, nullptr, nullptr);
	UASSERT(ver);

	unsigned char *bytes_M = nullptr;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		(const unsigned char *) salt.c_str(), salt.size(),
		bytes_B, len_B, &bytes_M, &len_M);
	UASSERT(bytes_M);

	unsigned char *bytes_HAMK = nullptr;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	UASSERT(bytes_HAMK);
	srp_user_verify_session(usr, bytes_HAMK);

	size_t k1 = 0, k2 = 0;
	const unsigned char *sk1 = srp_user_get_session_key(usr, &k1);
	const unsigned char *sk2 = srp_verifier_get_session_key(ver, &k2);
	UASSERTEQ(size_t, k1, k2);
	UASSERT(k1 > 0);
	UASSERT(std::memcmp(sk1, sk2, k1) == 0);

	// All buffers returned by the SRP API are owned by the user/verifier
	// and will be released by srp_user_delete / srp_verifier_delete.
	srp_user_delete(usr);
	srp_verifier_delete(ver);
}

} // anonymous namespace
