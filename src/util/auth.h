// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015, 2016 est31 <MTest31@outlook.com>

#pragma once

/// Gets the base64 encoded legacy password db entry.
std::string translate_password(const std::string &name,
	const std::string &password);

/// Creates a verification key with given salt and password.
std::string generate_srp_verifier(std::string_view name,
	std::string_view password, std::string_view salt);

/// Creates a verification key and salt with given password.
void generate_srp_verifier_and_salt(std::string_view name,
	std::string_view password, std::string *verifier,
	std::string *salt);

/// Gets an SRP verifier, generating a salt,
/// and encodes it as DB-ready string.
std::string get_encoded_srp_verifier(std::string_view name,
	std::string_view password);

/// Converts the passed SRP verifier into a DB-ready format.
std::string encode_srp_verifier(std::string_view verifier,
	std::string_view salt);

/// Reads the DB-formatted SRP verifier and gets the verifier
/// and salt components.
bool decode_srp_verifier_and_salt(const std::string &encoded,
	std::string *verifier, std::string *salt);
