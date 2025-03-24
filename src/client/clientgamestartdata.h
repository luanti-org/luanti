/*
Minetest
Copyright (C) 2024 SFENCE, <sfence.software@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "gameparams.h"
#include "clientauth.h"

// Information processed by main menu
struct ClientGameStartData : GameParams
{
	ClientGameStartData(const GameStartData &start_data):
		GameParams(start_data),
		name(start_data.name),
		address(start_data.address),
		local_server(start_data.local_server),
		allow_login_or_register(start_data.allow_login_or_register),
		world_spec(start_data.world_spec)
	{
	}

	bool isSinglePlayer() const { return address.empty() && !local_server; }

	std::string name;
	ClientAuth auth;
	std::string address;
	bool local_server;

	ELoginRegister allow_login_or_register = ELoginRegister::Any;

	// "world_path" must be kept in sync!
	WorldSpec world_spec;
};
