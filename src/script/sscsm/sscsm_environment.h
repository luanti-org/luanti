// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "client/client.h"
#include "threading/thread.h"
#include "sscsm_controller.h"
#include "sscsm_irequest.h"
#include "../scripting_sscsm.h"

/** The thread that runs SSCSM code.
 *
 * Meant to be replaced by a sandboxed process.
 *
 * RAII-owns and abstracts away resources to communicate to the main process / thread.
 *
 * See also SSCSMController for other side.
 */
class SSCSMEnvironment : public Thread
{
	std::shared_ptr<StupidChannel> m_channel;
	std::unique_ptr<SSCSMScripting> m_script;
	// the virtual file system.
	// paths look like this:
	// *client_builtin*:subdir/foo.lua
	// *server_builtin*:subdir/foo.lua
	// modname:subdir/foo.lua
	std::unique_ptr<ModVFS> m_vfs;

	void *run() override;

	SerializedSSCSMAnswer exchange(SerializedSSCSMRequest req);

public:
	SSCSMEnvironment(std::shared_ptr<StupidChannel> channel);
	~SSCSMEnvironment() override;

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
