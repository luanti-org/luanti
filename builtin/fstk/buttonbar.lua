-- Luanti
-- Copyright (C) 2014 sapier
-- Copyright (C) 2023 Gregor Parzefall
-- SPDX-License-Identifier: LGPL-2.1-or-later


local BASE_SPACING = 0.1
local function get_scroll_btn_width()
	return core.settings:get_bool("touch_gui") and 0.8 or 0.5
end

local function buttonbar_formspec(self)
	if self.hidden then
		return ""
	end

	local formspec = {
		string.format("box[%f,%f;%f,%f;%s]", self.pos.x, self.pos.y, self.size.x,
				self.size.y, self.bgcolor),
	}

	local btn_size = self.size.y - 2*BASE_SPACING

	-- Spacing works like CSS Flexbox with `justify-content: space-evenly;`.
	-- `BASE_SPACING` is used as the minimum spacing, like `gap` in CSS Flexbox.

	-- The number of buttons per page is always calculated as if the scroll
	-- buttons were visible.
	local avail_space = self.size.x - 2*BASE_SPACING - 2*get_scroll_btn_width()
	local btns_per_page = math.floor((avail_space - BASE_SPACING) / (btn_size + BASE_SPACING))

	self.num_pages = math.ceil(#self.buttons / btns_per_page)
	self.cur_page = math.min(self.cur_page, self.num_pages)
	local first_btn = (self.cur_page - 1) * btns_per_page + 1

	local show_scroll_btns = self.num_pages > 1

	-- In contrast, the button spacing calculation takes hidden scroll buttons
	-- into account.
	local real_avail_space = show_scroll_btns and avail_space or self.size.x
	local btn_spacing = (real_avail_space - btns_per_page * btn_size) / (btns_per_page + 1)

	local btn_start_x = self.pos.x + btn_spacing
	if show_scroll_btns then
		btn_start_x = btn_start_x + BASE_SPACING + get_scroll_btn_width()
	end

	for i = first_btn, first_btn + btns_per_page - 1 do
		local btn = self.buttons[i]
		if btn == nil then
			break
		end

		local btn_pos = {
			x = btn_start_x + (i - first_btn) * (btn_size + btn_spacing),
			y = self.pos.y + BASE_SPACING,
		}

		table.insert(formspec, string.format("image_button[%f,%f;%f,%f;%s;%s;%s;false;false]tooltip[%s;%s]",
				btn_pos.x, btn_pos.y, btn_size, btn_size, btn.image, btn.name,
				btn.caption, btn.name, btn.tooltip))
	end

	if show_scroll_btns then
		local btn_prev_pos = {
			x = self.pos.x + BASE_SPACING,
			y = self.pos.y + BASE_SPACING,
		}
		local btn_next_pos = {
			x = self.pos.x + self.size.x - BASE_SPACING - get_scroll_btn_width(),
			y = self.pos.y + BASE_SPACING,
		}

		table.insert(formspec, string.format("button[%f,%f;%f,%f;%s;<]",
				btn_prev_pos.x, btn_prev_pos.y, get_scroll_btn_width(), btn_size,
				self.btn_prev_name))

		table.insert(formspec, string.format("button[%f,%f;%f,%f;%s;>]",
				btn_next_pos.x, btn_next_pos.y, get_scroll_btn_width(), btn_size,
				self.btn_next_name))
	end

	return table.concat(formspec)
end

local function buttonbar_buttonhandler(self, fields)
	if fields[self.btn_prev_name] then
		if self.cur_page > 1 then
			self.cur_page = self.cur_page - 1
			return true
		elseif self.cur_page == 1 then
			self.cur_page = self.num_pages
			return true
		end
	end

	if fields[self.btn_next_name] then
		if self.cur_page < self.num_pages then
			self.cur_page = self.cur_page + 1
			return true
		elseif self.cur_page == self.num_pages then
			self.cur_page = 1
			return true
		end
	end

	for _, btn in ipairs(self.buttons) do
		if fields[btn.name] then
			return self.userbuttonhandler(fields)
		end
	end
end

local buttonbar_metatable = {
	handle_buttons = buttonbar_buttonhandler,
	handle_events  = function(self, event) end,
	get_formspec   = buttonbar_formspec,

	hide = function(self) self.hidden = true end,
	show = function(self) self.hidden = false end,

	delete = function(self) ui.delete(self) end,

	add_button = function(self, name, caption, image, tooltip)
		if caption == nil then caption = "" end
		if image == nil then image = "" end
		if tooltip == nil then tooltip = "" end

		table.insert(self.buttons, {
			name = name,
			caption = caption,
			image = image,
			tooltip = tooltip,
		})
	end,
}

buttonbar_metatable.__index = buttonbar_metatable

function buttonbar_create(name, pos, size, bgcolor, cbf_buttonhandler)
	assert(type(name)              == "string"  )
	assert(type(pos)               == "table"   )
	assert(type(size)              == "table"   )
	assert(type(bgcolor)           == "string"  )
	assert(type(cbf_buttonhandler) == "function")

	local self = {}
	self.type = "addon"
	self.name = name
	self.pos = pos
	self.size = size
	self.bgcolor = bgcolor
	self.userbuttonhandler = cbf_buttonhandler

	self.hidden = false
	self.buttons = {}
	self.num_pages = 1
	self.cur_page = 1

	self.btn_prev_name = "btnbar_prev_" .. self.name
	self.btn_next_name = "btnbar_next_" .. self.name

	setmetatable(self, buttonbar_metatable)

	ui.add(self)
	return self
end
