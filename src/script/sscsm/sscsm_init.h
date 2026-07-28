// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mod_vfs.h"
#include <string>
#include <vector>
#include <utility>

struct SSCSMInit {
	/// SSCSM files (usually Lua) to be distributed to clients
	ModVFS vfs;
	/// Pairs of modnames and paths to init scripts, run in the given order
	std::vector<std::pair<std::string, std::string>> mod_init_scripts;
};
