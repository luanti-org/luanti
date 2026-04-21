// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "threading/ipc_channel.h"
#include "sscsm_irequest.h"
#include "../scripting_sscsm.h"

struct ModVFS;

/** Runs SSCSM code in the worker process.
 *
 * Constructed at the entry point of the worker process. The worker's main
 * function creates one of these and calls run() which loops forever,
 * polling the main process for events and dispatching them.
 *
 * Communicates with the main process via an IPC channel over shared memory.
 *
 * See also SSCSMController for the other side.
 */
class SSCSMEnvironment
{
	IPCChannelEnd m_channel;
	std::unique_ptr<SSCSMScripting> m_script;
	// the virtual file system.
	// paths look like this:
	// *client_builtin*:subdir/foo.lua
	// *server_builtin*:subdir/foo.lua
	// modname:subdir/foo.lua
	std::unique_ptr<ModVFS> m_vfs;

	SerializedSSCSMAnswer exchange(SerializedSSCSMRequest req);

public:
	SSCSMEnvironment(IPCChannelEnd channel);
	~SSCSMEnvironment();

	// Main loop: polls events from the main process, dispatches, repeats.
	// Returns when a TearDown event is received or on fatal error.
	void run();

	SSCSMScripting *getScript() { return m_script.get(); }

	ModVFS *getModVFS() { return m_vfs.get(); }
	void updateVFSFiles(std::vector<std::pair<std::string, std::string>> &&files);
	std::optional<std::string_view> readVFSFile(const std::string &path);

	void setFatalError(const std::string &reason);

	template <typename RQ>
	typename RQ::Answer doRequest(RQ &&rq)
	{
		return deserializeSSCSMAnswer<typename RQ::Answer>(
				exchange(serializeSSCSMRequest(std::forward<RQ>(rq)))
			);
	}
};
