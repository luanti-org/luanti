-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2025 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Context = core.class()

local open_contexts = {}

local OPEN_WINDOW   = 0x00
local REOPEN_WINDOW = 0x01
local UPDATE_WINDOW = 0x02
local CLOSE_WINDOW  = 0x03

function ui.Context:new(builder, player, state)
	self._builder = ui._req(builder, "function")
	self._player = ui._req(player, "string")
	self._state = ui._opt(state, "table", {})

	self._id = nil
	self._window = nil
end

function ui.Context:open(param)
	if self:is_open() then
		return self
	end

	self:_open_window()
	self:_build_window(ui._opt(param, "table", {}))

	local data = ui._encode("BL Z", OPEN_WINDOW, self._id,
			self._window:_encode(self._player, true))

	core.send_ui_message(self._player, data)
	return self
end

function ui.Context:reopen(param)
	if not self:is_open() then
		return self
	end

	local close_id = self:_close_window()
	self:_open_window()
	self:_build_window(ui._opt(param, "table", {}))

	local data = ui._encode("BLL Z", REOPEN_WINDOW, self._id, close_id,
			self._window:_encode(self._player, true))

	core.send_ui_message(self._player, data)
	return self
end

function ui.Context:update(param)
	if not self:is_open() then
		return self
	end

	self:_build_window(ui._opt(param, "table", {}))

	local data = ui._encode("BL Z", UPDATE_WINDOW, self._id,
			self._window:_encode(self._player, false))

	core.send_ui_message(self._player, data)
	return self
end

function ui.Context:close()
	if not self:is_open() then
		return self
	end

	local close_id = self:_close_window()
	local data = ui._encode("BL", CLOSE_WINDOW, close_id)

	core.send_ui_message(self._player, data)
	return self
end

function ui.Context:is_open()
	return self._id ~= nil
end

function ui.Context:get_builder()
	return self._builder
end

function ui.Context:get_player()
	return self._player
end

function ui.Context:get_state()
	return self._state
end

function ui.Context:set_state(state)
	self._state = ui._req(state, "table")
	return self
end

local last_id = 0

function ui.Context:_open_window()
	self._id = last_id
	last_id = last_id + 1

	open_contexts[self._id] = self
end

function ui.Context:_build_window(param)
	self._window = self._builder(self, self._player, self._state, param)

	ui._req(self._window, ui.Window)
	assert(not self._window._context, "Window object has already been used")

	self._window._context = self
end

function ui.Context:_close_window()
	local close_id = self._id

	self._id = nil
	self._window = nil

	open_contexts[close_id] = nil
	return close_id
end

function ui.get_open_contexts()
	local contexts = {}
	for _, context in pairs(open_contexts) do
		table.insert(contexts, context)
	end
	return contexts
end

core.register_on_leaveplayer(function(player)
	for _, context in pairs(open_contexts) do
		if context:get_player() == player:get_player_name() then
			context:_close_window()
		end
	end
end)

local WINDOW_EVENT = 0x00
local ELEM_EVENT   = 0x01

function core.receive_ui_message(player, data)
	local action, id, code, rest = ui._decode("BLB Z", data, -1)

	-- Discard events for any window that isn't currently open, since it's
	-- probably due to network latency and events coming late.
	local context = open_contexts[id]
	if not context then
		core.log("info", "Window " .. id .. " is not open")
		return
	end

	-- If the player doesn't match up with what we expected, ignore the
	-- (probably malicious) event.
	if context:get_player() ~= player then
		core.log("action", "Window " .. id .. " has player '" .. context:get_player() ..
				"', but received event from player '" .. player .. "'")
		return
	end

	-- No events should ever fire for non-GUI windows.
	if context._window._type ~= "gui" then
		core.log("info", "Non-GUI window received event: " .. code)
		return
	end

	-- Prepare the basic event table shared by all events.
	local ev = {
		context = context,
		player = context:get_player(),
		state = context:get_state(),
	}

	if action == WINDOW_EVENT then
		context._window:_on_window_event(code, ev, rest)
	elseif action == ELEM_EVENT then
		context._window:_on_elem_event(code, ev, rest)
	else
		core.log("info", "Invalid window action: " .. action)
	end
end
