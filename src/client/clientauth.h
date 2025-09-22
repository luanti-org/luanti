// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#pragma once

#include <string>
#include "network/networkprotocol.h"
#include "util/basic_macros.h"

struct SRPUser;

class ClientAuth
{
public:
	ClientAuth();
	ClientAuth(const std::string &player_name, const std::string &password);

	~ClientAuth();
	DISABLE_CLASS_COPY(ClientAuth);

	ClientAuth(ClientAuth &&other) { *this = std::move(other); }
	ClientAuth &operator=(ClientAuth &&other);

	void applyPassword(const std::string &player_name, const std::string &password);

	bool getIsEmpty() const { return m_is_empty; }
	const std::string &getSrpVerifier() const { return m_srp_verifier; }
	const std::string &getSrpSalt() const { return m_srp_salt; }
	SRPUser * getLegacyAuthData() const { return m_legacy_auth_data; }
	SRPUser * getSrpAuthData() const { return m_srp_auth_data; }
	SRPUser * getAuthData(AuthMechanism chosen_auth_mech) const;

	void clear();
	void clearSessionData();
private:
	bool m_is_empty;

	std::string m_srp_verifier;
	std::string m_srp_salt;

	SRPUser *m_legacy_auth_data = nullptr;
	SRPUser *m_srp_auth_data = nullptr;
};
