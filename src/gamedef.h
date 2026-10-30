// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>
#include "irrlichttypes.h"

class IItemDefManager;
class NodeDefManager;
class ICraftDefManager;
class IRollbackManager;
class ModChannel;
class ModStorageDatabase;
struct SubgameSpec;
struct ModSpec;
struct ModIPCStore;

/*
	An interface for fetching game-global definitions like tool and
	mapnode properties
*/
class IGameDef
{
public:
	// These are thread-safe IF they are not edited while running threads.
	// Thus, first they are set up and then they are only read.
	virtual const IItemDefManager* getItemDefManager() const=0;
	virtual const ICraftDefManager* getCraftDefManager() const=0;

	// The node definition manager is NOT thread-safe
	// (due to allocation of new unknown ids)
	virtual const NodeDefManager* getNodeDefManager()=0;

	// Used for keeping track of names/ids of unknown nodes
	virtual u16 allocateUnknownNodeId(const std::string &name)=0;

	// Only usable on the server, and NOT thread-safe. Server thread only.
	virtual IRollbackManager* getRollbackManager() { return nullptr; }
	IRollbackManager *rollback() { return getRollbackManager(); }

	// Only usable on server.
	virtual ModIPCStore *getModIPCStore() { return nullptr; }

	// Shorthands
	const IItemDefManager *idef() const { return getItemDefManager(); }
	const NodeDefManager *ndef() { return getNodeDefManager(); }
	const ICraftDefManager *cdef() const { return getCraftDefManager(); }

	virtual const std::vector<ModSpec> &getMods() const = 0;
	virtual const ModSpec* getModSpec(const std::string &modname) const = 0;
	virtual const SubgameSpec* getGameSpec() const { return nullptr; }
	virtual std::string getWorldPath() const { return ""; }
	virtual std::string getModDataPath() const { return ""; }
	virtual ModStorageDatabase *getModStorageDatabase() = 0;

	virtual bool joinModChannel(const std::string &channel) = 0;
	virtual bool leaveModChannel(const std::string &channel) = 0;
	virtual bool sendModChannelMessage(const std::string &channel,
		const std::string &message) = 0;
	virtual ModChannel *getModChannel(const std::string &channel) = 0;
	virtual bool isClient() = 0;
};
