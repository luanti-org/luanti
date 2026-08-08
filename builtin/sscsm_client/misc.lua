local builtin_shared = ...

function core.check_local_player_privs(...)
	local requested_privs = {...}
	local player_privs = core.get_local_player_privs()

	return builtin_shared.compare_privs(requested_privs, player_privs)
end
