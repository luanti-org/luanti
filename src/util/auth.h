// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015, 2016 est31 <MTest31@outlook.com>

#pragma once

/// Gets the base64 encoded legacy password db entry.
std::string translate_password(const std::string &name,
	const std::string &password);

/// Creates a verification key with given salt and password.
std::string generate_srp_verifier(const std::string &name,
	const std::string &password, const std::string &salt);

/// Creates a verification key and salt with given password.
void generate_srp_verifier_and_salt(const std::string &name,
	const std::string &password, std::string *verifier,
	std::string *salt);

/// Gets an SRP verifier, generating a salt,
/// and encodes it as DB-ready string.
std::string get_encoded_srp_verifier(const std::string &name,
	const std::string &password);

/// Converts the passed SRP verifier into a DB-ready format.
std::string encode_srp_verifier(const std::string &verifier,
	const std::string &salt);

/// Reads the DB-formatted SRP verifier and gets the verifier
/// and salt components.
bool decode_srp_verifier_and_salt(const std::string &encoded,
	std::string *verifier, std::string *salt);
