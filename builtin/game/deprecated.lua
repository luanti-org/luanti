--
-- EnvRef
--
core.env = {}
local envref_deprecation_message_printed = false
setmetatable(core.env, {
	__index = function(table, key)
		if not envref_deprecation_message_printed then
			core.log("deprecated", "core.env:[...] is deprecated and should be replaced with core.[...]")
			envref_deprecation_message_printed = true
		end
		local func = core[key]
		if type(func) == "function" then
			rawset(table, key, function(self, ...)
				return func(...)
			end)
		else
			rawset(table, key, nil)
		end
		return rawget(table, key)
	end
})

function core.rollback_get_last_node_actor(pos, range, seconds)
	return core.rollback_get_node_actions(pos, range, seconds, 1)[1]
end

--
-- core.setting_*
--

local settings = core.settings

local function setting_proxy(name)
	return function(...)
		core.log("deprecated", "WARNING: core.setting_* "..
			"functions are deprecated.  "..
			"Use methods on the core.settings object.")
		return settings[name](settings, ...)
	end
end

core.setting_set = setting_proxy("set")
core.setting_get = setting_proxy("get")
core.setting_setbool = setting_proxy("set_bool")
core.setting_getbool = setting_proxy("get_bool")
core.setting_save = setting_proxy("write")

--
-- core.register_on_auth_fail
--

function core.register_on_auth_fail(func)
	core.log("deprecated", "core.register_on_auth_fail " ..
		"is deprecated and should be replaced by " ..
		"core.register_on_authplayer instead.")

	core.register_on_authplayer(function (player_name, ip, is_success)
		if not is_success then
			func(player_name, ip)
		end
	end)
end

-- "Perlin" noise that was value noise
-- deprecated as of 5.12, as it was not Perlin noise
local get_perlin_deprecation_message_printed = false
function core.get_perlin(seed, octaves, persist, spread)
	if not get_perlin_deprecation_message_printed then
		core.log("deprecated", "core.get_perlin is deprecated and was renamed to core.get_value_noise")
		get_perlin_deprecation_message_printed = true
	end
	return core.get_value_noise(seed, octaves, persist, spread)
end
local get_perlin_map_deprecation_message_printed
-- deprecated as of 5.12, as it was not Perlin noise
function core.get_perlin_map(params, size)
	if not get_perlin_map_deprecation_message_printed then
		core.log("deprecated", "core.get_perlin_map is deprecated and was renamed to core.get_value_noise_map")
		get_perlin_map_deprecation_message_printed = true
	end
	return core.get_value_noise_map(params, size)
end
-- deprecated as of 5.12, as it was not Perlin noise
local perlinnoise_deprecation_message_printed = false
function PerlinNoise(noiseparams_or_seed, octaves, persistence, spread)
	if not perlinnoise_deprecation_message_printed then
		core.log("deprecated", "PerlinNoise is deprecated and was renamed to ValueNoise")
		perlinnoise_deprecation_message_printed = true
	end
	return ValueNoise(noiseparams_or_seed, octaves, persistence, spread)
end
-- deprecated as of 5.12, as it was not Perlin noise
local perlinnoisemap_deprecation_message_printed = false
function PerlinNoiseMap(noiseparams, size)
	if not perlinnoisemap_deprecation_message_printed then
		core.log("deprecated", "PerlinNoise is deprecated and was renamed to ValueNoise")
		perlinnoisemap_deprecation_message_printed = true
	end
	return ValueNoiseMap(noiseparams, size)
end
