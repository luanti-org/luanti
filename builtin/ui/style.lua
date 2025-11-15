-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Style = core.class()

function ui.Style:new(param)
	local function make_style(props)
		self:_init(ui._req(props, "table"))
		return self
	end

	if type(param) == "string" then
		self._sel = ui._parse_sel(param, false, false)
		return make_style
	end

	self._sel = ui._universal_sel
	return make_style(param)
end

function ui.Style:_init(props)
	self._props = ui._cascade_props(props.props or props, {})
	self._nested = table.merge(ui._opt_array(props.nested, ui.Style, props))
	self._reset = ui._opt(props.reset, "boolean")

	for _, item in ipairs(props) do
		ui._req(item, ui.Style)
	end
end

function ui.Style:_get_flat()
	local flat_styles = {}
	self:_get_flat_impl(flat_styles, ui._universal_sel)
	return flat_styles
end

function ui.Style:_get_flat_impl(flat_styles, parent_sel)
	-- Intersect our selector with our parent selector, resulting in a fully
	-- qualified selector.
	local full_sel = ui._intersect_sels({parent_sel, self._sel})

	-- Copy this style's properties into a new style with the full selector.
	local flat = ui.Style {
		props = self._props,
		reset = self._reset,
	}
	flat._sel = full_sel

	table.insert(flat_styles, flat)

	-- For each sub-style of this style, cascade it with our full selector and
	-- add it to the list of flat styles.
	for _, nested in ipairs(self._nested) do
		nested:_get_flat_impl(flat_styles, full_sel)
	end
end

local layout_type_map = {
	place = 0,
}

local dir_flags_map = {
	none = 0,
	x = 1,
	y = 2,
	both = 3,
}

local clip_mode_map = {
	normal = 0,
	overflow = 1,
	clipped = 2,
}

local obj_fit_map = {
	fill = 0,
	contain = 1,
	cover = 2,
	fixed = 3,
}

local text_align_map = {
	left = 0,
	center = 1,
	right = 2,
}

local text_valign_map = {
	top = 0,
	center = 1,
	bottom = 2,
}

local function opt_color(val, def)
	assert(val == nil or core.colorspec_to_int(val))
	return val or def
end

local function opt_vec2d(val, def)
	ui._opt_array(val, "number")
	assert(val == nil or #val == 1 or #val == 2)
	return val or def
end

local function opt_rect(val, def)
	ui._opt_array(val, "number")
	assert(val == nil or #val == 1 or #val == 2 or #val == 4)
	return val or def
end

local function cascade_layout(new, add, props)
	new.layout = ui._opt_enum(add.layout, layout_type_map, props.layout)
	new.truncate = ui._opt_enum(add.truncate, dir_flags_map, props.truncate)

	new.scale = ui._opt(add.scale, "number", props.scale)
end

local function cascade_sizing(new, add, props)
	new.min = opt_vec2d(add.min, props.min)

	new.margin = opt_rect(add.margin, props.margin)
	new.padding = opt_rect(add.padding, props.padding)

	new.pos = opt_vec2d(add.pos, props.pos)
	new.size = opt_vec2d(add.size, props.size)
	new.anchor = opt_vec2d(add.anchor, props.anchor)
end

local function cascade_visual(new, add, props)
	new.clip = ui._opt_enum(add.clip, clip_mode_map, props.clip)
	new.hidden = ui._opt(add.hidden, "boolean", props.hidden)

	new.fill = opt_color(add.fill, props.fill)
end

local function cascade_image(new, add, props)
	new.img_pane = ui._opt(add.img_pane, "string", props.img_pane)
	new.img_overlay = ui._opt(add.img_overlay, "string", props.img_overlay)

	new.img_tint = opt_color(add.img_tint, props.img_tint)
	new.img_slice = opt_rect(add.img_slice, props.img_slice)

	new.img_frames = ui._opt(add.img_frames, "number", props.img_frames)
	new.img_frame_time = ui._opt(add.img_frame_time, "number", props.img_frame_time)

	new.img_border = opt_rect(add.img_border, props.img_border)
	new.img_tile = ui._opt_enum(add.img_tile, dir_flags_map, props.img_tile)

	new.img_align = opt_vec2d(add.img_align, props.img_align)
	new.img_scale = ui._opt(add.img_scale, "number", props.img_scale)
end

local function cascade_text(new, add, props)
	new.prepend = ui._opt(add.prepend, "string", props.prepend)
	new.append = ui._opt(add.append, "string", props.append)

	new.text_color = opt_color(add.text_color, props.text_color)
	new.text_mark = opt_color(add.text_mark, props.text_mark)
	new.text_size = ui._opt(add.text_size, "number", props.text_size)

	new.text_mono = ui._opt(add.text_mono, "boolean", props.text_mono)
	new.text_italic = ui._opt(add.text_italic, "boolean", props.text_italic)
	new.text_bold = ui._opt(add.text_bold, "boolean", props.text_bold)

	new.text_align = ui._opt_enum(add.text_align, text_align_map, props.text_align)
	new.text_valign = ui._opt_enum(add.text_valign, text_valign_map, props.text_valign)
end

function ui._cascade_props(add, props)
	local new = {}

	cascade_layout(new, add, props)
	cascade_sizing(new, add, props)
	cascade_visual(new, add, props)
	cascade_image(new, add, props)
	cascade_text(new, add, props)

	return new
end

local function unpack_vec2d(vec)
	if #vec == 2 then
		return vec[1], vec[2]
	elseif #vec == 1 then
		return vec[1], vec[1]
	end
end

local function unpack_rect(rect)
	if #rect == 4 then
		return rect[1], rect[2], rect[3], rect[4]
	elseif #rect == 2 then
		return rect[1], rect[2], rect[1], rect[2]
	elseif #rect == 1 then
		return rect[1], rect[1], rect[1], rect[1]
	end
end

local function encode_layout(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.layout) then
		ui._encode_flag(fl, "B", layout_type_map[props.layout])
	end
	if ui._shift_flag(fl, props.truncate) then
		ui._encode_flag(fl, "B", dir_flags_map[props.truncate])
	end

	if ui._shift_flag(fl, props.scale) then
		ui._encode_flag(fl, "f", props.scale)
	end

	return fl
end

local function encode_sizing(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.min) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.min))
	end

	if ui._shift_flag(fl, props.margin) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.margin))
	end
	if ui._shift_flag(fl, props.padding) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.padding))
	end

	if ui._shift_flag(fl, props.pos) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.pos))
	end
	if ui._shift_flag(fl, props.size) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.size))
	end
	if ui._shift_flag(fl, props.anchor) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.anchor))
	end

	return fl
end

local function encode_visual(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.clip) then
		ui._encode_flag(fl, "B", clip_mode_map[props.clip])
	end
	ui._shift_flag_bool(fl, props.hidden)

	if ui._shift_flag(fl, props.fill) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props.fill))
	end

	return fl
end

local function encode_image(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.img_pane) then
		ui._encode_flag(fl, "z", props.img_pane)
	end
	if ui._shift_flag(fl, props.img_overlay) then
		ui._encode_flag(fl, "z", props.img_overlay)
	end

	if ui._shift_flag(fl, props.img_tint) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props.img_tint))
	end
	if ui._shift_flag(fl, props.img_slice) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.img_slice))
	end

	if ui._shift_flag(fl, props.img_frames) then
		ui._encode_flag(fl, "I", props.img_frames)
	end
	if ui._shift_flag(fl, props.img_frame_time) then
		ui._encode_flag(fl, "I", props.img_frame_time)
	end

	if ui._shift_flag(fl, props.img_border) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.img_border))
	end
	if ui._shift_flag(fl, props.img_tile) then
		ui._encode_flag(fl, "B", dir_flags_map[props.img_tile])
	end

	if ui._shift_flag(fl, props.img_align) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.img_align))
	end
	if ui._shift_flag(fl, props.img_scale) then
		ui._encode_flag(fl, "f", props.img_scale)
	end

	return fl
end

local function encode_text(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.prepend) then
		ui._encode_flag(fl, "s", props.prepend)
	end
	if ui._shift_flag(fl, props.append) then
		ui._encode_flag(fl, "s", props.append)
	end

	if ui._shift_flag(fl, props.text_color) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props.text_color))
	end
	if ui._shift_flag(fl, props.text_mark) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props.text_mark))
	end
	if ui._shift_flag(fl, props.text_size) then
		ui._encode_flag(fl, "I", props.text_size)
	end

	ui._shift_flag_bool(fl, props.text_mono)
	ui._shift_flag_bool(fl, props.text_italic)
	ui._shift_flag_bool(fl, props.text_bold)

	if ui._shift_flag(fl, props.text_align) then
		ui._encode_flag(fl, "B", text_align_map[props.text_align])
	end
	if ui._shift_flag(fl, props.text_valign) then
		ui._encode_flag(fl, "B", text_valign_map[props.text_valign])
	end

	return fl
end

local function encode_subprops(fl)
	return fl.flags ~= 0 and ui._encode_flags(fl) or ""
end

function ui._encode_props(props)
	return ui._encode("sssss",
		encode_subprops(encode_layout(props)),
		encode_subprops(encode_sizing(props)),
		encode_subprops(encode_visual(props)),
		encode_subprops(encode_image(props)),
		encode_subprops(encode_text(props)))
end
