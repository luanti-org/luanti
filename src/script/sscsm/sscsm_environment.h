// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include "client/client.h"
#include "threading/thread.h"
#include "sscsm_controller.h"
#include "sscsm_irequest.h"
#include "../scripting_sscsm.h"

// The thread that runs SSCSM code.
// Meant to be replaced by a sandboxed process.
class SSCSMEnvironment : public Thread
{
	std::shared_ptr<StupidChannel> m_channel;
	std::unique_ptr<SSCSMScripting> m_script;
	// virtual file system.
	// TODO: decide and doc how paths look like, maybe:
	// /client_builtin/subdir/foo.lua
	// /server_builtin/subdir/foo.lua
	// /mods/modname/subdir/foo.lua
	std::unordered_map<std::string, std::string> m_vfs;

	void *run() override;

	SerializedSSCSMAnswer exchange(SerializedSSCSMRequest req);

public:
	SSCSMEnvironment(std::shared_ptr<StupidChannel> channel);

	SSCSMScripting *getScript() { return m_script.get(); }

	void updateVFSFiles(std::vector<std::pair<std::string, std::string>> &&files);

	void setFatalError(const std::string &reason);
	void setFatalError(const LuaError &e)
	{
		setFatalError(std::string("Lua: ") + e.what());
	}

	std::unique_ptr<ISSCSMEvent> requestPollNextEvent();
	MapNode requestGetNode(v3s16 pos);
};
