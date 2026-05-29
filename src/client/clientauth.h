// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#pragma once

#include <memory>
#include <string>
#include "network/networkprotocol.h"
#include "util/basic_macros.h"
#include "util/secure_string.h"

struct SRPUser;

struct SRPUserDeleter {
	void operator()(SRPUser *usr) const;
};

using SRPUserPtr = std::unique_ptr<SRPUser, SRPUserDeleter>;

class ClientAuth
{
public:
	ClientAuth();
	ClientAuth(const std::string &player_name, const SecureString &password);

	ClientAuth(ClientAuth &&) = default;
	ClientAuth &operator=(ClientAuth &&) = default;

	~ClientAuth();

	void applyPassword(const std::string &player_name, const SecureString &password);

	bool getIsEmpty() const { return m_is_empty; }
	const std::string &getSrpVerifier() const { return m_srp_verifier; }
	const std::string &getSrpSalt() const { return m_srp_salt; }
	SRPUser * getLegacyAuthData() const { return m_legacy_auth_data.get(); }
	SRPUser * getSrpAuthData() const { return m_srp_auth_data.get(); }
	SRPUser * getAuthData(AuthMechanism chosen_auth_mech) const;

	void clear();
	void clearSessionData();
private:
	bool m_is_empty;

	std::string m_srp_verifier;
	std::string m_srp_salt;

	SRPUserPtr m_legacy_auth_data;
	SRPUserPtr m_srp_auth_data;
};
