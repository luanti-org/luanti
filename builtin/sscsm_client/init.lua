local scriptpath = core.get_builtin_path()
local commonpath = scriptpath .. "common" .. DIR_DELIM
local mypath     = scriptpath .. "sscsm_client".. DIR_DELIM

-- Shared between builtin files, but
-- not exposed to outer context
local builtin_shared = {}

-- Populated by SSCSMEventUpdateContentDefs before mods load.
core.registered_content_ids = {}
core.registered_content_names = {}

function core.get_content_id(name)
	return core.registered_content_ids[name]
end
function core.get_name_from_content_id(id)
	return core.registered_content_names[id]
end

dofile(commonpath .. "vector.lua")
dofile(commonpath .. "misc_helpers.lua")
assert(loadfile(commonpath .. "item_s.lua"))(builtin_shared)
assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(mypath .. "register.lua"))(builtin_shared)

-- Channel-object wrapper around the bare clientmodchannel_* C bindings.
-- Server-mod ↔ SSCSM RPC primitive; sends always go to server-mods, never
-- fanned out to other clients. See doc/sscsm_api.md.
do
	local raw_join = core.clientmodchannel_join
	local raw_send = core.clientmodchannel_send
	local raw_leave = core.clientmodchannel_leave

	local ClientModChannel = {}
	ClientModChannel.__index = ClientModChannel

	function ClientModChannel:get_name()
		return self._name
	end
	function ClientModChannel:send(message)
		if self._closed then return false end
		return raw_send(self._name, message)
	end
	function ClientModChannel:leave()
		if self._closed then return end
		self._closed = true
		raw_leave(self._name)
	end

	function core.clientmodchannel_join(name)
		local ok = raw_join(name)
		if not ok then return nil end
		return setmetatable({ _name = name, _closed = false },
				ClientModChannel)
	end

	-- Bare functional access stays available too.
end

dofile(commonpath .. "after.lua")

-- unset, as promised in initializeSecuritySSCSM()
debug.getinfo = nil
