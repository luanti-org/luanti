local builtin_shared = ...

function builtin_shared.compare_privs(requested_privs, player_privs)
	local missing_privileges = {}

	if type(requested_privs[1]) == "table" then
		-- We were provided with a table like { privA = true, privB = true }.
		for priv, value in pairs(requested_privs[1]) do
			if value and not player_privs[priv] then
				missing_privileges[#missing_privileges + 1] = priv
			end
		end
	else
		-- Only a list, we can process it directly.
		for key, priv in pairs(requested_privs) do
			if not player_privs[priv] then
				missing_privileges[#missing_privileges + 1] = priv
			end
		end
	end

	if #missing_privileges > 0 then
		return false, missing_privileges
	end

	return true, ""
end
