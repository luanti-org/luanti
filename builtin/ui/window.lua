-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Window = core.class()

ui._window_types = {
	filter = 0,
	mask = 1,
	hud = 2,
	chat = 3,
	gui = 4,
}

function ui.Window:new(param)
	local function make_window(props)
		self:_init(ui._req(props, "table"))
		return self
	end

	self._type = ui._req_enum(param, ui._window_types)
	return make_window
end

function ui.Window:_init(props)
	self._theme = ui._opt(props.theme, ui.Style, ui.get_default_theme())
	self._styles = ui._opt_array(props.styles, ui.Style, props)

	self._root = ui._req(props.root, ui.Root)

	self._focused = ui._opt(props.focused, "string")
	self._allow_close = ui._opt(props.allow_close, "boolean", true)

	self._on_close = ui._opt(props.on_close, "function")
	self._on_submit = ui._opt(props.on_submit, "function")
	self._on_focus_change = ui._opt(props.on_focus_change, "function")

	self._context = nil -- Set by ui.Context

	self._elems = self._root:_get_flat()
	self._elems_by_id = {}

	for _, elem in ipairs(self._elems) do
		local id = elem._id

		assert(not self._elems_by_id[id], "Element has duplicate ID: '" .. id .. "'")
		self._elems_by_id[id] = elem

		assert(elem._window == nil, "Element '" .. elem._id .. "' already has a window")
		elem._window = self
	end

	if self._focused and self._focused ~= "" then
		assert(self._elems_by_id[self._focused],
				"Invalid focused element: '" .. self._focused .. "'")
	end

	for _, item in ipairs(props) do
		ui._req(item, ui.Style)
	end
end

function ui.Window:_encode(player, opening)
	local enc_styles = self:_encode_styles()
	local enc_elems = self:_encode_elems()

	local fl = ui._make_flags()

	if ui._shift_flag(fl, self._focused) then
		ui._encode_flag(fl, "z", self._focused)
	end
	ui._shift_flag(fl, opening and self._allow_close)

	ui._shift_flag(fl, self._on_submit)
	ui._shift_flag(fl, self._on_focus_change)

	local data = ui._encode("ZzZ", enc_elems, self._root._id, enc_styles)
	if opening then
		data = ui._encode("ZB", data, ui._window_types[self._type])
	end

	return ui._encode("ZZ", data, ui._encode_flags(fl))
end

function ui.Window:_encode_styles()
	-- Clear out all the boxes in every element.
	for _, elem in ipairs(self._elems) do
		for box in pairs(elem._boxes) do
			elem._boxes[box] = {n = 0}
		end
	end

	-- Get a cascaded and flattened list of all the styles for this window.
	local styles = self:_get_full_style():_get_flat()

	-- Take each style and apply its properties to every box and state matched
	-- by its selector.
	self:_apply_styles(styles)

	-- Take the styled boxes and encode their styles into a single table,
	-- replacing the boxes' style property tables with indices into this table.
	local enc_styles = self:_index_styles()

	return ui._encode_array("Z", enc_styles)
end

function ui.Window:_get_full_style()
	-- The full style contains the theme, global styles, and local element
	-- styles as sub-styles, in that order, to ensure the correct precedence.
	local styles = table.merge({self._theme}, self._styles)

	for _, elem in ipairs(self._elems) do
		-- Cascade the inline style with the element's ID, ensuring that the
		-- inline style globally refers to this element only.
		local local_style = ui.Style("#" .. elem._id) {
			props = elem._props,
			nested = elem._styles,
		}
		table.insert(styles, local_style)
	end

	-- Return all these styles wrapped up into a single style.
	return ui.Style {nested = styles}
end

local function apply_style(elem, boxes, style)
	-- Loop through each box, applying the styles accordingly. The table of
	-- boxes may be empty, in which case nothing happens.
	for _, box in pairs(boxes) do
		local name = box.name or "main"

		-- If this style resets all properties, find all states that are a
		-- subset of the state being styled and clear their property tables.
		if style._reset then
			for i = ui._STATE_NONE, ui._NUM_STATES - 1 do
				if bit.band(box.states, i) == box.states then
					elem._boxes[name][i] = nil
				end
			end
		end

		-- Get the existing style property table for this box if it exists.
		local props = elem._boxes[name][box.states] or {}

		-- Cascade the properties from this style onto the box.
		elem._boxes[name][box.states] = ui._cascade_props(style._props, props)
	end
end

function ui.Window:_apply_styles(styles)
	-- Loop through each style and element and see if the style properties can
	-- be applied to any boxes.
	for _, style in ipairs(styles) do
		for _, elem in ipairs(self._elems) do
			-- Check if the selector for this style. If it matches, apply the
			-- style to each of the applicable boxes.
			local matches, boxes = style._sel(elem)
			if matches then
				apply_style(elem, boxes, style)
			end
		end
	end
end

local function index_style(box, i, style_indices, enc_styles)
	-- If we have a style for this state, serialize it to a string. Identical
	-- styles have identical strings, so we use this to our advantage.
	local enc = ui._encode("s", ui._encode_props(box[i]))

	-- If we haven't serialized a style identical to this one before, store
	-- this as the latest index in the list of style strings.
	if not style_indices[enc] then
		style_indices[enc] = #enc_styles
		table.insert(enc_styles, enc)
	end

	-- Set the index of our state to the index of its style string, and keep
	-- count of how many states with valid indices we have for this box so far.
	box[i] = style_indices[enc]
	box.n = box.n + 1
end

function ui.Window:_index_styles()
	local style_indices = {}
	local enc_styles = {}

	for _, elem in ipairs(self._elems) do
		for _, box in pairs(elem._boxes) do
			for i = ui._STATE_NONE, ui._NUM_STATES - 1 do
				if box[i] then
					-- If this box has a style, encode and index it.
					index_style(box, i, style_indices, enc_styles)
				else
					-- Otherwise, this state has no style, so set it as such.
					box[i] = ui._NO_STYLE
				end
			end
		end
	end

	return enc_styles
end

function ui.Window:_encode_elems()
	local enc_elems = {}

	for _, elem in ipairs(self._elems) do
		table.insert(enc_elems, elem:_encode())
	end

	return ui._encode_array("Z", enc_elems)
end

function ui.Window:_on_window_event(code, ev, data)
	-- Get the handler function for this event if we recognize it.
	local handler = self._handlers[code]
	if not handler then
		core.log("info", "Invalid window event: " .. code)
		return
	end

	-- If the event handler returned a callback function for the user, call it
	-- with the event table.
	local callback = handler(self, ev, data)
	if callback then
		callback(ev)
	end
end

function ui.Window:_on_elem_event(code, ev, data)
	local type_id, target, rest = ui._decode("BzZ", data, -1)
	ev.target = target

	-- Get the element for this ID. If it doesn't exist or has a different
	-- type, the window probably updated before receiving this event.
	local elem = self._elems_by_id[target]
	if not elem then
		core.log("info", "Dropped event for non-existent element '" .. target .. "'")
		return
	elseif elem._type_id ~= type_id then
		core.log("info", "Dropped event with type " .. type_id ..
				" sent to element with type " .. elem._type_id)
		return
	end

	-- Pass the event and data to the element for further processing.
	elem:_on_event(code, ev, rest)
end

ui.Window._handlers = {}

ui.Window._handlers[0x00] = function(self, ev, data)
	-- We should never receive an event for an uncloseable window. If we
	-- did, this player might be trying to cheat.
	if not self._allow_close then
		core.log("action", "Player '" .. self._context:get_player() ..
				"' closed uncloseable window")
		return nil
	end

	-- Since the window is now closed, remove the open window data.
	self._context:_close_window()
	return self._on_close
end

ui.Window._handlers[0x01] = function(self, ev, data)
	return self._on_submit
end

ui.Window._handlers[0x02] = function(self, ev, data)
	ev.unfocused, ev.focused = ui._decode("zz", data)

	-- If the ID for either element doesn't exist, we probably updated the
	-- window to remove the element. Assume nothing is focused then.
	if not self._elems_by_id[ev.unfocused] then
		ev.unfocused = ""
	end
	if not self._elems_by_id[ev.focused] then
		ev.focused = ""
	end

	return self._on_focus_change
end
