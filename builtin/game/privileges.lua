local S = core.get_translator("__builtin")

--
-- Privileges
--

core.registered_privileges = {}

function core.register_privilege(name, param)
	local function fill_defaults(def)
		if def.give_to_singleplayer == nil then
			def.give_to_singleplayer = true
		end
		if def.give_to_admin == nil then
			def.give_to_admin = def.give_to_singleplayer
		end
		if def.description == nil then
			def.description = S("(no description)")
		end
	end
	local def
	if type(param) == "table" then
		def = param
	else
		def = {description = param}
	end
	fill_defaults(def)
	core.registered_privileges[name] = def
end

core.register_privilege("interact", S("Can interact with things and modify the world"))
core.register_privilege("shout", S("Can speak in chat"))

local basic_privs =
	core.string_to_privs((core.settings:get("basic_privs") or "shout,interact"))
local basic_privs_desc = S("Can modify basic privileges (@1)",
	core.privs_to_string(basic_privs, ', '))
core.register_privilege("basic_privs", basic_privs_desc)

core.register_privilege("privs", S("Can modify privileges"))

core.register_privilege("teleport", {
	description = S("Can teleport self"),
	give_to_singleplayer = false,
})
core.register_privilege("bring", {
	description = S("Can teleport other players"),
	give_to_singleplayer = false,
})
core.register_privilege("settime", {
	description = S("Can set the time of day using /time"),
	give_to_singleplayer = false,
})
core.register_privilege("server", {
	description = S("Can do server maintenance stuff"),
	give_to_singleplayer = false,
	give_to_admin = true,
})
core.register_privilege("protection_bypass", {
	description = S("Can bypass node protection in the world"),
	give_to_singleplayer = false,
})
core.register_privilege("ban", {
	description = S("Can ban and unban players"),
	give_to_singleplayer = false,
	give_to_admin = true,
})
core.register_privilege("kick", {
	description = S("Can kick players"),
	give_to_singleplayer = false,
	give_to_admin = true,
})
core.register_privilege("give", {
	description = S("Can use /give and /giveme"),
	give_to_singleplayer = false,
})
core.register_privilege("password", {
	description = S("Can use /setpassword and /clearpassword"),
	give_to_singleplayer = false,
	give_to_admin = true,
})
core.register_privilege("fly", {
	description = S("Can use fly mode"),
	give_to_singleplayer = false,
})
core.register_privilege("fast", {
	description = S("Can use fast mode"),
	give_to_singleplayer = false,
})
core.register_privilege("noclip", {
	description = S("Can fly through solid nodes using noclip mode"),
	give_to_singleplayer = false,
})
core.register_privilege("rollback", {
	description = S("Can use the rollback functionality"),
	give_to_singleplayer = false,
})
core.register_privilege("debug", {
	description = S("Can enable wireframe"),
	give_to_singleplayer = false,
})

core.register_can_bypass_userlimit(function(name, ip)
	local privs = core.get_player_privs(name)
	return privs["server"] or privs["ban"] or privs["privs"] or privs["password"]
end)
