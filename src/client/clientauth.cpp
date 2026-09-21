// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#include "clientauth.h"
#include "exceptions.h"
#include "util/auth.h"
#include "util/srp.h"
#include "util/string.h"

void SRPUserDeleter::operator()(SRPUser *usr) const
{
	srp_user_delete(usr);
}

ClientAuth::ClientAuth() :
		m_is_empty(true)
{
}

ClientAuth::ClientAuth(const std::string &player_name, const SecureString &password)
{
	applyPassword(player_name, password);
}

ClientAuth::~ClientAuth()
{
	clear();
}

void ClientAuth::applyPassword(const std::string &player_name, const SecureString &password)
{
	clear();
	// AUTH_MECHANISM_FIRST_SRP
	generate_srp_verifier_and_salt(player_name, password, &m_srp_verifier, &m_srp_salt);
	m_is_empty = password.empty();

	std::string player_name_u = lowercase(player_name);
	// AUTH_MECHANISM_SRP
	m_srp_auth_data.reset(srp_user_new(SRP_SHA256, SRP_NG_2048,
			player_name.c_str(), player_name_u.c_str(),
			reinterpret_cast<const unsigned char *>(password.c_str()),
			password.length(), nullptr, nullptr));
	// AUTH_MECHANISM_LEGACY_PASSWORD
	std::string translated = translate_password(player_name, password);
	m_legacy_auth_data.reset(srp_user_new(SRP_SHA256, SRP_NG_2048,
			player_name.c_str(), player_name_u.c_str(),
			reinterpret_cast<const unsigned char *>(translated.c_str()),
			translated.length(), nullptr, nullptr));
}

SRPUser * ClientAuth::getAuthData(AuthMechanism chosen_auth_mech) const
{
	SRPUser *chosen = nullptr;
	switch (chosen_auth_mech) {
		case AUTH_MECHANISM_LEGACY_PASSWORD:
			chosen = m_legacy_auth_data.get();
			break;
		case AUTH_MECHANISM_SRP:
			chosen = m_srp_auth_data.get();
			break;
		case AUTH_MECHANISM_FIRST_SRP:
			return nullptr;
		default:
			throw AuthError("Unknown auth mechanism.");
	}
	if (!chosen)
		throw AuthError("No valid auth data found.");
	return chosen;
}

void ClientAuth::clear()
{
	m_legacy_auth_data.reset();
	m_srp_auth_data.reset();
	m_srp_verifier.clear();
	m_srp_salt.clear();
}

void ClientAuth::clearSessionData()
{
	if (m_legacy_auth_data) {
		srp_user_clear_sessiondata(m_legacy_auth_data.get());
	}
	if (m_srp_auth_data) {
		srp_user_clear_sessiondata(m_srp_auth_data.get());
	}
	// This is need only for first login to server.
	// So, there is no need to keep this for reconnect purposes.
	m_srp_verifier.clear();
	m_srp_salt.clear();
}
