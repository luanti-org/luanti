// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

#pragma once

#include "gamedef.h"
#include "itemdef.h"
#include "nodedef.h"
#include "craftdef.h"
#include "content/mods.h"
#include "database/database-dummy.h"

class DummyGameDef : public IGameDef {
public:
	DummyGameDef():
		m_itemdef(createItemDefManager()),
		m_nodedef(createNodeDefManager()),
		m_craftdef(createCraftDefManager()),
		m_mod_storage_database(new Database_Dummy())
	{
	}

	~DummyGameDef()
	{
		delete m_mod_storage_database;
		delete m_craftdef;
		delete m_nodedef;
		delete m_itemdef;
	}

	bool isClient() override { return false; }
	IItemDefManager *getItemDefManager() override { return m_itemdef; }
	const NodeDefManager *getNodeDefManager() override { return m_nodedef; }
	NodeDefManager* getWritableNodeDefManager() { return m_nodedef; }
	ICraftDefManager *getCraftDefManager() override { return m_craftdef; }

	u16 allocateUnknownNodeId(const std::string &name) override
	{
		return m_nodedef->allocateDummy(name);
	}

	const std::vector<ModSpec> &getMods() const override
	{
		static std::vector<ModSpec> emptymodspec;
		return emptymodspec;
	}
	const ModSpec* getModSpec(const std::string &modname) const override { return nullptr; }
	ModStorageDatabase *getModStorageDatabase() override { return m_mod_storage_database; }

	bool joinModChannel(const std::string &channel) override { return false; }
	bool leaveModChannel(const std::string &channel) override { return false; }
	bool sendModChannelMessage(const std::string &channel, const std::string &message) override
	{
		return false;
	}
	ModChannel *getModChannel(const std::string &channel) override { return nullptr; }

protected:
	IItemDefManager *m_itemdef = nullptr;
	NodeDefManager *m_nodedef = nullptr;
	ICraftDefManager *m_craftdef = nullptr;
	ModStorageDatabase *m_mod_storage_database = nullptr;
};
