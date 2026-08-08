local builtin_shared = ...

function core.setting_get_pos(name)
	local value = core.settings:get(name)
	if not value then
		return nil
	end
	return core.string_to_pos(value)
end


-- old non-method sound functions

function core.sound_stop(handle, ...)
	return handle:stop(...)
end

function core.sound_fade(handle, ...)
	return handle:fade(...)
end

function core.check_local_player_privs(...)
	local requested_privs = {...}
	local player_privs = core.get_privilege_list()

	return builtin_shared.compare_privs(requested_privs, player_privs)
end
