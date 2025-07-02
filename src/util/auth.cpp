// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015, 2016 est31 <MTest31@outlook.com>

#include <algorithm>
#include <string>
#include "auth.h"
#include "base64.h"
#include "util/hashing.h"
#include "srp.h"
#include "util/string.h"
#include "debug.h"

// Get an sha-1 hash of the player's name combined with
// the password entered. That's what the server uses as
// their password. (Exception : if the password field is
// blank, we send a blank password - this is for backwards
// compatibility with password-less players).
std::string translate_password(const std::string &name,
	const std::string &password)
{
	if (password.length() == 0)
		return "";

	std::string slt = name + password;
	std::string digest = hashing::sha1(slt);
	std::string pwd = base64_encode(digest);
	return pwd;
}

// Call lower level SRP code to generate a verifier with the
// given pointers. Contains the preparations, call parameters
// and error checking common to all srp verifier generation code.
// See docs of srp_create_salted_verification_key for more info.
static inline void gen_srp_v(const std::string &name,
	const std::string &password, char **salt, size_t *salt_len,
	char **bytes_v, size_t *len_v)
{
	std::string n_name = lowercase(name);
	SRP_Result res = srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		n_name.c_str(), (const unsigned char *)password.c_str(),
		password.size(), (unsigned char **)salt, salt_len,
		(unsigned char **)bytes_v, len_v, NULL, NULL);
	FATAL_ERROR_IF(res != SRP_OK, "Couldn't create salted SRP verifier");
}

/// Creates a verification key with given salt and password.
std::string generate_srp_verifier(const std::string &name,
	const std::string &password, const std::string &salt)
{
	size_t salt_len = salt.size();
	// The API promises us that the salt doesn't
	// get modified if &salt_ptr isn't NULL.
	char *salt_ptr = (char *)salt.c_str();

	char *bytes_v = nullptr;
	size_t verifier_len = 0;
	gen_srp_v(name, password, &salt_ptr, &salt_len, &bytes_v, &verifier_len);
	std::string verifier = std::string(bytes_v, verifier_len);
	free(bytes_v);
	return verifier;
}

/// Creates a verification key and salt with given password.
void generate_srp_verifier_and_salt(const std::string &name,
	const std::string &password, std::string *verifier,
	std::string *salt)
{
	char *bytes_v = nullptr;
	size_t verifier_len = 0;
	char *salt_ptr = nullptr;
	size_t salt_len = 0;
	gen_srp_v(name, password, &salt_ptr, &salt_len, &bytes_v, &verifier_len);
	*verifier = std::string(bytes_v, verifier_len);
	*salt = std::string(salt_ptr, salt_len);
	free(bytes_v);
	free(salt_ptr);
}

/// Gets an SRP verifier, generating a salt,
/// and encodes it as DB-ready string.
std::string get_encoded_srp_verifier(const std::string &name,
	const std::string &password)
{
	std::string verifier;
	std::string salt;
	generate_srp_verifier_and_salt(name, password, &verifier, &salt);
	return encode_srp_verifier(verifier, salt);
}

/// Converts the passed SRP verifier into a DB-ready format.
std::string encode_srp_verifier(const std::string &verifier,
	const std::string &salt)
{
	std::ostringstream ret_str;
	ret_str << "#1#"
		<< base64_encode(salt) << "#"
		<< base64_encode(verifier);
	return ret_str.str();
}

/// Reads the DB-formatted SRP verifier and gets the verifier
/// and salt components.
bool decode_srp_verifier_and_salt(const std::string &encoded,
	std::string *verifier, std::string *salt)
{
	std::vector<std::string> components = str_split(encoded, '#');

	if ((components.size() != 4)
			|| (components[1] != "1") // 1 means srp
			|| !base64_is_valid(components[2])
			|| !base64_is_valid(components[3]))
		return false;

	*salt = base64_decode(components[2]);
	*verifier = base64_decode(components[3]);
	return true;

}
