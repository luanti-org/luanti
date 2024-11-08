local function window_info_equal(a, b)
	return
		-- size
		a.size.x == b.size.x and a.size.y == b.size.y and
		-- real_gui_scaling, real_hud_scaling
		a.real_gui_scaling == b.real_gui_scaling and
		a.real_hud_scaling == b.real_hud_scaling and
		-- max_formspec_size
		a.max_formspec_size.x == b.max_formspec_size.x and
		a.max_formspec_size.y == b.max_formspec_size.y and
		-- touch_controls
		a.touch_controls == b.touch_controls
end

local last_window_info = {}

local function show_fullscreen_fs(name, window)
	print(dump(window))

	local size = window.max_formspec_size
	local touch_text = window.touch_controls and "Touch controls enabled" or
			"Touch controls disabled"
	local fs = {
		"formspec_version[4]",
		("size[%f,%f]"):format(size.x, size.y),
		"padding[0,0]",
		"bgcolor[;true]",
		("button[%f,%f;1,1;%s;%s]"):format(0, 0, "tl", "TL"),
		("button[%f,%f;1,1;%s;%s]"):format(size.x - 1, 0, "tr", "TR"),
		("button[%f,%f;1,1;%s;%s]"):format(size.x - 1, size.y - 1, "br", "BR"),
		("button[%f,%f;1,1;%s;%s]"):format(0, size.y - 1, "bl", "BL"),

		("label[%f,%f;%s]"):format(size.x / 2, size.y / 2, "Fullscreen"),
		("label[%f,%f;%s]"):format(size.x / 2, size.y / 2 + 1, touch_text),
	}

	core.show_formspec(name, "testfullscreenfs:fs", table.concat(fs))
	core.chat_send_player(name, ("Calculated size of %f, %f"):format(size.x, size.y))
	last_window_info[name] = window
end

core.register_chatcommand("testfullscreenfs", {
	func = function(name)
		local window = core.get_player_window_information(name)
		if not window then
			return false, "Unable to get window info"
		end

		show_fullscreen_fs(name, window)
		return true
	end,
})

core.register_globalstep(function()
	for name, last_window in pairs(last_window_info) do
		local window = core.get_player_window_information(name)
		if window and not window_info_equal(last_window, window) then
			show_fullscreen_fs(name, window)
		end
	end
end)

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname == "testfullscreenfs:fs" and fields.quit then
		last_window_info[player:get_player_name()] = nil
	end
end)

core.register_on_leaveplayer(function(player)
	last_window_info[player:get_player_name()] = nil
end)
