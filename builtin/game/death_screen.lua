local F = core.formspec_escape
local S = core.get_translator("__builtin")

function core.show_death_screen(player, _reason)
	local fs = {
		"formspec_version[1]",
		"size[11,5.5,true]",
		"bgcolor[#320000b4;true]",
		"label[4.85,1.35;", F(S("You died")), "]",
		"button_exit[4,3;3,0.5;btn_respawn;", F(S("Respawn")), "]",
	}
	core.show_formspec(player:get_player_name(), "__builtin:death", table.concat(fs, ""))
end

core.register_on_dieplayer(function(player, reason)
	core.show_death_screen(player, reason)
end)

core.register_on_joinplayer(function(player)
	if player:get_hp() == 0 then
		core.show_death_screen(player, nil)
	end
end)

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname == "__builtin:death" and fields.quit and player:get_hp() == 0 then
		player:respawn()
		core.log("action", player:get_player_name() .. " respawns at " ..
				player:get_pos():to_string())
	end
end)
